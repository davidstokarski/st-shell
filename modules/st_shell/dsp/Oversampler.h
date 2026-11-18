#pragma once

#include <array>
#include <atomic>
#include <memory>

namespace st::dsp
{

/** Runtime-switchable oversampler (1x/2x/4x/8x/16x) over juce::dsp::Oversampling.

    All five variants are built in prepare() (half-band FIR equiripple, max quality,
    integer latency), so switching factor on the audio thread never allocates: the
    newly selected variant is reset and used from the next processUp().

    Threading:
      - prepare(): message thread.
      - setFactorIndex(): any thread (atomic); applied at the next processUp().
      - processUp() / processDown() / reset(): audio thread, always in pairs.
      - getLatencySamples(): any thread (values are fixed after prepare()).
    When the factor changes the caller must report the new latency to the host
    (and re-time dry-path compensation). */
class Oversampler
{
public:
    static constexpr int kNumFactors = 5; // index i -> 2^i times

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset() noexcept;

    void setFactorIndex (int index) noexcept;                        // 0..4 (1x..16x)
    int getFactorIndex() const noexcept { return requestedIndex.load (std::memory_order_relaxed); }
    int getFactor() const noexcept      { return 1 << getFactorIndex(); }
    static int factorForIndex (int index) noexcept { return 1 << index; }

    /** Integer latency (base-rate samples) of the currently requested factor / any factor. */
    int getLatencySamples() const noexcept { return getLatencySamples (getFactorIndex()); }
    int getLatencySamples (int factorIndex) const noexcept;

    double getOversampledRate() const noexcept { return baseRate * getFactor(); }
    int getMaxOversampledBlockSize() const noexcept { return maxBlock * (1 << (kNumFactors - 1)); }

    /** Upsamples (numSamples = input.getNumSamples()). The returned block (numSamples *
        factor long) is owned by the oversampler and valid until processDown(). At 1x it
        is an internal copy, so the input is never modified. */
    juce::dsp::AudioBlock<float> processUp (const juce::dsp::AudioBlock<const float>& input) noexcept;

    /** Downsamples the block returned by processUp() into output (same length as the
        processUp() input). */
    void processDown (juce::dsp::AudioBlock<float>& output) noexcept;

private:
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, kNumFactors> stages;
    std::array<int, kNumFactors> latencies {};
    std::atomic<int> requestedIndex { 0 };
    int activeIndex = 0;
    double baseRate = 48000.0;
    int maxBlock = 0;
};

} // namespace st::dsp
