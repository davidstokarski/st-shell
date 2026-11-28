namespace st::dsp
{

void Oversampler::prepare (double sampleRate, int maxBlockSize, int numChannels, int maxFactorIndex)
{
    baseRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    maxBlock = std::max (1, maxBlockSize);
    const auto chans = (size_t) std::max (1, numChannels);
    numFactors = std::clamp (maxFactorIndex, 0, kMaxNumFactors - 1) + 1;
    for (auto& s : stages) s.reset();

    for (int i = 0; i < numFactors; ++i)
    {
        auto os = std::make_unique<juce::dsp::Oversampling<float>> (
            chans, (size_t) i, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
            true /* max quality */, true /* integer latency */);
        os->initProcessing ((size_t) maxBlock);
        latencies[(size_t) i] = (int) std::lround (os->getLatencyInSamples());
        stages[(size_t) i] = std::move (os);
    }

    requestedIndex.store (std::clamp (requestedIndex.load(), 0, numFactors - 1));
    activeIndex = requestedIndex.load();
    reset();
}

void Oversampler::reset() noexcept
{
    for (auto& s : stages)
        if (s != nullptr)
            s->reset();
}

void Oversampler::setFactorIndex (int index) noexcept
{
    requestedIndex.store (std::clamp (index, 0, numFactors - 1));
}

int Oversampler::getLatencySamples (int factorIndex) const noexcept
{
    return latencies[(size_t) std::clamp (factorIndex, 0, numFactors - 1)];
}

juce::dsp::AudioBlock<float> Oversampler::processUp (const juce::dsp::AudioBlock<const float>& input) noexcept
{
    const int requested = requestedIndex.load (std::memory_order_relaxed);
    if (requested != activeIndex)
    {
        activeIndex = requested;
        if (stages[(size_t) activeIndex] != nullptr)
            stages[(size_t) activeIndex]->reset(); // no allocation
    }

    auto& os = stages[(size_t) activeIndex];
    jassert (os != nullptr && (int) input.getNumSamples() <= maxBlock);
    if (os == nullptr)
        return {};
    return os->processSamplesUp (input);
}

void Oversampler::processDown (juce::dsp::AudioBlock<float>& output) noexcept
{
    if (auto& os = stages[(size_t) activeIndex]; os != nullptr)
        os->processSamplesDown (output);
}

} // namespace st::dsp
