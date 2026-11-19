#pragma once

#include <array>
#include <atomic>
#include <complex>
#include <memory>
#include <vector>

/*  PartitionedConvolver — uniform partitioned FFT convolution (overlap-save) for 1–2 channels,
    with impulse responses that can be replaced while audio runs.

      - Routing: `mono` (one IR applied to every channel), `diagonal` (one IR per channel) or
        `matrix` (true-stereo 2x2: out0 = in0*h00 + in1*h01, out1 = in0*h10 + in1*h11).
        The matrix form is what you need for mid/side or cross-feed filters and true-stereo IRs.
      - Latency is exactly `partitionSize` samples.
      - IRs are written from ONE non-audio thread (`writeImpulseResponses`): their spectra are built
        in a free slot and published lock-free; the audio thread picks them up at its next block
        boundary and cross-fades old → new over one partition, so IR changes never click.
      - Before any IR is published the convolver passes audio through (delayed by the latency).

    Threading: prepare() on the message thread with audio and the writer stopped. process() on the
    audio thread (allocation- and lock-free). writeImpulseResponses() on one writer thread. */

namespace st::dsp
{

class PartitionedConvolver
{
public:
    enum class Routing { mono, diagonal, matrix };

    PartitionedConvolver();
    ~PartitionedConvolver();

    /** partitionSize must be a power of two (e.g. 64..4096). maxIrs = 1 (mono), 2 (diagonal) or 4 (matrix). */
    void prepare (int partitionSize, int maxIrLength, int numChannels, int maxIrs = 4);
    void reset() noexcept;

    int getLatencySamples() const noexcept { return P; }
    int getPartitionSize() const noexcept { return P; }
    int getMaxIrLength() const noexcept { return numParts * P; }
    bool isPrepared() const noexcept { return P > 0; }

    /** Writer thread. `irs` holds 1 (mono), 2 (diagonal: ch0, ch1) or 4 (matrix: h00, h01, h10, h11)
        pointers to `length` samples each (length is clipped to getMaxIrLength()). */
    void writeImpulseResponses (Routing, const float* const* irs, int length);

    /** Audio thread: processes in place. */
    void process (float* const* channels, int numChannels, int numSamples) noexcept;

private:
    using Cpx = std::complex<float>;

    struct Slot
    {
        Routing routing = Routing::mono;
        int numIrs = 0, usedParts = 0;
        std::vector<Cpx> spectra;   // [ir][part][bin]
    };

    int P = 0, fftSize = 0, numBins = 0, numParts = 0, numCh = 0, maxIrCount = 1;
    std::unique_ptr<juce::dsp::FFT> fftAudio, fftWriter;

    std::array<Slot, 4> slots;
    std::atomic<int> shared { 2 };
    int writerSlot = 3, activeSlot = 0, prevSlot = 1;
    static constexpr int kFresh = 4;

    // audio state
    std::array<std::vector<float>, 2> inBuf, outBuf;         // inBuf: 2P sliding window, outBuf: P ready samples
    std::array<std::vector<Cpx>, 2> fdl;                     // [part][bin] ring per input channel
    int fdlHead = 0, fill = 0;
    std::vector<float> work;                                  // 2 * fftSize floats
    std::array<std::vector<Cpx>, 2> acc, accOld;
    std::vector<float> fadeOld;

    // writer state
    std::vector<float> writerWork;

    void processPartition() noexcept;
    void accumulate (const Slot&, std::array<std::vector<Cpx>, 2>& out) noexcept;
    void inverse (const std::vector<Cpx>& spectrum, float* dest) noexcept;

    JUCE_DECLARE_NON_COPYABLE (PartitionedConvolver)
};

} // namespace st::dsp
