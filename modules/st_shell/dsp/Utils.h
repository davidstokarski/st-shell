#pragma once

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <utility>

namespace st::dsp
{

//==============================================================================
// Constants
//==============================================================================
inline constexpr double kPi    = 3.14159265358979323846;
inline constexpr double kTwoPi = 6.28318530717958647692;

/** Lowest level (dB) returned by gainToDb() for zero / tiny gains. */
inline constexpr float kMinusInfDb = -120.0f;

//==============================================================================
// dB <-> gain
//==============================================================================
/** dB -> linear gain. Uses exp(), which is a single fast intrinsic on all targets. */
inline float dbToGain (float db) noexcept
{
    return std::exp (db * 0.11512925464970228f); // ln(10)/20
}

/** Linear gain -> dB, floored at floorDb (default -120 dB). */
inline float gainToDb (float gain, float floorDb = kMinusInfDb) noexcept
{
    return gain > 0.0f ? std::max (floorDb, 8.685889638065035f * std::log (gain)) : floorDb; // 20/ln(10)
}

/** Power (mean-square) -> dB, floored at floorDb. Avoids a sqrt for RMS detectors. */
inline float powerToDb (float power, float floorDb = kMinusInfDb) noexcept
{
    return power > 0.0f ? std::max (floorDb, 4.3429448190325175f * std::log (power)) : floorDb; // 10/ln(10)
}

/** Fast 2^x (relative error < 1e-6, i.e. well below 0.001 dB). x is clamped to [-126, 126]. */
inline float fastExp2 (float x) noexcept
{
    x = std::clamp (x, -126.0f, 126.0f);
    const float xi = std::nearbyint (x);
    const float f  = x - xi;  // [-0.5, 0.5]
    // Taylor series of 2^f to 6th order.
    const float p = 1.0f + f * (0.693147181f + f * (0.240226507f + f * (0.0555041087f
                    + f * (0.00961812911f + f * (0.00133335581f + f * 0.000154035304f)))));
    const auto bits = (std::uint32_t) ((int) xi + 127) << 23;
    return p * std::bit_cast<float> (bits);
}

/** Fast log2(x) for x > 0 (abs error ~1e-7). Returns -126 for x <= 0 / denormals. */
inline float fastLog2 (float x) noexcept
{
    if (! (x >= 1.1754944e-38f))
        return -126.0f;
    const auto bits = std::bit_cast<std::uint32_t> (x);
    int e = (int) ((bits >> 23) & 255u) - 127;
    float m = std::bit_cast<float> ((bits & 0x007FFFFFu) | 0x3F800000u); // [1, 2)
    if (m > 1.41421356f) { m *= 0.5f; ++e; }                              // [0.707, 1.414)
    const float u = (m - 1.0f) / (m + 1.0f);                              // |u| < 0.1716
    const float u2 = u * u;
    // ln(m) = 2 atanh(u) = 2(u + u^3/3 + u^5/5 + u^7/7)
    const float lnm = 2.0f * u * (1.0f + u2 * (0.333333333f + u2 * (0.2f + u2 * 0.142857143f)));
    return (float) e + lnm * 1.44269504f;
}

/** Fast dB -> gain (relative error < 1e-6). */
inline float fastDbToGain (float db) noexcept { return fastExp2 (db * 0.166096404744f); } // log2(10)/20

/** Fast gain -> dB, floored at floorDb. */
inline float fastGainToDb (float gain, float floorDb = kMinusInfDb) noexcept
{
    return gain > 0.0f ? std::max (floorDb, 6.02059991f * fastLog2 (gain)) : floorDb; // 20 log10(2)
}

//==============================================================================
// Misc numerics
//==============================================================================
/** Flushes values that would decay into the denormal range to zero. */
inline float flushDenormal (float x) noexcept   { return std::abs (x) < 1.0e-15f ? 0.0f : x; }
inline double flushDenormal (double x) noexcept { return std::abs (x) < 1.0e-30 ? 0.0 : x; }

/** One-pole smoothing coefficient so that the step response reaches 1-1/e after timeMs. */
inline float onePoleCoeff (double timeMs, double sampleRate) noexcept
{
    if (timeMs <= 0.0 || sampleRate <= 0.0)
        return 1.0f;
    return (float) (1.0 - std::exp (-1.0 / (timeMs * 0.001 * sampleRate)));
}

//==============================================================================
// Pan / mid-side
//==============================================================================
/** Equal-power (-3 dB centre) pan law. pan in [-1 (L), +1 (R)]. Returns {gainL, gainR}. */
inline std::pair<float, float> equalPowerPan (float pan) noexcept
{
    const float angle = (std::clamp (pan, -1.0f, 1.0f) + 1.0f) * 0.25f * (float) kPi; // 0..pi/2
    return { std::cos (angle), std::sin (angle) };
}

/** M = (L+R)/2, S = (L-R)/2 (decode is the exact inverse). */
inline void encodeMidSide (float l, float r, float& m, float& s) noexcept
{
    m = 0.5f * (l + r);
    s = 0.5f * (l - r);
}

inline void decodeMidSide (float m, float s, float& l, float& r) noexcept
{
    l = m + s;
    r = m - s;
}

/** In-place L/R -> M/S on two channel pointers. */
inline void encodeMidSide (float* left, float* right, int numSamples) noexcept
{
    for (int i = 0; i < numSamples; ++i)
    {
        const float l = left[i], r = right[i];
        left[i]  = 0.5f * (l + r);
        right[i] = 0.5f * (l - r);
    }
}

/** In-place M/S -> L/R on two channel pointers. */
inline void decodeMidSide (float* mid, float* side, int numSamples) noexcept
{
    for (int i = 0; i < numSamples; ++i)
    {
        const float m = mid[i], s = side[i];
        mid[i]  = m + s;
        side[i] = m - s;
    }
}

/** In-place M/S encode of channels 0/1 of a buffer (no-op for < 2 channels). */
inline void encodeMidSide (juce::AudioBuffer<float>& buffer, int numSamples) noexcept
{
    if (buffer.getNumChannels() >= 2)
        encodeMidSide (buffer.getWritePointer (0), buffer.getWritePointer (1), numSamples);
}

inline void decodeMidSide (juce::AudioBuffer<float>& buffer, int numSamples) noexcept
{
    if (buffer.getNumChannels() >= 2)
        decodeMidSide (buffer.getWritePointer (0), buffer.getWritePointer (1), numSamples);
}

//==============================================================================
// Sanitising
//==============================================================================
/** Checks a buffer for NaN/Inf. If any are found and zeroNonFinite is true, the
    offending samples are set to 0. Returns true if the buffer was entirely finite.
    Real-time safe. */
inline bool isFiniteBuffer (juce::AudioBuffer<float>& buffer, int numSamples, bool zeroNonFinite = true) noexcept
{
    bool allFinite = true;
    numSamples = std::min (numSamples, buffer.getNumSamples());

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            if (! std::isfinite (d[i]))
            {
                allFinite = false;
                if (zeroNonFinite)
                    d[i] = 0.0f;
                else
                    return false;
            }
        }
    }
    return allFinite;
}

/** Read-only check. */
inline bool isFiniteBuffer (const juce::AudioBuffer<float>& buffer, int numSamples) noexcept
{
    numSamples = std::min (numSamples, buffer.getNumSamples());
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const auto* d = buffer.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
            if (! std::isfinite (d[i]))
                return false;
    }
    return true;
}

//==============================================================================
// LinearSmoother
//==============================================================================
/** Linear ramp smoother for one scalar value. "Multichannel-friendly": a single
    instance is advanced once per sample frame (getNext()) and the value applied to
    every channel, or rendered once into a scratch buffer with fillRamp() and reused
    per channel. Real-time safe, no allocation. */
class LinearSmoother
{
public:
    LinearSmoother() = default;
    explicit LinearSmoother (float initial) noexcept : current (initial), target (initial) {}

    /** Sets sample rate and ramp time; snaps to the current target (call from prepare()). */
    void prepare (double sampleRate, double rampMs) noexcept
    {
        rampLength = std::max (1, (int) std::round (sampleRate * rampMs * 0.001));
        setCurrentAndTarget (target);
    }

    void setRampLength (int samples) noexcept { rampLength = std::max (1, samples); }

    void setCurrentAndTarget (float v) noexcept
    {
        current = target = v;
        step = 0.0f;
        remaining = 0;
    }

    /** Starts a new ramp from the current value if the target changed. */
    void setTarget (float v) noexcept
    {
        if (juce::exactlyEqual (v, target))
            return;
        target = v;
        remaining = rampLength;
        step = (target - current) / (float) rampLength;
    }

    float getNext() noexcept
    {
        if (remaining <= 0)
            return current;
        if (--remaining == 0)
            current = target;
        else
            current += step;
        return current;
    }

    /** Advances numSamples and returns the new current value. */
    float skip (int numSamples) noexcept
    {
        if (remaining <= 0)
            return current;
        if (numSamples >= remaining)
        {
            remaining = 0;
            current = target;
        }
        else
        {
            remaining -= numSamples;
            current += step * (float) numSamples;
        }
        return current;
    }

    /** Writes the next numSamples values into dest (advances the smoother). */
    void fillRamp (float* dest, int numSamples) noexcept
    {
        if (remaining <= 0)
        {
            std::fill (dest, dest + numSamples, current);
            return;
        }
        for (int i = 0; i < numSamples; ++i)
            dest[i] = getNext();
    }

    /** Multiplies every channel by the ramp (advances the smoother once per frame). */
    void applyGain (juce::AudioBuffer<float>& buffer, int numSamples) noexcept
    {
        const int numCh = buffer.getNumChannels();
        if (remaining <= 0)
        {
            if (! juce::exactlyEqual (current, 1.0f))
                for (int ch = 0; ch < numCh; ++ch)
                    juce::FloatVectorOperations::multiply (buffer.getWritePointer (ch), current, numSamples);
            return;
        }
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = getNext();
            for (int ch = 0; ch < numCh; ++ch)
                buffer.getWritePointer (ch)[i] *= g;
        }
    }

    bool isSmoothing() const noexcept { return remaining > 0; }
    float getCurrent() const noexcept { return current; }
    float getTarget() const noexcept  { return target; }

private:
    float current = 0.0f, target = 0.0f, step = 0.0f;
    int remaining = 0, rampLength = 1;
};

} // namespace st::dsp
