#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace st::dsp
{

/** Attack/release envelope follower (linear domain), per channel, up to kMaxChannels.
    Detector d = lerp(|x|, sqrt(meanSquare), rmsBlend), where meanSquare is a one-pole
    average of x^2 over rmsWindowMs (default 10 ms); the envelope then follows d with
    branching attack / release (same semantics as the compressor's detector).
    All methods except prepare() are real-time safe. Denormals are flushed internally,
    so no ScopedNoDenormals is required at call sites. */
class EnvelopeFollower
{
public:
    static constexpr int kMaxChannels = 8;

    void prepare (double sampleRate, int numChannels) noexcept
    {
        fs = sampleRate > 0.0 ? sampleRate : 44100.0;
        numCh = std::clamp (numChannels, 1, kMaxChannels);
        updateCoeffs();
        reset();
    }

    void reset() noexcept
    {
        env.fill (0.0f);
        msEnv.fill (0.0f);
    }

    void setAttackMs (float ms) noexcept  { attackMs = std::max (0.0f, ms); updateCoeffs(); }
    void setReleaseMs (float ms) noexcept { releaseMs = std::max (0.0f, ms); updateCoeffs(); }
    /** 0 = peak, 1 = RMS, values in between blend the two envelopes. */
    void setRmsBlend (float blend) noexcept { rmsBlend = std::clamp (blend, 0.0f, 1.0f); }
    void setRmsWindowMs (float ms) noexcept { rmsWindowMs = std::max (0.1f, ms); updateCoeffs(); }

    /** Per-sample. Returns the envelope (linear). */
    float processSample (int channel, float x) noexcept
    {
        float& m = msEnv[(size_t) channel];
        m += rmsCoeff * (x * x - m);
        if (m < 1.0e-30f) m = 0.0f;

        const float a = std::abs (x);
        const float d = rmsBlend > 0.0f ? a + rmsBlend * (std::sqrt (m) - a) : a;

        float& e = env[(size_t) channel];
        e += (d > e ? attCoeff : relCoeff) * (d - e);
        if (e < 1.0e-15f) e = 0.0f;
        return e;
    }

    /** Per-block on one channel: runs every sample, returns the envelope at block end. */
    float processBlock (int channel, const float* samples, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
            processSample (channel, samples[i]);
        return getEnvelope (channel);
    }

    /** Per-block, optionally writing the per-sample envelope into envOut (may be nullptr). */
    void processBlock (int channel, const float* samples, float* envOut, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float e = processSample (channel, samples[i]);
            if (envOut != nullptr)
                envOut[i] = e;
        }
    }

    /** All channels of a buffer; envelope values are read with getEnvelope(). */
    void process (const juce::AudioBuffer<float>& buffer, int numSamples) noexcept
    {
        const int chans = std::min (numCh, buffer.getNumChannels());
        for (int ch = 0; ch < chans; ++ch)
            processBlock (ch, buffer.getReadPointer (ch), numSamples);
    }

    float getEnvelope (int channel) const noexcept { return env[(size_t) channel]; }

    /** Max envelope over channels. */
    float getMaxEnvelope() const noexcept
    {
        float m = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
            m = std::max (m, getEnvelope (ch));
        return m;
    }

private:
    void updateCoeffs() noexcept
    {
        attCoeff = attackMs  <= 0.0f ? 1.0f : (float) (1.0 - std::exp (-1.0 / (attackMs * 0.001 * fs)));
        relCoeff = releaseMs <= 0.0f ? 1.0f : (float) (1.0 - std::exp (-1.0 / (releaseMs * 0.001 * fs)));
        rmsCoeff = (float) (1.0 - std::exp (-1.0 / (rmsWindowMs * 0.001 * fs)));
    }

    double fs = 44100.0;
    int numCh = 2;
    float attackMs = 10.0f, releaseMs = 100.0f, rmsBlend = 0.0f, rmsWindowMs = 10.0f;
    float attCoeff = 1.0f, relCoeff = 1.0f, rmsCoeff = 1.0f;
    std::array<float, kMaxChannels> env {}, msEnv {};
};

} // namespace st::dsp
