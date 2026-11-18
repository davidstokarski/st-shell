#pragma once

#include <array>
#include <atomic>

#include "Utils.h"

namespace st::dsp
{

/** OTT-style combined upward + downward compressor (feed-forward), one instance per band.

    Signal flow per sample and channel:
      detector d = lerp(|x|, rms(x), rmsBlend)       (RMS = one-pole mean square, rmsWindowMs)
      stereo link: d = lerp(d, max_over_channels(d), stereoLink)
      ballistics on d (linear domain, branching attack / release)
      gainDb = computeStaticGainDb (dB(env), params)  — downward + upward, times depth
      y = x * dB->gain(gainDb)
    Because the gain is a static function of the smoothed level, the steady-state gain
    equals computeStaticGainDb() exactly (attack/release apply to level rises / falls
    for both directions, so upward boost recovers with "release" like OTT).

    Static curve (W = knee):
      down: soft-knee (Giannoulis) reduction above downThreshold with slope (1/R - 1).
      up:   mirror image below upThreshold with slope (1 - 1/R), capped at maxUpwardGainDb
            and faded out smoothly (smoothstep) between upwardFloorDb + upwardFloorFadeDb
            and upwardFloorDb, so silence / noise floor is not pumped up.
      total = (down + up) * depth     (depth < 0 turns compression into expansion).
    Ratios >= 100 are treated as infinity.

    Threading: prepare() on the message thread; setParams()/process()/reset() on the
    audio thread (setParams is cheap: it only retargets smoothers). Meter getters and
    computeStaticGainDb() are safe from any thread. process() installs ScopedNoDenormals. */
class UpDownCompressor
{
public:
    static constexpr int kMaxChannels = 8;
    static constexpr float kInfiniteRatio = 100.0f;

    struct Params
    {
        float downThresholdDb   = -12.0f;
        float downRatio         = 4.0f;    // 1..inf (>= 100 -> inf)
        float upThresholdDb     = -36.0f;
        float upRatio           = 4.0f;    // 1..inf (>= 100 -> inf)
        float attackMs          = 10.0f;
        float releaseMs         = 120.0f;
        float kneeDb            = 6.0f;
        float maxUpwardGainDb   = 24.0f;
        float depth             = 1.0f;    // -1..+1, scales the dB gain change
        float stereoLink        = 1.0f;    // 0..1
        float rmsBlend          = 0.0f;    // 0 = peak, 1 = RMS
        float rmsWindowMs       = 10.0f;
        float upwardFloorDb     = -80.0f;  // no upward gain below this
        float upwardFloorFadeDb = 12.0f;   // fade-in range above the floor
    };

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset() noexcept;

    void setParams (const Params& p) noexcept;
    const Params& getParams() const noexcept { return params; }

    /** Processes `audio` in place. If `detector` is given, the level is detected on it
        (channel i, or its last channel if it has fewer channels). Audio thread only. */
    void process (juce::AudioBuffer<float>& audio, int numSamples, const juce::AudioBuffer<float>* detector = nullptr) noexcept;

    /** Static transfer: gain change (dB, signed) for a detector level inputDb. Any thread. */
    static float computeStaticGainDb (float inputDb, const Params& p) noexcept;

    /** Meters, updated once per block: signed gain change (+ upward, - downward) of the
        channel with the largest |gain change|, and the max detector level over channels. */
    float getGainChangeDb() const noexcept    { return meterGainDb.load (std::memory_order_relaxed); }
    float getDetectorLevelDb() const noexcept { return meterLevelDb.load (std::memory_order_relaxed); }

    std::atomic<float> meterGainDb { 0.0f };
    std::atomic<float> meterLevelDb { kMinusInfDb };

private:
    struct Curve
    {
        float downT, downSlope, upT, upSlope, knee, maxUp, floorDb, fadeDb, depth;
    };
    static float curveGainDb (float x, const Curve& c) noexcept;
    Curve currentCurve() const noexcept;
    void updateBallistics() noexcept;

    enum { sDownT, sDownSlope, sUpT, sUpSlope, sKnee, sMaxUp, sDepth, sLink, sBlend, kNumSmoothers };

    Params params;
    std::array<LinearSmoother, kNumSmoothers> smoothers;
    double fs = 48000.0;
    int numCh = 2;
    float attCoeff = 1.0f, relCoeff = 1.0f, rmsCoeff = 1.0f;
    std::array<float, kMaxChannels> env {}, meanSq {};
    bool firstBlock = true;
};

} // namespace st::dsp
