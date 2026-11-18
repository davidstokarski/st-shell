#pragma once

#include <array>
#include <atomic>
#include <complex>
#include <vector>

#include "Utils.h"

namespace st::dsp
{

/** Phase-coherent N-band Linkwitz-Riley crossover (1..8 bands, LR2 / LR4 / LR8).

    Topology: serial split tree (split 0 at the lowest frequency, its high output feeds
    split 1, ...) with allpass compensation on every lower band, so that
        sum(bands) = AP_0 * AP_1 * ... * AP_{n-1} * input      (flat magnitude).
    LR8 is the squared 4th-order Butterworth (two Q's), not cascaded LR4.

    All sections are Simper/Cytomic TPT SVFs (and a TPT one-pole for the LR2 allpass)
    with DOUBLE state and coefficients: numerically robust at e.g. 20 Hz @ 768 kHz.

    Crossover frequencies are smoothed in the log-frequency domain (one-pole, default
    20 ms) with coefficients recomputed every kCoeffUpdateInterval (16) samples, which
    makes UI drags zipper/click free. Targets are clamped to [10 Hz, 0.45 fs] and kept at
    least kMinNeighbourRatio apart (applied to the active crossovers only).

    Threading:
      - prepare(): message thread (allocates per-channel state), not concurrently with process().
      - setNumBands / setSlope / setCrossoverFrequency / setters: any thread (atomics);
        they take effect at the start of the next process() call.
      - process() / reset(): audio thread only. process() installs ScopedNoDenormals.
      - getBandMagnitude / getBandResponse: any thread. They read only the atomics
        (target settings), never the audio-thread state.

    Band-count changes keep the state of crossovers that still exist; newly activated
    crossovers start from a cleared state at their target frequency. Slope changes keep
    the state of sections common to both topologies. Both are inherently discontinuous
    (the network changes) — crossfade at a higher level if that matters. */
class Crossover
{
public:
    static constexpr int kMaxBands = 8;
    static constexpr int kMaxCrossovers = kMaxBands - 1;
    static constexpr int kCoeffUpdateInterval = 16;
    static constexpr float kMinFrequencyHz = 10.0f;
    static constexpr float kMaxFrequencyRatio = 0.45f;  // of the sample rate
    static constexpr float kMinNeighbourRatio = 1.12f;  // ~1/6 octave

    enum class Slope { db12 = 0, db24 = 1, db48 = 2 };

    Crossover();

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset() noexcept;

    void setNumBands (int numBands) noexcept;                       // 1..8
    void setSlope (Slope slope) noexcept;
    void setCrossoverFrequency (int index, float hz) noexcept;      // index 0..6
    void setSmoothingTimeMs (float ms) noexcept;                    // default 20 ms

    int getNumBands() const noexcept   { return numBandsTarget.load (std::memory_order_relaxed); }
    Slope getSlope() const noexcept    { return (Slope) slopeTarget.load (std::memory_order_relaxed); }
    float getCrossoverFrequency (int index) const noexcept;         // raw target (unconstrained)
    int getLatencySamples() const noexcept { return 0; }

    /** Splits `input` into getNumBands() bands. bandOutputs points to kMaxBands buffers,
        each with >= numChannels channels and >= numSamples samples. Only the first
        getNumBands() buffers are written; the rest are left untouched. input must not
        alias any band buffer. Audio thread only. */
    void process (const juce::AudioBuffer<float>& input, juce::AudioBuffer<float>* bandOutputs, int numSamples) noexcept;

    /** Analytic |H| of band `band` at hz for the current TARGET settings (UI drawing).
        Exactly the digital response of the network at steady state. Any thread. */
    float getBandMagnitude (int band, float hz) const noexcept;
    std::complex<double> getBandResponse (int band, double hz) const noexcept;

    /** Applies the clamping / minimum-ratio rules in place to n ascending frequencies. */
    static void constrainFrequencies (float* freqs, int n, double sampleRate) noexcept;

private:
    struct SvfState { double s1 = 0.0, s2 = 0.0; };
    struct SvfCoef  { double k = 1.0, a1 = 1.0, a2 = 0.0, a3 = 0.0; };

    struct SplitCoeffs
    {
        SvfCoef c2;          // LR2 split (k = 2)
        SvfCoef c4;          // LR4 split & allpass (k = sqrt 2)
        SvfCoef ca, cb;      // LR8 (Butterworth-4 Q's)
        double onePoleG = 0; // LR2 allpass
    };

    // Section indices within a split: 0 = shared first stage, 1..3 low chain, 4..6 high chain.
    struct SplitState { std::array<SvfState, 7> sec {}; };
    struct ApState    { std::array<SvfState, 2> sec {}; };

    struct ChannelState
    {
        std::array<SplitState, kMaxCrossovers> split {};
        std::array<std::array<ApState, kMaxCrossovers>, kMaxCrossovers> ap {}; // [band][split]
    };

    template <int SlopeIdx> void processRange (int channel, const float* in, float* const* out, int start, int len) noexcept;
    void applyPendingStructureChanges() noexcept;
    void advanceSmoothing (int len) noexcept;
    void updateCoeffs (int split) noexcept;

    static SvfCoef makeSvf (double g, double k) noexcept;

    // Settings (written from any thread)
    std::array<std::atomic<float>, kMaxCrossovers> freqTarget;
    std::atomic<int> numBandsTarget { 4 };
    std::atomic<int> slopeTarget { (int) Slope::db24 };
    std::atomic<double> sampleRateAtomic { 48000.0 };
    std::atomic<float> smoothingMs { 20.0f };

    // Audio-thread state
    double fs = 48000.0;
    int numCh = 2;
    int activeBands = 0;  // 0 = "not yet initialised"
    Slope activeSlope = Slope::db24;
    std::array<double, kMaxCrossovers> curLog2 {}, tgtLog2 {}, coeffLog2 {};
    std::array<SplitCoeffs, kMaxCrossovers> coeffs {};
    std::vector<ChannelState> state;
};

} // namespace st::dsp
