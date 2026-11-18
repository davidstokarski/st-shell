namespace st::dsp
{

namespace
{
    constexpr double kSqrt2 = 1.4142135623730951;
    constexpr double kKa = 0.7653668647301796;  // 2 sin(pi/8)  -> Q 1.3066
    constexpr double kKb = 1.8477590650225735;  // 2 sin(3pi/8) -> Q 0.5412

    constexpr std::array<float, Crossover::kMaxCrossovers> kDefaultFreqs { 100.0f, 400.0f, 1600.0f, 4000.0f,
                                                                           7000.0f, 10000.0f, 14000.0f };

    inline void svfTick (double& s1, double& s2, double a1, double a2, double a3,
                         double v0, double& v1, double& v2) noexcept
    {
        const double v3 = v0 - s2;
        v1 = a1 * s1 + a2 * v3;
        v2 = s2 + a2 * s1 + a3 * v3;
        s1 = 2.0 * v1 - s1;
        s2 = 2.0 * v2 - s2;
    }
} // namespace

Crossover::Crossover()
{
    for (size_t i = 0; i < freqTarget.size(); ++i)
        freqTarget[i].store (kDefaultFreqs[i]);
}

Crossover::SvfCoef Crossover::makeSvf (double g, double k) noexcept
{
    SvfCoef c;
    c.k = k;
    c.a1 = 1.0 / (1.0 + g * (g + k));
    c.a2 = g * c.a1;
    c.a3 = g * c.a2;
    return c;
}

void Crossover::constrainFrequencies (float* f, int n, double sampleRate) noexcept
{
    if (n <= 0)
        return;
    const float lo = kMinFrequencyHz;
    const float hi = std::max (lo, (float) (kMaxFrequencyRatio * sampleRate));
    const float r = kMinNeighbourRatio;

    for (int i = 0; i < n; ++i)
    {
        float v = std::isfinite (f[i]) ? std::clamp (f[i], lo, hi) : lo;
        if (i > 0)
            v = std::max (v, f[i - 1] * r);
        f[i] = v;
    }
    f[n - 1] = std::min (f[n - 1], hi);
    for (int i = n - 2; i >= 0; --i)
        f[i] = std::min (f[i], f[i + 1] / r);
    for (int i = 0; i < n; ++i)   // only matters if the range can't fit n crossovers
        f[i] = std::max (f[i], lo);
}

//==============================================================================
void Crossover::prepare (double sampleRate, int /*maxBlockSize*/, int numChannels)
{
    fs = sampleRate > 0.0 ? sampleRate : 48000.0;
    sampleRateAtomic.store (fs);
    numCh = std::max (1, numChannels);
    state.assign ((size_t) numCh, ChannelState {});
    activeBands = 0; // forces snapping of all smoothed frequencies at the next process()
    reset();
}

void Crossover::reset() noexcept
{
    for (auto& s : state)
        s = ChannelState {};
    activeBands = 0;
}

void Crossover::setNumBands (int n) noexcept          { numBandsTarget.store (std::clamp (n, 1, kMaxBands)); }
void Crossover::setSlope (Slope s) noexcept           { slopeTarget.store ((int) s); }
void Crossover::setSmoothingTimeMs (float ms) noexcept { smoothingMs.store (std::max (0.0f, ms)); }

void Crossover::setCrossoverFrequency (int index, float hz) noexcept
{
    if (index >= 0 && index < kMaxCrossovers && std::isfinite (hz))
        freqTarget[(size_t) index].store (hz);
}

float Crossover::getCrossoverFrequency (int index) const noexcept
{
    return (index >= 0 && index < kMaxCrossovers) ? freqTarget[(size_t) index].load() : 0.0f;
}

//==============================================================================
void Crossover::updateCoeffs (int i) noexcept
{
    const double f = std::exp2 (curLog2[(size_t) i]);
    const double g = std::tan (kPi * f / fs);
    auto& c = coeffs[(size_t) i];
    c.c2 = makeSvf (g, 2.0);
    c.c4 = makeSvf (g, kSqrt2);
    c.ca = makeSvf (g, kKa);
    c.cb = makeSvf (g, kKb);
    c.onePoleG = g / (1.0 + g);
    coeffLog2[(size_t) i] = curLog2[(size_t) i];
}

void Crossover::applyPendingStructureChanges() noexcept
{
    const int nb = numBandsTarget.load (std::memory_order_relaxed);
    const auto sl = (Slope) slopeTarget.load (std::memory_order_relaxed);
    const int nS = nb - 1;

    // Constrained target frequencies for the active crossovers.
    std::array<float, kMaxCrossovers> f {};
    for (int i = 0; i < nS; ++i)
        f[(size_t) i] = freqTarget[(size_t) i].load (std::memory_order_relaxed);
    constrainFrequencies (f.data(), nS, fs);
    for (int i = 0; i < nS; ++i)
        tgtLog2[(size_t) i] = std::log2 ((double) f[(size_t) i]);

    if (activeBands == 0)
    {
        // First block after prepare/reset: snap everything.
        for (int i = 0; i < nS; ++i)
        {
            curLog2[(size_t) i] = tgtLog2[(size_t) i];
            updateCoeffs (i);
        }
        activeBands = nb;
        activeSlope = sl;
        return;
    }

    if (sl != activeSlope)
    {
        const bool toLR8 = (sl == Slope::db48 && activeSlope != Slope::db48);
        const bool apKindChanged = ((sl == Slope::db12) != (activeSlope == Slope::db12));
        for (auto& cs : state)
        {
            for (auto& sp : cs.split)
            {
                if (toLR8)
                    for (int s : { 2, 3, 5, 6 })
                        sp.sec[(size_t) s] = {};
                if (activeSlope == Slope::db12) // LR2 used section 0 only
                    sp.sec[1] = sp.sec[4] = {};
            }
            for (auto& row : cs.ap)
                for (auto& a : row)
                {
                    if (apKindChanged) a = {};
                    else if (toLR8)    a.sec[1] = {};
                }
        }
        activeSlope = sl;
    }

    if (nb != activeBands)
    {
        const int oldS = activeBands - 1;
        for (int j = oldS; j < nS; ++j)  // newly activated crossovers
        {
            for (auto& cs : state)
            {
                cs.split[(size_t) j] = {};
                for (int b = 0; b < j; ++b)
                    cs.ap[(size_t) b][(size_t) j] = {};
            }
            curLog2[(size_t) j] = tgtLog2[(size_t) j];
            updateCoeffs (j);
        }
        activeBands = nb;
    }
}

void Crossover::advanceSmoothing (int len) noexcept
{
    const int nS = activeBands - 1;
    const double tauSamples = (double) smoothingMs.load (std::memory_order_relaxed) * 0.001 * fs;
    const double alpha = tauSamples <= 1.0 ? 1.0 : 1.0 - std::exp (-(double) len / tauSamples);

    for (int i = 0; i < nS; ++i)
    {
        auto& cur = curLog2[(size_t) i];
        const double tgt = tgtLog2[(size_t) i];
        if (! juce::exactlyEqual (cur, tgt))
        {
            cur += alpha * (tgt - cur);
            if (std::abs (tgt - cur) < 1.0e-5)
                cur = tgt;
        }
        if (! juce::exactlyEqual (cur, coeffLog2[(size_t) i]))
            updateCoeffs (i);
    }
}

//==============================================================================
template <int SlopeIdx>
void Crossover::processRange (int channel, const float* in, float* const* out, int start, int len) noexcept
{
    auto& cs = state[(size_t) channel];
    const int nS = activeBands - 1;

    for (int n = start; n < start + len; ++n)
    {
        double rest = in[n];

        for (int k = 0; k < nS; ++k)
        {
            const auto& c = coeffs[(size_t) k];
            auto& sec = cs.split[(size_t) k].sec;
            double lo, hi, v1, v2;

            if constexpr (SlopeIdx == 0) // LR2: Q = 0.5 SVF, high inverted
            {
                svfTick (sec[0].s1, sec[0].s2, c.c2.a1, c.c2.a2, c.c2.a3, rest, v1, v2);
                lo = v2;
                hi = -(rest - 2.0 * v1 - v2);
            }
            else if constexpr (SlopeIdx == 1) // LR4: Butterworth-2 squared
            {
                const double k4 = c.c4.k;
                svfTick (sec[0].s1, sec[0].s2, c.c4.a1, c.c4.a2, c.c4.a3, rest, v1, v2);
                const double lpA = v2, hpA = rest - k4 * v1 - v2;
                svfTick (sec[1].s1, sec[1].s2, c.c4.a1, c.c4.a2, c.c4.a3, lpA, v1, v2);
                lo = v2;
                svfTick (sec[4].s1, sec[4].s2, c.c4.a1, c.c4.a2, c.c4.a3, hpA, v1, v2);
                hi = hpA - k4 * v1 - v2;
            }
            else // LR8: Butterworth-4 squared = (Ka * Kb)^2
            {
                const auto& A = c.ca;
                const auto& B = c.cb;
                svfTick (sec[0].s1, sec[0].s2, A.a1, A.a2, A.a3, rest, v1, v2);
                double l = v2, h = rest - kKa * v1 - v2;

                svfTick (sec[1].s1, sec[1].s2, B.a1, B.a2, B.a3, l, v1, v2);  l = v2;
                svfTick (sec[2].s1, sec[2].s2, A.a1, A.a2, A.a3, l, v1, v2);  l = v2;
                svfTick (sec[3].s1, sec[3].s2, B.a1, B.a2, B.a3, l, v1, v2);  lo = v2;

                svfTick (sec[4].s1, sec[4].s2, B.a1, B.a2, B.a3, h, v1, v2);  h = h - kKb * v1 - v2;
                svfTick (sec[5].s1, sec[5].s2, A.a1, A.a2, A.a3, h, v1, v2);  h = h - kKa * v1 - v2;
                svfTick (sec[6].s1, sec[6].s2, B.a1, B.a2, B.a3, h, v1, v2);  hi = h - kKb * v1 - v2;
            }

            // Allpass compensation of this band for every higher crossover.
            auto& apRow = cs.ap[(size_t) k];
            for (int j = k + 1; j < nS; ++j)
            {
                const auto& cj = coeffs[(size_t) j];
                auto& a = apRow[(size_t) j].sec;
                if constexpr (SlopeIdx == 0)
                {
                    double& s = a[0].s1;
                    const double v = (lo - s) * cj.onePoleG;
                    const double lp = v + s;
                    s = lp + v;
                    lo = 2.0 * lp - lo;
                }
                else if constexpr (SlopeIdx == 1)
                {
                    svfTick (a[0].s1, a[0].s2, cj.c4.a1, cj.c4.a2, cj.c4.a3, lo, v1, v2);
                    lo = lo - 2.0 * kSqrt2 * v1;
                }
                else
                {
                    svfTick (a[0].s1, a[0].s2, cj.ca.a1, cj.ca.a2, cj.ca.a3, lo, v1, v2);
                    lo = lo - 2.0 * kKa * v1;
                    svfTick (a[1].s1, a[1].s2, cj.cb.a1, cj.cb.a2, cj.cb.a3, lo, v1, v2);
                    lo = lo - 2.0 * kKb * v1;
                }
            }

            out[k][n] = (float) lo;
            rest = hi;
        }

        out[nS][n] = (float) rest;
    }
}

void Crossover::process (const juce::AudioBuffer<float>& input, juce::AudioBuffer<float>* bandOutputs, int numSamples) noexcept
{
    if (state.empty() || bandOutputs == nullptr || numSamples <= 0)
        return;

    juce::ScopedNoDenormals noDenormals;
    applyPendingStructureChanges();

    const int nb = activeBands;
    const int chans = std::min (numCh, input.getNumChannels());

    if (nb == 1)
    {
        for (int ch = 0; ch < chans; ++ch)
            bandOutputs[0].copyFrom (ch, 0, input, ch, 0, numSamples);
        return;
    }

    for (int start = 0; start < numSamples; start += kCoeffUpdateInterval)
    {
        const int len = std::min (kCoeffUpdateInterval, numSamples - start);
        advanceSmoothing (len);

        for (int ch = 0; ch < chans; ++ch)
        {
            std::array<float*, kMaxBands> out {};
            for (int b = 0; b < nb; ++b)
                out[(size_t) b] = bandOutputs[b].getWritePointer (ch);
            const float* in = input.getReadPointer (ch);

            switch (activeSlope)
            {
                case Slope::db12: processRange<0> (ch, in, out.data(), start, len); break;
                case Slope::db24: processRange<1> (ch, in, out.data(), start, len); break;
                case Slope::db48: processRange<2> (ch, in, out.data(), start, len); break;
            }
        }
    }

    // Flush near-denormal states once per block (cheap insurance on platforms where
    // FTZ does not cover doubles).
    for (int ch = 0; ch < chans; ++ch)
    {
        auto& cs = state[(size_t) ch];
        for (auto& sp : cs.split)
            for (auto& s : sp.sec) { s.s1 = flushDenormal (s.s1); s.s2 = flushDenormal (s.s2); }
        for (auto& row : cs.ap)
            for (auto& a : row)
                for (auto& s : a.sec) { s.s1 = flushDenormal (s.s1); s.s2 = flushDenormal (s.s2); }
    }
}

//==============================================================================
std::complex<double> Crossover::getBandResponse (int band, double hz) const noexcept
{
    const int nb = numBandsTarget.load (std::memory_order_relaxed);
    const auto sl = (Slope) slopeTarget.load (std::memory_order_relaxed);
    const double sr = sampleRateAtomic.load (std::memory_order_relaxed);
    const int nS = nb - 1;

    if (band < 0 || band >= nb)
        return 0.0;
    if (nb == 1)
        return 1.0;

    std::array<float, kMaxCrossovers> f {};
    for (int i = 0; i < nS; ++i)
        f[(size_t) i] = freqTarget[(size_t) i].load (std::memory_order_relaxed);
    constrainFrequencies (f.data(), nS, sr);

    const double wEval = std::tan (kPi * std::clamp (hz, 0.0, 0.4999999 * sr) / sr);

    std::complex<double> h = 1.0;
    for (int j = 0; j < nS; ++j)
    {
        const double g = std::tan (kPi * (double) f[(size_t) j] / sr);
        const std::complex<double> s (0.0, wEval / g);
        const auto s2 = s * s;

        std::complex<double> lo, hi, ap;
        switch (sl)
        {
            case Slope::db12:
            {
                const auto d = s2 + 2.0 * s + 1.0;
                lo = 1.0 / d;
                hi = -s2 / d;
                ap = (1.0 - s) / (1.0 + s);
                break;
            }
            case Slope::db24:
            {
                const auto d = s2 + kSqrt2 * s + 1.0;
                lo = 1.0 / (d * d);
                hi = (s2 * s2) / (d * d);
                ap = (s2 - kSqrt2 * s + 1.0) / d;
                break;
            }
            case Slope::db48:
            {
                const auto da = s2 + kKa * s + 1.0;
                const auto db = s2 + kKb * s + 1.0;
                const auto d = da * db;
                const auto s4 = s2 * s2;
                lo = 1.0 / (d * d);
                hi = (s4 * s4) / (d * d);
                ap = ((s2 - kKa * s + 1.0) * (s2 - kKb * s + 1.0)) / d;
                break;
            }
        }

        if (j < band)       h *= hi;   // band sits above this crossover
        else if (j == band) h *= lo;   // band's own upper edge
        else                h *= ap;   // compensation allpass
    }
    return h;
}

float Crossover::getBandMagnitude (int band, float hz) const noexcept
{
    return (float) std::abs (getBandResponse (band, (double) hz));
}

} // namespace st::dsp
