namespace st::dsp
{

struct StftProcessor::Fft
{
    explicit Fft (int order) : fft (order) {}
    juce::dsp::FFT fft;
};

StftProcessor::StftProcessor() = default;
StftProcessor::~StftProcessor() = default;

void StftProcessor::prepare (int fftOrder, int numMainChannels, int numSideChannels)
{
    fftOrder = std::clamp (fftOrder, 6, 15);
    N = 1 << fftOrder;
    mask = N - 1;
    numMain = std::clamp (numMainChannels, 1, 2);
    numSideCh = std::clamp (numSideChannels, 0, 4);
    fft = std::make_unique<Fft> (fftOrder);

    window.assign ((size_t) N, 0.0f);
    double sum = 0.0;
    for (int i = 0; i < N; ++i)
    {
        const double h = 0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * i / N);   // periodic Hann
        window[(size_t) i] = (float) std::sqrt (h);
        sum += window[(size_t) i];
    }
    sineNorm = (float) (2.0 / sum);

    auto alloc = [] (std::vector<std::vector<float>>& v, int count, size_t size) { v.assign ((size_t) count, std::vector<float> (size, 0.0f)); };
    alloc (mainIn, numMain, (size_t) N);
    alloc (mainOut, numMain, (size_t) N);
    alloc (sideIn, numSideCh, (size_t) N);
    alloc (mainWork, numMain, (size_t) (2 * N));
    alloc (sideWork, numSideCh, (size_t) (2 * N));
    reset();
}

void StftProcessor::release()
{
    fft.reset();
    N = mask = numMain = numSideCh = writePos = hopCount = 0;
    window = {};
    mainIn = mainOut = sideIn = mainWork = sideWork = {};
}

void StftProcessor::reset() noexcept
{
    for (auto* v : { &mainIn, &mainOut, &sideIn, &mainWork, &sideWork })
        for (auto& b : *v) std::fill (b.begin(), b.end(), 0.0f);
    writePos = 0;
    hopCount = 0;
}

void StftProcessor::analyse (int nc) noexcept
{
    auto run = [this] (const std::vector<float>& ring, std::vector<float>& work)
    {
        for (int j = 0; j < N; ++j)
            work[(size_t) j] = ring[(size_t) ((writePos + j) & mask)] * window[(size_t) j];
        std::fill (work.begin() + N, work.end(), 0.0f);
        fft->fft.performRealOnlyForwardTransform (work.data(), true);
    };
    for (int c = 0; c < nc; ++c) run (mainIn[(size_t) c], mainWork[(size_t) c]);
    for (int s = 0; s < numSideCh; ++s) run (sideIn[(size_t) s], sideWork[(size_t) s]);
}

void StftProcessor::synthesise (int nc) noexcept
{
    // sqrt-Hann analysis x synthesis = Hann; Hann frames at hop N/4 sum to 2.
    constexpr float overlapScale = 0.5f;
    for (int c = 0; c < nc; ++c)
    {
        auto& work = mainWork[(size_t) c];
        // Mirror the negative frequencies so the inverse is real (JUCE reads the full spectrum).
        auto* x = reinterpret_cast<Cpx*> (work.data());
        for (int k = 1; k < N / 2; ++k) x[N - k] = std::conj (x[k]);
        fft->fft.performRealOnlyInverseTransform (work.data());   // normalised by 1/N

        auto& out = mainOut[(size_t) c];
        for (int j = 0; j < N; ++j)
            out[(size_t) ((writePos + j) & mask)] += work[(size_t) j] * window[(size_t) j] * overlapScale;
    }
}

} // namespace st::dsp
