#pragma once

#include <complex>
#include <memory>
#include <vector>

/*  StftProcessor — streaming short-time Fourier transform with weighted overlap-add, for spectral
    processing (spectral dynamics, de-noising, spectral gates...).

      - Frame size N = 2^order, hop N/4, square-root periodic Hann analysis and synthesis windows.
        With no change to the spectra the output is the input delayed by exactly N samples.
      - Main channels (1–2) are analysed, handed to the frame callback (modifiable) and resynthesised.
      - Side channels (0–4) are analysed only (detectors: dry input, side chain...).
      - Spectra hold N/2 + 1 bins (DC..Nyquist). A sine of amplitude A centred on a bin reads
        |X| = A * getSineNormalisation()^-1, i.e. multiply |X| by getSineNormalisation() for amplitude.

    Threading: prepare() on the message thread (allocates). process() on the audio thread,
    allocation- and lock-free; the callback runs on the audio thread once per hop. */

namespace st::dsp
{

class StftProcessor
{
public:
    using Cpx = std::complex<float>;

    StftProcessor();
    ~StftProcessor();

    void prepare (int fftOrder, int numMainChannels, int numSideChannels = 0);
    void reset() noexcept;
    /** Frees everything (isPrepared() becomes false). Message thread. */
    void release();

    bool isPrepared() const noexcept { return N > 0; }
    int getFftSize() const noexcept { return N; }
    int getHopSize() const noexcept { return N / 4; }
    int getNumBins() const noexcept { return N / 2 + 1; }
    int getLatencySamples() const noexcept { return N; }
    /** |X_k| * this = amplitude of a sine centred on bin k. */
    float getSineNormalisation() const noexcept { return sineNorm; }

    /** Frame callback access (valid inside the callback). */
    Cpx* getSpectrum (int mainChannel) noexcept { return reinterpret_cast<Cpx*> (mainWork[(size_t) mainChannel].data()); }
    const Cpx* getSideSpectrum (int sideChannel) const noexcept { return reinterpret_cast<const Cpx*> (sideWork[(size_t) sideChannel].data()); }

    /** Processes numSamples in place. `side` may be null when there are no side channels (silence is used
        for missing ones). onFrame (StftProcessor&) is called once per hop with fresh spectra. */
    template <typename FrameFn>
    void process (float* const* io, int numChannels, const float* const* side, int numSide, int numSamples, FrameFn&& onFrame) noexcept
    {
        const int nc = std::min (numChannels, numMain);
        for (int i = 0; i < numSamples; ++i)
        {
            for (int c = 0; c < nc; ++c)
            {
                auto& in = mainIn[(size_t) c];
                auto& out = mainOut[(size_t) c];
                in[(size_t) writePos] = io[c][i];
                io[c][i] = out[(size_t) writePos];
                out[(size_t) writePos] = 0.0f;
            }
            for (int s = 0; s < numSideCh; ++s)
                sideIn[(size_t) s][(size_t) writePos] = (side != nullptr && s < numSide && side[s] != nullptr) ? side[s][i] : 0.0f;

            writePos = (writePos + 1) & mask;
            if (++hopCount == N / 4)
            {
                hopCount = 0;
                analyse (nc);
                onFrame (*this);
                synthesise (nc);
            }
        }
    }

private:
    struct Fft;
    std::unique_ptr<Fft> fft;
    int N = 0, mask = 0, numMain = 0, numSideCh = 0, writePos = 0, hopCount = 0;
    float sineNorm = 1.0f;
    std::vector<float> window;
    std::vector<std::vector<float>> mainIn, mainOut, sideIn, mainWork, sideWork;

    void analyse (int numChannels) noexcept;
    void synthesise (int numChannels) noexcept;
};

} // namespace st::dsp
