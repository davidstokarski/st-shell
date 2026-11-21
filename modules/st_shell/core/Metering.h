#pragma once

namespace st
{

/** Lock-free level meter feed. Audio thread pushes, UI thread reads. */
class MeterSource
{
public:
    static constexpr int maxChannels = 2;

    void prepare (double sampleRate, float rmsWindowMs = 300.0f);
    void reset();

    /** Audio thread. */
    void push (const float* const* channels, int numChannels, int numSamples) noexcept;
    void push (const juce::AudioBuffer<float>& b, int numSamples) noexcept { push (b.getArrayOfReadPointers(), b.getNumChannels(), numSamples); }

    struct Reading
    {
        std::array<float, maxChannels> peak {};   // linear, max since last read
        std::array<float, maxChannels> rms {};    // linear, smoothed
        int numChannels = 2;
    };

    /** UI thread: returns peak-since-last-read and current RMS. */
    Reading read() noexcept;

private:
    std::array<std::atomic<float>, maxChannels> peaks {}, rmsValues {};
    std::array<float, maxChannels> meanSquare {};
    std::atomic<int> channelsSeen { 2 };
    float rmsCoeff = 0.999f;
};

//==============================================================================
/** Audio-thread sample feed for a spectrum analyser (mono sum, lock-free FIFO). */
class AnalyzerSource
{
public:
    AnalyzerSource();

    void prepare (double sampleRate);
    /** Audio thread. Sums channels to mono. Drops samples if the UI isn't reading. */
    void push (const float* const* channels, int numChannels, int numSamples) noexcept;
    void push (const juce::AudioBuffer<float>& b, int numSamples) noexcept { push (b.getArrayOfReadPointers(), b.getNumChannels(), numSamples); }

    /** UI thread: pull up to `max` samples, returns count. (Single consumer: the plugin's own analyser.) */
    int pull (float* dest, int max) noexcept;

    /** Non-consuming tap for any number of readers on any thread (e.g. another instance's editor):
        the latest history samples are always kept, whether or not the FIFO is active or read.
        getHistoryPosition() counts samples written so far (wraps); readLatest copies the newest n <= historySize. */
    static constexpr int historySize = 1 << 14;
    uint32_t getHistoryPosition() const noexcept { return historyWrite.load (std::memory_order_acquire); }
    void readLatest (float* dest, int n) const noexcept;
    /** Copies n <= historySize samples starting at absolute history position `position` (as counted by
        getHistoryPosition). Readers that track their own position use this, so a block written between their
        position read and this call can't make them skip or repeat samples. */
    void readFrom (uint32_t position, float* dest, int n) const noexcept;
    double getSampleRate() const noexcept { return sampleRate.load(); }

    void setActive (bool shouldBeActive) noexcept { active.store (shouldBeActive); }
    bool isActive() const noexcept { return active.load(); }

private:
    static constexpr int fifoSize = 1 << 15;
    juce::AbstractFifo fifo { fifoSize };
    std::vector<float> buffer;
    std::atomic<double> sampleRate { 48000.0 };
    std::atomic<bool> active { true };
    std::vector<float> history;
    std::atomic<uint32_t> historyWrite { 0 };
};

//==============================================================================
struct SpectrumAnalyzerOptions
{
    int fftOrder = 13;              // 8192
    int numDisplayPoints = 320;
    float minHz = 10.0f, maxHz = 30000.0f;
    float slopeDbPerOct = 4.5f;     // tilt so pink noise looks flat
    float releaseDbPerSec = 42.0f;  // fall speed
    float attack = 0.65f;           // 0..1 rise smoothing
    float floorDb = -120.0f;
    int smoothingPasses = 3;        // ~1/6-octave smoothing at 300 points
    int hopSize = 0;                // new samples needed per FFT (0 = fftSize / 8). Smaller = smoother motion.
    bool nonConsuming = false;      // read the source's history tap instead of its FIFO (several analysers per source)
};

/** UI-side FFT analyser. Call update() from a timer; read display levels. */
class SpectrumAnalyzer
{
public:
    using Options = SpectrumAnalyzerOptions;

    explicit SpectrumAnalyzer (AnalyzerSource&, Options = {});

    /** Pulls new samples, runs FFT, updates smoothed levels. Returns true if levels changed. */
    bool update (double elapsedSeconds);

    const std::vector<float>& getLevelsDb() const noexcept { return levels; }
    float getFrequencyForPoint (int i) const noexcept { return freqs[(size_t) i]; }
    int getNumPoints() const noexcept { return (int) levels.size(); }

    /** Builds a smooth path. x maps log(freq) in [minHz,maxHz] -> bounds.x..right, y maps dB [minDb,maxDb] -> bottom..top. */
    juce::Path createPath (juce::Rectangle<float> bounds, float minDb, float maxDb, float minHz, float maxHz, bool closed) const;

    void clear();

private:
    AnalyzerSource& source;
    Options opts;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> ring, window, fftData, binDb, levels, targets, freqs, scratch;
    int ringPos = 0, fftSize = 0, newSamples = 0;
    uint32_t lastHistory = 0;
    bool historyPrimed = false;
    double pendingSeconds = 0.0, idleSeconds = 0.0;   // ballistics time since the last update / without any data
    double lastSampleRate = 0.0;
    void rebuildMapping (double sr);
};

} // namespace st
