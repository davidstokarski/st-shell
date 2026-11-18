namespace st
{

//==============================================================================
void MeterSource::prepare (double sampleRate, float rmsWindowMs)
{
    rmsCoeff = (float) std::exp (-1.0 / (juce::jmax (1.0, sampleRate) * rmsWindowMs * 0.001));
    reset();
}

void MeterSource::reset()
{
    for (auto& p : peaks) p.store (0.0f);
    for (auto& r : rmsValues) r.store (0.0f);
    meanSquare.fill (0.0f);
}

void MeterSource::push (const float* const* channels, int numChannels, int numSamples) noexcept
{
    const int nc = juce::jmin (numChannels, maxChannels);
    channelsSeen.store (nc);

    for (int c = 0; c < nc; ++c)
    {
        const float* x = channels[c];
        float pk = 0.0f;
        float ms = meanSquare[(size_t) c];

        for (int i = 0; i < numSamples; ++i)
        {
            const float v = x[i];
            pk = juce::jmax (pk, std::abs (v));
            ms = rmsCoeff * ms + (1.0f - rmsCoeff) * v * v;
        }

        if (! std::isfinite (ms)) ms = 0.0f;
        if (ms < 1.0e-20f) ms = 0.0f;
        meanSquare[(size_t) c] = ms;

        auto& peakAtomic = peaks[(size_t) c];
        float prev = peakAtomic.load (std::memory_order_relaxed);
        while (pk > prev && ! peakAtomic.compare_exchange_weak (prev, pk, std::memory_order_relaxed)) {}

        rmsValues[(size_t) c].store (std::sqrt (ms), std::memory_order_relaxed);
    }
}

MeterSource::Reading MeterSource::read() noexcept
{
    Reading r;
    r.numChannels = channelsSeen.load();
    for (size_t c = 0; c < (size_t) maxChannels; ++c)
    {
        r.peak[c] = peaks[c].exchange (0.0f, std::memory_order_relaxed);
        r.rms[c] = rmsValues[c].load (std::memory_order_relaxed);
    }
    if (r.numChannels == 1)
    {
        r.peak[1] = r.peak[0];
        r.rms[1] = r.rms[0];
    }
    return r;
}

//==============================================================================
AnalyzerSource::AnalyzerSource() : buffer ((size_t) fifoSize, 0.0f) {}

void AnalyzerSource::prepare (double sr)
{
    sampleRate.store (sr);
    fifo.reset();
}

void AnalyzerSource::push (const float* const* channels, int numChannels, int numSamples) noexcept
{
    if (! active.load (std::memory_order_relaxed) || numChannels <= 0)
        return;

    const int toWrite = juce::jmin (numSamples, fifo.getFreeSpace());
    if (toWrite <= 0)
        return;

    int s1, n1, s2, n2;
    fifo.prepareToWrite (toWrite, s1, n1, s2, n2);
    const float gain = 1.0f / (float) numChannels;

    auto write = [&] (int start, int count, int offset)
    {
        for (int i = 0; i < count; ++i)
        {
            float sum = 0.0f;
            for (int c = 0; c < numChannels; ++c)
                sum += channels[c][offset + i];
            buffer[(size_t) (start + i)] = sum * gain;
        }
    };

    write (s1, n1, 0);
    write (s2, n2, n1);
    fifo.finishedWrite (n1 + n2);
}

int AnalyzerSource::pull (float* dest, int max) noexcept
{
    const int toRead = juce::jmin (max, fifo.getNumReady());
    int s1, n1, s2, n2;
    fifo.prepareToRead (toRead, s1, n1, s2, n2);
    if (n1 > 0) std::copy (buffer.begin() + s1, buffer.begin() + s1 + n1, dest);
    if (n2 > 0) std::copy (buffer.begin() + s2, buffer.begin() + s2 + n2, dest + n1);
    fifo.finishedRead (n1 + n2);
    return n1 + n2;
}

//==============================================================================
SpectrumAnalyzer::SpectrumAnalyzer (AnalyzerSource& s, Options o) : source (s), opts (o)
{
    fftSize = 1 << opts.fftOrder;
    fft = std::make_unique<juce::dsp::FFT> (opts.fftOrder);
    ring.assign ((size_t) fftSize, 0.0f);
    window.resize ((size_t) fftSize);
    juce::dsp::WindowingFunction<float>::fillWindowingTables (window.data(), (size_t) fftSize,
                                                             juce::dsp::WindowingFunction<float>::blackmanHarris, false);
    fftData.assign ((size_t) fftSize * 2, 0.0f);
    binDb.assign ((size_t) fftSize / 2 + 1, opts.floorDb);
    levels.assign ((size_t) opts.numDisplayPoints, opts.floorDb);
    freqs.resize ((size_t) opts.numDisplayPoints);
    scratch.resize (8192);

    for (int i = 0; i < opts.numDisplayPoints; ++i)
    {
        const float t = (float) i / (float) (opts.numDisplayPoints - 1);
        freqs[(size_t) i] = opts.minHz * std::pow (opts.maxHz / opts.minHz, t);
    }
}

void SpectrumAnalyzer::clear()
{
    std::fill (ring.begin(), ring.end(), 0.0f);
    std::fill (levels.begin(), levels.end(), opts.floorDb);
}

void SpectrumAnalyzer::rebuildMapping (double sr) { lastSampleRate = sr; }

bool SpectrumAnalyzer::update (double elapsedSeconds)
{
    const double sr = source.getSampleRate();
    if (sr != lastSampleRate)
        rebuildMapping (sr);

    // Pull everything available into the ring buffer.
    int got = 0;
    while ((got = source.pull (scratch.data(), (int) scratch.size())) > 0)
    {
        for (int i = 0; i < got; ++i)
        {
            ring[(size_t) ringPos] = scratch[(size_t) i];
            ringPos = (ringPos + 1) & (fftSize - 1);
        }
        newSamples += got;
    }

    const float fall = (float) (opts.releaseDbPerSec * elapsedSeconds);

    if (newSamples < fftSize / 8)
    {
        // No fresh data: let the display decay gracefully.
        bool changed = false;
        for (auto& l : levels)
        {
            const float n = juce::jmax (opts.floorDb, l - fall);
            changed |= (n != l);
            l = n;
        }
        return changed;
    }

    newSamples = 0;

    for (int i = 0; i < fftSize; ++i)
        fftData[(size_t) i] = ring[(size_t) ((ringPos + i) & (fftSize - 1))] * window[(size_t) i];
    std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);

    fft->performFrequencyOnlyForwardTransform (fftData.data(), true);

    // Window coherent gain for Blackman-Harris ~0.35875
    const float norm = 2.0f / ((float) fftSize * 0.35875f);
    const int numBins = fftSize / 2 + 1;
    for (int b = 0; b < numBins; ++b)
        binDb[(size_t) b] = juce::Decibels::gainToDecibels (fftData[(size_t) b] * norm, opts.floorDb);

    const float binHz = (float) (sr / fftSize);

    for (int i = 0; i < opts.numDisplayPoints; ++i)
    {
        const float f = freqs[(size_t) i];
        const float fLo = (i > 0 ? std::sqrt (f * freqs[(size_t) i - 1]) : f);
        const float fHi = (i < opts.numDisplayPoints - 1 ? std::sqrt (f * freqs[(size_t) i + 1]) : f);

        const float bLo = fLo / binHz, bHi = fHi / binHz;
        float db;

        if (bHi - bLo < 1.0f)
        {
            // Interpolate between bins (cubic-ish via linear in dB is fine for display).
            const float pos = juce::jlimit (0.0f, (float) (numBins - 2), f / binHz);
            const int i0 = (int) pos;
            const float frac = pos - (float) i0;
            db = binDb[(size_t) i0] + (binDb[(size_t) i0 + 1] - binDb[(size_t) i0]) * frac;
        }
        else
        {
            const int b0 = juce::jlimit (0, numBins - 1, (int) std::ceil (bLo));
            const int b1 = juce::jlimit (0, numBins - 1, (int) std::floor (bHi));
            float mx = opts.floorDb;
            for (int b = b0; b <= b1; ++b) mx = juce::jmax (mx, binDb[(size_t) b]);
            db = mx;
        }

        if (f > sr * 0.5) db = opts.floorDb;

        db += opts.slopeDbPerOct * std::log2 (f / 1000.0f);

        auto& l = levels[(size_t) i];
        if (db > l) l += (db - l) * opts.attack;
        else        l = juce::jmax (db, l - fall);
    }

    return true;
}

juce::Path SpectrumAnalyzer::createPath (juce::Rectangle<float> b, float minDb, float maxDb, float minHz, float maxHz, bool closed) const
{
    juce::Path p;
    const float logMin = std::log (minHz), logRange = std::log (maxHz) - logMin;
    bool started = false;
    juce::Point<float> prev;

    for (int i = 0; i < (int) levels.size(); ++i)
    {
        const float f = freqs[(size_t) i];
        if (f < minHz * 0.9f || f > maxHz * 1.1f) continue;

        const float x = b.getX() + b.getWidth() * (std::log (f) - logMin) / logRange;
        const float y = juce::jmap (juce::jlimit (minDb - 6.0f, maxDb, levels[(size_t) i]), minDb, maxDb, b.getBottom(), b.getY());
        const juce::Point<float> pt (x, y);

        if (! started)
        {
            if (closed) { p.startNewSubPath (x, b.getBottom()); p.lineTo (pt); }
            else p.startNewSubPath (pt);
            started = true;
        }
        else
        {
            const auto mid = (prev + pt) * 0.5f;
            p.quadraticTo (prev, mid);
        }
        prev = pt;
    }

    if (started)
    {
        p.lineTo (prev);
        if (closed)
        {
            p.lineTo (prev.x, b.getBottom());
            p.closeSubPath();
        }
    }
    return p;
}

} // namespace st
