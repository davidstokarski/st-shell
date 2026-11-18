#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>

#include "Utils.h"

/*  Filters — general-purpose IIR building blocks.

    Conventions shared by every class in this file:
      - Float audio I/O, DOUBLE internal state and coefficients. This keeps the filters
        clean at very low cutoff / very high sample rate (e.g. 20 Hz @ 768 kHz) at the
        same scalar cost as float.
      - Up to kMaxChannels (8) channels of state held inline: prepare() never allocates,
        so every method is real-time safe.
      - Denormals: block-level process() methods install juce::ScopedNoDenormals
        themselves. Per-sample processSample() methods do NOT — call them from inside a
        scope that has juce::ScopedNoDenormals (JUCE's processBlock wrapper usually does,
        or add one at the top of your processBlock).
      - Static response helpers (getResponse / getMagnitude) are pure functions and may
        be called from any thread (UI drawing).
*/

namespace st::dsp
{

inline constexpr int kMaxFilterChannels = 8;

//==============================================================================
// Biquad (RBJ cookbook, transposed direct form II)
//==============================================================================
enum class BiquadType { lowPass, highPass, bandPass, notch, peak, lowShelf, highShelf, allPass };

struct BiquadCoeffs
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0; // a0 normalised to 1

    /** RBJ design. bandPass = constant 0 dB peak gain. For shelves, q is the RBJ "Q"
        (0.7071 = slope S of 1). gainDb is used by peak and shelves only. */
    static BiquadCoeffs design (BiquadType type, double sampleRate, double freq, double q, double gainDb = 0.0) noexcept;

    std::complex<double> response (double hz, double sampleRate) const noexcept;
    double magnitude (double hz, double sampleRate) const noexcept { return std::abs (response (hz, sampleRate)); }
};

class Biquad
{
public:
    static constexpr int kMaxChannels = kMaxFilterChannels;

    void prepare (int numChannels) noexcept { numCh = std::clamp (numChannels, 1, kMaxChannels); reset(); }
    void reset() noexcept                   { s1.fill (0.0); s2.fill (0.0); }

    void setCoefficients (const BiquadCoeffs& c) noexcept { coeffs = c; }
    void setParameters (BiquadType type, double sampleRate, double freq, double q, double gainDb = 0.0) noexcept
    {
        coeffs = BiquadCoeffs::design (type, sampleRate, freq, q, gainDb);
    }
    const BiquadCoeffs& getCoefficients() const noexcept { return coeffs; }

    float processSample (int channel, float xf) noexcept
    {
        const double x = xf;
        double& z1 = s1[(size_t) channel];
        double& z2 = s2[(size_t) channel];
        const double y = coeffs.b0 * x + z1;
        z1 = coeffs.b1 * x - coeffs.a1 * y + z2;
        z2 = coeffs.b2 * x - coeffs.a2 * y;
        return (float) y;
    }

    void process (float* data, int numSamples, int channel) noexcept;
    void process (juce::AudioBuffer<float>& buffer, int numSamples) noexcept;

    /** Complex response of an RBJ design at evalHz (for UI drawing; any thread). */
    static std::complex<double> getResponse (BiquadType type, double sampleRate, double freq, double q,
                                             double gainDb, double evalHz) noexcept
    {
        return BiquadCoeffs::design (type, sampleRate, freq, q, gainDb).response (evalHz, sampleRate);
    }
    static double getMagnitude (BiquadType type, double sampleRate, double freq, double q,
                                double gainDb, double evalHz) noexcept
    {
        return std::abs (getResponse (type, sampleRate, freq, q, gainDb, evalHz));
    }

private:
    BiquadCoeffs coeffs;
    std::array<double, kMaxChannels> s1 {}, s2 {};
    int numCh = 2;
};

//==============================================================================
// Svf (Cytomic / Simper TPT state-variable filter)
//==============================================================================
enum class SvfMode { lowPass, bandPass, highPass, notch, peak, allPass, bell, lowShelf, highShelf };

/** Coefficients for Simper's SVF in "mix" form: y = m0*v0 + m1*v1 + m2*v2, where
    v1 = band (s/(s^2+ks+1)), v2 = low (1/(s^2+ks+1)). bandPass is the 0 dB-peak variant. */
struct SvfCoeffs
{
    double g = 0.0, k = 1.4142135623730951, a1 = 1.0, a2 = 0.0, a3 = 0.0;
    double m0 = 0.0, m1 = 0.0, m2 = 1.0;

    static SvfCoeffs design (SvfMode mode, double sampleRate, double freq, double q, double gainDb = 0.0) noexcept;
    std::complex<double> response (double hz, double sampleRate) const noexcept;
};

/** Stable under per-sample modulation of cutoff, Q and gain (TPT structure keeps the
    state meaningful when coefficients change). setParameters() costs one tan(). */
class Svf
{
public:
    static constexpr int kMaxChannels = kMaxFilterChannels;

    struct Outputs { float low, band, high; }; // band = unity-peak band-pass

    void prepare (double sampleRate, int numChannels) noexcept
    {
        fs = sampleRate > 0.0 ? sampleRate : 44100.0;
        numCh = std::clamp (numChannels, 1, kMaxChannels);
        update();
        reset();
    }
    void reset() noexcept { ic1.fill (0.0); ic2.fill (0.0); }

    void setParameters (SvfMode newMode, double cutoffHz, double q, double gainDb = 0.0) noexcept
    {
        mode = newMode; cutoff = cutoffHz; reso = q; gain = gainDb;
        update();
    }
    void setMode (SvfMode m) noexcept       { mode = m; update(); }
    void setCutoff (double hz) noexcept     { cutoff = hz; update(); }
    void setQ (double q) noexcept           { reso = q; update(); }
    void setGainDb (double db) noexcept     { gain = db; update(); }
    const SvfCoeffs& getCoefficients() const noexcept { return c; }

    /** Output of the selected mode. */
    float processSample (int channel, float x) noexcept
    {
        double v1, v2;
        tick (channel, x, v1, v2);
        return (float) (c.m0 * x + c.m1 * v1 + c.m2 * v2);
    }

    /** Raw low / band / high outputs (ignores the mode's mix; shelves/bell use their warped g). */
    Outputs processSampleAll (int channel, float x) noexcept
    {
        double v1, v2;
        tick (channel, x, v1, v2);
        return { (float) v2, (float) (c.k * v1), (float) (x - c.k * v1 - v2) };
    }

    void process (float* data, int numSamples, int channel) noexcept;
    void process (juce::AudioBuffer<float>& buffer, int numSamples) noexcept;

    static std::complex<double> getResponse (SvfMode mode, double sampleRate, double freq, double q,
                                             double gainDb, double evalHz) noexcept
    {
        return SvfCoeffs::design (mode, sampleRate, freq, q, gainDb).response (evalHz, sampleRate);
    }
    static double getMagnitude (SvfMode mode, double sampleRate, double freq, double q,
                                double gainDb, double evalHz) noexcept
    {
        return std::abs (getResponse (mode, sampleRate, freq, q, gainDb, evalHz));
    }

private:
    void update() noexcept { c = SvfCoeffs::design (mode, fs, cutoff, reso, gain); }

    void tick (int channel, double v0, double& v1, double& v2) noexcept
    {
        double& s1 = ic1[(size_t) channel];
        double& s2 = ic2[(size_t) channel];
        const double v3 = v0 - s2;
        v1 = c.a1 * s1 + c.a2 * v3;
        v2 = s2 + c.a2 * s1 + c.a3 * v3;
        s1 = 2.0 * v1 - s1;
        s2 = 2.0 * v2 - s2;
    }

    SvfCoeffs c;
    std::array<double, kMaxChannels> ic1 {}, ic2 {};
    double fs = 44100.0, cutoff = 1000.0, reso = 0.7071067811865476, gain = 0.0;
    SvfMode mode = SvfMode::lowPass;
    int numCh = 2;
};

//==============================================================================
// OnePole (TPT, LP / HP) and DcBlocker
//==============================================================================
enum class OnePoleType { lowPass, highPass, allPass };

class OnePole
{
public:
    static constexpr int kMaxChannels = kMaxFilterChannels;

    void prepare (double sampleRate, int numChannels) noexcept
    {
        fs = sampleRate > 0.0 ? sampleRate : 44100.0;
        numCh = std::clamp (numChannels, 1, kMaxChannels);
        setCutoff (cutoff);
        reset();
    }
    void reset() noexcept { state.fill (0.0); }

    void setType (OnePoleType t) noexcept { type = t; }
    void setCutoff (double hz) noexcept
    {
        cutoff = hz;
        const double g = std::tan (kPi * std::clamp (hz, 0.01, 0.49 * fs) / fs);
        G = g / (1.0 + g);
    }

    float processSample (int channel, float x) noexcept
    {
        double& s = state[(size_t) channel];
        const double v = (x - s) * G;
        const double lp = v + s;
        s = lp + v;
        switch (type)
        {
            case OnePoleType::lowPass:  return (float) lp;
            case OnePoleType::highPass: return (float) (x - lp);
            case OnePoleType::allPass:  return (float) (2.0 * lp - x);
        }
        return (float) lp;
    }

    void process (float* data, int numSamples, int channel) noexcept;
    void process (juce::AudioBuffer<float>& buffer, int numSamples) noexcept;

    static std::complex<double> getResponse (OnePoleType type, double sampleRate, double cutoffHz, double evalHz) noexcept;

private:
    std::array<double, kMaxChannels> state {};
    double fs = 44100.0, cutoff = 1000.0, G = 0.0;
    OnePoleType type = OnePoleType::lowPass;
    int numCh = 2;
};

/** First-order DC blocking high-pass (default 5 Hz), double state. */
class DcBlocker
{
public:
    void prepare (double sampleRate, int numChannels, double cutoffHz = 5.0) noexcept
    {
        filter.setType (OnePoleType::highPass);
        filter.prepare (sampleRate, numChannels);
        filter.setCutoff (cutoffHz);
    }
    void reset() noexcept                              { filter.reset(); }
    void setCutoff (double hz) noexcept                { filter.setCutoff (hz); }
    float processSample (int channel, float x) noexcept { return filter.processSample (channel, x); }
    void process (juce::AudioBuffer<float>& buffer, int numSamples) noexcept { filter.process (buffer, numSamples); }

private:
    OnePole filter;
};

//==============================================================================
// TiltFilter ("Tone")
//==============================================================================
/** First-order tilt EQ: H(s) = G*HP(s) + LP(s)/G with the one-pole at pivot*G, giving
    exactly 0 dB at the pivot, -maxGain dB at DC and +maxGain dB at Nyquist-ish for
    tilt=+1 (mirror for tilt=-1). Default pivot 1 kHz, maxGain 9 dB.

    Modulation:
      - setTilt() from the audio thread (per block): internally ramped (default 20 ms)
        and coefficients are recomputed per sample while ramping -> zipper free.
      - process (buffer, n, tiltPerSample): per-sample absolute tilt values (e.g. from a
        modulation buffer). TPT one-pole is safe for audio-rate coefficient changes. */
class TiltFilter
{
public:
    static constexpr int kMaxChannels = kMaxFilterChannels;

    void prepare (double sampleRate, int numChannels) noexcept
    {
        fs = sampleRate > 0.0 ? sampleRate : 44100.0;
        numCh = std::clamp (numChannels, 1, kMaxChannels);
        smoother.prepare (fs, rampMs);
        smoother.setCurrentAndTarget (smoother.getTarget());
        updateCoeffs (smoother.getCurrent());
        reset();
    }
    void reset() noexcept { state.fill (0.0); }

    void setTilt (float tilt) noexcept             { smoother.setTarget (std::clamp (tilt, -1.0f, 1.0f)); }
    void setTiltImmediate (float tilt) noexcept
    {
        smoother.setCurrentAndTarget (std::clamp (tilt, -1.0f, 1.0f));
        updateCoeffs (smoother.getCurrent());
    }
    void setPivot (double hz) noexcept             { pivot = hz; updateCoeffs (smoother.getCurrent()); }
    void setMaxGainDb (float db) noexcept          { maxGainDb = db; updateCoeffs (smoother.getCurrent()); }
    void setRampMs (double ms) noexcept            { rampMs = ms; smoother.prepare (fs, rampMs); }
    float getTilt() const noexcept                 { return smoother.getTarget(); }

    /** Per-sample, using the current coefficients (no smoothing step). */
    float processSample (int channel, float x) noexcept
    {
        double& s = state[(size_t) channel];
        const double v = (x - s) * G;
        const double lp = v + s;
        s = lp + v;
        return (float) (gainHi * (x - lp) + gainLo * lp);
    }

    void process (juce::AudioBuffer<float>& buffer, int numSamples) noexcept;
    void process (juce::AudioBuffer<float>& buffer, int numSamples, const float* tiltPerSample) noexcept;

    static std::complex<double> getResponse (double sampleRate, double pivotHz, float tilt, float maxGainDb, double evalHz) noexcept;
    double getMagnitude (double evalHz) const noexcept
    {
        return std::abs (getResponse (fs, pivot, smoother.getTarget(), maxGainDb, evalHz));
    }

private:
    void updateCoeffs (float tilt) noexcept;

    LinearSmoother smoother;
    std::array<double, kMaxChannels> state {};
    double fs = 44100.0, pivot = 1000.0, rampMs = 20.0;
    double G = 0.0, gainHi = 1.0, gainLo = 1.0;
    float maxGainDb = 9.0f, lastTilt = 1.0e9f;
    int numCh = 2;
};

} // namespace st::dsp
