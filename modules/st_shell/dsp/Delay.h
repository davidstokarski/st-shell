#pragma once

#include <algorithm>
#include <vector>

namespace st::dsp
{

/** Multichannel integer delay line, used for dry-path latency compensation.
    prepare() allocates; everything else is real-time safe.
    setDelay() may be called from the audio thread at any time; the change is
    instantaneous (no interpolation) — latency changes are expected to coincide with
    a host latency change, where a discontinuity is unavoidable anyway. */
class DelayLine
{
public:
    void prepare (int numChannels, int maxDelaySamples)
    {
        numCh = std::max (1, numChannels);
        maxDelay = std::max (0, maxDelaySamples);
        size = 1;
        while (size < maxDelay + 1)
            size <<= 1;
        mask = size - 1;
        data.assign ((size_t) (numCh * size), 0.0f);
        writePos = 0;
        delay = std::min (delay, maxDelay);
    }

    void reset() noexcept
    {
        std::fill (data.begin(), data.end(), 0.0f);
        writePos = 0;
    }

    /** Clamped to [0, maxDelaySamples]. */
    void setDelay (int samples) noexcept { delay = std::clamp (samples, 0, maxDelay); }
    int getDelay() const noexcept        { return delay; }
    int getMaxDelay() const noexcept     { return maxDelay; }

    /** Processes all channels in place (channels beyond prepare()'s count are untouched). */
    void process (juce::AudioBuffer<float>& buffer, int numSamples) noexcept
    {
        if (data.empty())
            return;
        const int chans = std::min (numCh, buffer.getNumChannels());
        for (int ch = 0; ch < chans; ++ch)
            processChannel (ch, buffer.getWritePointer (ch), numSamples);
        writePos = (writePos + numSamples) & mask;
    }

    /** Processes one channel; call for every channel, then advance(numSamples) once. */
    void processChannel (int channel, float* samples, int numSamples) noexcept
    {
        float* line = data.data() + (size_t) channel * (size_t) size;
        int w = writePos;
        if (delay == 0)
        {
            for (int i = 0; i < numSamples; ++i, w = (w + 1) & mask)
                line[w] = samples[i];
            return;
        }
        for (int i = 0; i < numSamples; ++i, w = (w + 1) & mask)
        {
            line[w] = samples[i];
            samples[i] = line[(w - delay) & mask];
        }
    }

    void advance (int numSamples) noexcept { writePos = (writePos + numSamples) & mask; }

private:
    std::vector<float> data;
    int numCh = 1, maxDelay = 0, size = 1, mask = 0, writePos = 0, delay = 0;
};

} // namespace st::dsp
