namespace st::dsp
{

namespace
{
    constexpr double kParamRampMs = 20.0;

    inline float inverseRatio (float r) noexcept
    {
        return r >= UpDownCompressor::kInfiniteRatio ? 0.0f : 1.0f / std::max (1.0f, r);
    }
} // namespace

//==============================================================================
float UpDownCompressor::curveGainDb (float x, const Curve& c) noexcept
{
    const float W = c.knee;
    float g = 0.0f;

    // Downward (reduction above threshold): downSlope = 1/R - 1 <= 0
    {
        const float d = x - c.downT;
        if (W > 0.0f && 2.0f * std::abs (d) <= W)
        {
            const float t = d + 0.5f * W;
            g += c.downSlope * t * t / (2.0f * W);
        }
        else if (d > 0.0f)
        {
            g += c.downSlope * d;
        }
    }

    // Upward (boost below threshold): upSlope = 1 - 1/R >= 0
    {
        const float d = c.upT - x;
        float u = 0.0f;
        if (W > 0.0f && 2.0f * std::abs (d) <= W)
        {
            const float t = d + 0.5f * W;
            u = c.upSlope * t * t / (2.0f * W);
        }
        else if (d > 0.0f)
        {
            u = c.upSlope * d;
        }

        if (u > 0.0f)
        {
            u = std::min (u, c.maxUp);
            if (c.fadeDb > 0.0f)
            {
                const float t = std::clamp ((x - c.floorDb) / c.fadeDb, 0.0f, 1.0f);
                u *= t * t * (3.0f - 2.0f * t);
            }
            else if (x <= c.floorDb)
            {
                u = 0.0f;
            }
            g += u;
        }
    }

    return g * c.depth;
}

float UpDownCompressor::computeStaticGainDb (float inputDb, const Params& p) noexcept
{
    const Curve c { p.downThresholdDb, inverseRatio (p.downRatio) - 1.0f,
                    p.upThresholdDb,   1.0f - inverseRatio (p.upRatio),
                    std::max (0.0f, p.kneeDb), std::max (0.0f, p.maxUpwardGainDb),
                    p.upwardFloorDb, std::max (0.0f, p.upwardFloorFadeDb),
                    std::clamp (p.depth, -1.0f, 1.0f) };
    return curveGainDb (inputDb, c);
}

UpDownCompressor::Curve UpDownCompressor::currentCurve() const noexcept
{
    return { smoothers[sDownT].getCurrent(), smoothers[sDownSlope].getCurrent(),
             smoothers[sUpT].getCurrent(),   smoothers[sUpSlope].getCurrent(),
             smoothers[sKnee].getCurrent(),  smoothers[sMaxUp].getCurrent(),
             params.upwardFloorDb, std::max (0.0f, params.upwardFloorFadeDb),
             smoothers[sDepth].getCurrent() };
}

//==============================================================================
void UpDownCompressor::prepare (double sampleRate, int /*maxBlockSize*/, int numChannels)
{
    fs = sampleRate > 0.0 ? sampleRate : 48000.0;
    numCh = std::clamp (numChannels, 1, kMaxChannels);
    for (auto& s : smoothers)
        s.prepare (fs, kParamRampMs);
    firstBlock = true;
    setParams (params);
    reset();
}

void UpDownCompressor::reset() noexcept
{
    env.fill (0.0f);
    meanSq.fill (0.0f);
    meterGainDb.store (0.0f);
    meterLevelDb.store (kMinusInfDb);
}

void UpDownCompressor::updateBallistics() noexcept
{
    attCoeff = onePoleCoeff (params.attackMs, fs);
    relCoeff = onePoleCoeff (params.releaseMs, fs);
    rmsCoeff = onePoleCoeff (std::max (0.1f, params.rmsWindowMs), fs);
}

void UpDownCompressor::setParams (const Params& p) noexcept
{
    params = p;
    const float targets[kNumSmoothers] = {
        p.downThresholdDb,
        inverseRatio (p.downRatio) - 1.0f,
        p.upThresholdDb,
        1.0f - inverseRatio (p.upRatio),
        std::max (0.0f, p.kneeDb),
        std::max (0.0f, p.maxUpwardGainDb),
        std::clamp (p.depth, -1.0f, 1.0f),
        std::clamp (p.stereoLink, 0.0f, 1.0f),
        std::clamp (p.rmsBlend, 0.0f, 1.0f)
    };

    for (int i = 0; i < kNumSmoothers; ++i)
    {
        if (firstBlock)
            smoothers[(size_t) i].setCurrentAndTarget (targets[i]);
        else
            smoothers[(size_t) i].setTarget (targets[i]);
    }
    updateBallistics();
}

//==============================================================================
void UpDownCompressor::process (juce::AudioBuffer<float>& audio, int numSamples, const juce::AudioBuffer<float>* detector) noexcept
{
    juce::ScopedNoDenormals noDenormals;
    firstBlock = false;

    const int chans = std::min (numCh, audio.getNumChannels());
    if (chans <= 0 || numSamples <= 0)
        return;

    std::array<float*, kMaxChannels> out {};
    std::array<const float*, kMaxChannels> det {};
    for (int ch = 0; ch < chans; ++ch)
    {
        out[(size_t) ch] = audio.getWritePointer (ch);
        if (detector != nullptr && detector->getNumChannels() > 0)
            det[(size_t) ch] = detector->getReadPointer (std::min (ch, detector->getNumChannels() - 1));
        else
            det[(size_t) ch] = out[(size_t) ch];
    }

    auto anySmoothing = [this]
    {
        for (auto& s : smoothers)
            if (s.isSmoothing())
                return true;
        return false;
    };

    bool smoothing = anySmoothing();
    Curve curve = currentCurve();
    float link = smoothers[sLink].getCurrent();
    float blend = smoothers[sBlend].getCurrent();
    std::array<float, kMaxChannels> lastGain {};

    for (int n = 0; n < numSamples; ++n)
    {
        if (smoothing)
        {
            for (auto& s : smoothers)
                s.getNext();
            curve = currentCurve();
            link = smoothers[sLink].getCurrent();
            blend = smoothers[sBlend].getCurrent();
            smoothing = anySmoothing();
        }

        // Detection (all channels read before any is written -> detector may alias audio).
        std::array<float, kMaxChannels> d {};
        float dMax = 0.0f;
        for (int ch = 0; ch < chans; ++ch)
        {
            const float x = det[(size_t) ch][n];
            const float a = std::abs (x);
            float& ms = meanSq[(size_t) ch];
            ms += rmsCoeff * (x * x - ms);
            const float v = blend > 0.0f ? a + blend * (std::sqrt (ms) - a) : a;
            d[(size_t) ch] = v;
            dMax = std::max (dMax, v);
        }

        for (int ch = 0; ch < chans; ++ch)
        {
            const float dl = d[(size_t) ch] + link * (dMax - d[(size_t) ch]);
            float& e = env[(size_t) ch];
            e += (dl > e ? attCoeff : relCoeff) * (dl - e);
            const float g = curveGainDb (fastGainToDb (e), curve);
            out[(size_t) ch][n] *= fastDbToGain (g);
            lastGain[(size_t) ch] = g;
        }
    }

    // Denormal flush + meters.
    float meterGain = 0.0f, meterLevel = kMinusInfDb;
    for (int ch = 0; ch < chans; ++ch)
    {
        if (env[(size_t) ch] < 1.0e-9f)    env[(size_t) ch] = 0.0f;
        if (meanSq[(size_t) ch] < 1.0e-18f) meanSq[(size_t) ch] = 0.0f;
        if (std::abs (lastGain[(size_t) ch]) > std::abs (meterGain))
            meterGain = lastGain[(size_t) ch];
        meterLevel = std::max (meterLevel, gainToDb (env[(size_t) ch]));
    }
    meterGainDb.store (meterGain, std::memory_order_relaxed);
    meterLevelDb.store (meterLevel, std::memory_order_relaxed);
}

} // namespace st::dsp
