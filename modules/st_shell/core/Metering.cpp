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
AnalyzerSource::AnalyzerSource() : buffer ((size_t) fifoSize, 0.0f), history ((size_t) historySize, 0.0f) {}

void AnalyzerSource::prepare (double sr)
{
    sampleRate.store (sr);
    fifo.reset();
}

void AnalyzerSource::push (const float* const* channels, int numChannels, int numSamples) noexcept
{
    if (numChannels <= 0)
        return;

    // history tap (always)
    {
        const float g = 1.0f / (float) numChannels;
        const uint32_t w = historyWrite.load (std::memory_order_relaxed);
        for (int i = 0; i < numSamples; ++i)
        {
            float sum = 0.0f;
            for (int c = 0; c < numChannels; ++c) sum += channels[c][i];
            history[(size_t) ((w + (uint32_t) i) & (uint32_t) (historySize - 1))] = sum * g;
        }
        historyWrite.store (w + (uint32_t) numSamples, std::memory_order_release);
    }

    if (! active.load (std::memory_order_relaxed))
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

void AnalyzerSource::readLatest (float* dest, int n) const noexcept
{
    n = juce::jlimit (0, historySize, n);
    const uint32_t w = historyWrite.load (std::memory_order_acquire);
    for (int i = 0; i < n; ++i)
        dest[i] = history[(size_t) ((w - (uint32_t) n + (uint32_t) i) & (uint32_t) (historySize - 1))];
}

void AnalyzerSource::readFrom (uint32_t position, float* dest, int n) const noexcept
{
    n = juce::jlimit (0, historySize, n);
    for (int i = 0; i < n; ++i)
        dest[i] = history[(size_t) ((position + (uint32_t) i) & (uint32_t) (historySize - 1))];
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
    targets.assign ((size_t) opts.numDisplayPoints, opts.floorDb);
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
    if (std::abs (sr - lastSampleRate) > 0.5)
        rebuildMapping (sr);

    // Pull everything available into the ring buffer.
    int got = 0;
    if (opts.nonConsuming)
    {
        // Read exactly [lastHistory, w): continuous samples even when the audio thread writes meanwhile.
        const uint32_t w = source.getHistoryPosition();
        const uint32_t avail = historyPrimed ? w - lastHistory : w;
        const uint32_t limit = (uint32_t) juce::jmin ((int) scratch.size(), AnalyzerSource::historySize / 2);
        const uint32_t start = avail > limit ? w - limit : (historyPrimed ? lastHistory : 0u);
        const int fresh = (int) juce::jmin (avail, limit);
        historyPrimed = true;
        lastHistory = w;
        got = fresh;
        if (fresh > 0)
        {
            source.readFrom (start, scratch.data(), fresh);
            for (int i = 0; i < fresh; ++i)
            {
                ring[(size_t) ringPos] = scratch[(size_t) i];
                ringPos = (ringPos + 1) & (fftSize - 1);
            }
            newSamples += fresh;
        }
    }
    else
    {
        int n = 0;
        while ((n = source.pull (scratch.data(), (int) scratch.size())) > 0)
        {
            for (int i = 0; i < n; ++i)
            {
                ring[(size_t) ringPos] = scratch[(size_t) i];
                ringPos = (ringPos + 1) & (fftSize - 1);
            }
            newSamples += n;
            got += n;
        }
    }

    pendingSeconds += elapsedSeconds;
    idleSeconds = got > 0 ? 0.0 : idleSeconds + elapsedSeconds;

    if (newSamples < (opts.hopSize > 0 ? opts.hopSize : fftSize / 8))
    {
        // Audio is still arriving (just not a whole hop yet, e.g. big host buffers): hold, don't flicker.
        if (idleSeconds < 0.12) return false;

        // No data any more (transport stopped): let the display decay gracefully.
        const float fall = (float) (opts.releaseDbPerSec * pendingSeconds);
        pendingSeconds = 0.0;
        bool changed = false;
        for (auto& l : levels)
        {
            const float n = juce::jmax (opts.floorDb, l - fall);
            changed |= std::abs (n - l) > 1.0e-4f;
            l = n;
        }
        return changed;
    }

    newSamples = 0;
    const float fall = (float) (opts.releaseDbPerSec * juce::jmin (0.25, pendingSeconds));
    pendingSeconds = 0.0;

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

    // 1) Per display point: power-average the bins it covers (or interpolate when narrower than a bin).
    for (int i = 0; i < opts.numDisplayPoints; ++i)
    {
        const float f = freqs[(size_t) i];
        const float fLo = (i > 0 ? std::sqrt (f * freqs[(size_t) i - 1]) : f);
        const float fHi = (i < opts.numDisplayPoints - 1 ? std::sqrt (f * freqs[(size_t) i + 1]) : f);
        const float bLo = fLo / binHz, bHi = fHi / binHz;
        float db;

        if (bHi - bLo < 1.0f)
        {
            const float pos = juce::jlimit (0.0f, (float) (numBins - 2), f / binHz);
            const int i0 = (int) pos;
            const float frac = pos - (float) i0;
            db = binDb[(size_t) i0] + (binDb[(size_t) i0 + 1] - binDb[(size_t) i0]) * frac;
        }
        else
        {
            const int b0 = juce::jlimit (0, numBins - 1, (int) std::ceil (bLo));
            const int b1 = juce::jlimit (b0, numBins - 1, (int) std::floor (bHi));
            double power = 0.0;
            for (int b = b0; b <= b1; ++b) power += std::pow (10.0, binDb[(size_t) b] * 0.1);
            db = (float) (10.0 * std::log10 (juce::jmax (1.0e-14, power / (double) (b1 - b0 + 1))));
        }

        if (f > sr * 0.5) db = opts.floorDb;
        targets[(size_t) i] = db + opts.slopeDbPerOct * std::log2 (f / 1000.0f);
    }

    // 2) Fractional-octave smoothing across display points (keeps the curve calm and readable).
    for (int pass = 0; pass < opts.smoothingPasses; ++pass)
    {
        float prev = targets[0];
        for (int i = 1; i < opts.numDisplayPoints - 1; ++i)
        {
            const float cur = targets[(size_t) i];
            targets[(size_t) i] = 0.25f * prev + 0.5f * cur + 0.25f * targets[(size_t) i + 1];
            prev = cur;
        }
    }

    // 3) Ballistics
    for (int i = 0; i < opts.numDisplayPoints; ++i)
    {
        const float db = targets[(size_t) i];
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
