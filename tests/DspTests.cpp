// ST Shell DSP tests. Console app; exit code = number of failed checks.

#include <st_shell/st_shell.h>

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

using namespace st::dsp;

namespace
{
int gChecks = 0, gFailures = 0;

void check (bool ok, const std::string& what)
{
    ++gChecks;
    if (! ok)
    {
        ++gFailures;
        std::printf ("  FAIL: %s\n", what.c_str());
    }
}

std::string fmt (const char* f, ...)
{
    char buf[512];
    va_list args;
    va_start (args, f);
    std::vsnprintf (buf, sizeof (buf), f, args);
    va_end (args);
    return buf;
}

double toDb (double mag) { return 20.0 * std::log10 (std::max (mag, 1.0e-12)); }

const char* slopeName (Crossover::Slope s)
{
    switch (s)
    {
        case Crossover::Slope::db12: return "LR2";
        case Crossover::Slope::db24: return "LR4";
        case Crossover::Slope::db48: return "LR8";
    }
    return "?";
}

/** Magnitude spectrum (N/2+1 bins) of a real signal of length N (power of two). */
std::vector<float> magnitudeSpectrum (const float* x, int order)
{
    const int n = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> buf ((size_t) (2 * n), 0.0f);
    std::copy (x, x + n, buf.begin());
    fft.performFrequencyOnlyForwardTransform (buf.data(), true);
    buf.resize ((size_t) (n / 2 + 1));
    return buf;
}

std::vector<float> crossoverFreqs (int numBands)
{
    const int nS = numBands - 1;
    std::vector<float> f;
    if (nS <= 0)
        return f;
    if (nS == 1)
        return { 1000.0f };
    // <= 5 bands: 2.5-octave spacing from 40 Hz ("wide" bands); otherwise fill 50 Hz..15 kHz.
    const double spacing = nS <= 4 ? 2.5 : std::log2 (15000.0 / 50.0) / (nS - 1);
    const double start = nS <= 4 ? 40.0 : 50.0;
    for (int i = 0; i < nS; ++i)
        f.push_back ((float) (start * std::exp2 (spacing * i)));
    return f;
}

//==============================================================================
void testCrossoverImpulse()
{
    std::printf ("\n[Crossover] impulse: sum flatness, band centres, analytic match\n");
    const Crossover::Slope slopes[] = { Crossover::Slope::db12, Crossover::Slope::db24, Crossover::Slope::db48 };

    for (double fs : { 48000.0, 384000.0 })
    {
        const int order = fs > 100000.0 ? 18 : 15;
        const int N = 1 << order;
        const int block = 512;

        std::vector<juce::AudioBuffer<float>> bands (Crossover::kMaxBands);
        for (auto& b : bands)
            b.setSize (1, N);
        juce::AudioBuffer<float> in (1, N);

        for (auto slope : slopes)
        {
            double worstSum = 0.0, worstAnalytic = 0.0;
            std::string centreLine;

            for (int nb = 1; nb <= Crossover::kMaxBands; ++nb)
            {
                Crossover xo;
                xo.setNumBands (nb);
                xo.setSlope (slope);
                const auto freqs = crossoverFreqs (nb);
                for (int i = 0; i < (int) freqs.size(); ++i)
                    xo.setCrossoverFrequency (i, freqs[(size_t) i]);
                xo.prepare (fs, block, 1);

                in.clear();
                in.setSample (0, 0, 1.0f);
                for (auto& b : bands)
                    b.clear();

                for (int start = 0; start < N; start += block)
                {
                    juce::AudioBuffer<float> inView (in.getArrayOfWritePointers(), 1, start, block);
                    std::array<juce::AudioBuffer<float>, Crossover::kMaxBands> outViews;
                    for (int b = 0; b < Crossover::kMaxBands; ++b)
                        outViews[(size_t) b] = juce::AudioBuffer<float> (bands[(size_t) b].getArrayOfWritePointers(), 1, start, block);
                    xo.process (inView, outViews.data(), block);
                }

                // Sum flatness
                std::vector<float> sum ((size_t) N, 0.0f);
                bool finite = true;
                for (int b = 0; b < nb; ++b)
                {
                    const float* d = bands[(size_t) b].getReadPointer (0);
                    for (int i = 0; i < N; ++i)
                    {
                        sum[(size_t) i] += d[i];
                        finite = finite && std::isfinite (d[i]);
                    }
                }
                check (finite, fmt ("finite output fs=%.0f %s bands=%d", fs, slopeName (slope), nb));

                const auto sumMag = magnitudeSpectrum (sum.data(), order);
                const double binHz = fs / N;
                double maxDev = 0.0;
                for (int k = (int) std::ceil (20.0 / binHz); k <= (int) (20000.0 / binHz); ++k)
                    maxDev = std::max (maxDev, std::abs (toDb (sumMag[(size_t) k])));
                worstSum = std::max (worstSum, maxDev);
                check (maxDev <= 0.05, fmt ("sum flat fs=%.0f %s bands=%d dev=%.4f dB", fs, slopeName (slope), nb, maxDev));

                // Per-band: analytic vs measured, and centre level
                double minCentreDb = 0.0;
                for (int b = 0; b < nb; ++b)
                {
                    const auto mag = magnitudeSpectrum (bands[(size_t) b].getReadPointer (0), order);
                    double maxErr = 0.0;
                    for (int p = 0; p <= 160; ++p)
                    {
                        const double fTarget = 20.0 * std::pow (1000.0, p / 160.0);
                        const int k = std::max (1, (int) std::lround (fTarget / binHz));
                        const double fBin = k * binHz;
                        const double analytic = xo.getBandMagnitude (b, (float) fBin);
                        if (toDb (analytic) < -60.0)
                            continue;
                        maxErr = std::max (maxErr, std::abs (toDb (mag[(size_t) k]) - toDb (analytic)));
                    }
                    worstAnalytic = std::max (worstAnalytic, maxErr);
                    check (maxErr <= 0.2, fmt ("analytic match fs=%.0f %s bands=%d band=%d err=%.4f dB",
                                               fs, slopeName (slope), nb, b, maxErr));

                    // Edge bands: centre half a band-width beyond the outermost crossover.
                    const double ratio = nb > 2 ? (double) freqs[1] / freqs[0] : 8.0;
                    double centre;
                    if (nb == 1)           centre = 1000.0;
                    else if (b == 0)            centre = freqs[0] / std::sqrt (ratio);
                    else if (b == nb - 1)  centre = std::min (20000.0, freqs[(size_t) b - 1] * std::sqrt (ratio));
                    else                   centre = std::sqrt ((double) freqs[(size_t) b - 1] * freqs[(size_t) b]);
                    const int k = (int) std::lround (centre / binHz);
                    const double measured = toDb (mag[(size_t) k]);
                    const double analytic = toDb (xo.getBandMagnitude (b, (float) (k * binHz)));
                    check (std::abs (measured - analytic) <= 0.2,
                           fmt ("centre analytic fs=%.0f %s bands=%d band=%d meas=%.3f ana=%.3f",
                                fs, slopeName (slope), nb, b, measured, analytic));
                    minCentreDb = std::min (minCentreDb, measured);

                    if (nb <= 5) // wide (2.5 octave) bands: centre must be ~0 dB
                    {
                        // Theory for 2.5-octave bands: LR8 ~-0.01, LR4 ~-0.53, LR2 ~-2.83 dB
                        // (LR2's 12 dB/oct skirts overlap a lot; it's inherent, not an error).
                        const double tol = slope == Crossover::Slope::db48 ? 0.15 : slope == Crossover::Slope::db24 ? 0.6 : 3.0;
                        check (measured >= -tol && measured <= 0.05,
                               fmt ("centre ~0 dB fs=%.0f %s bands=%d band=%d level=%.3f dB (tol %.2f)",
                                    fs, slopeName (slope), nb, b, measured, tol));
                    }
                }
                centreLine += fmt (" %d:%.2f", nb, minCentreDb);
            }

            std::printf ("  fs=%6.0f %s  worst sum deviation %.5f dB | worst analytic err %.4f dB | min centre dB per band count:%s\n",
                         fs, slopeName (slope), worstSum, worstAnalytic, centreLine.c_str());
        }
    }
}

//==============================================================================
struct SweepResult { std::array<float, Crossover::kMaxBands> maxJump {}; float sumJump = 0; float sumErr = 0; bool finite = true; };

SweepResult runSweep (Crossover::Slope slope, int mode /*0 static lo, 1 static hi, 2 sweep, 3 jumps*/,
                      const juce::AudioBuffer<float>& input, double fs, int nb)
{
    const int block = 64, chans = input.getNumChannels(), total = input.getNumSamples();
    Crossover xo;
    xo.setNumBands (nb);
    xo.setSlope (slope);
    xo.setCrossoverFrequency (0, 120.0f);
    xo.setCrossoverFrequency (1, mode == 1 ? 6000.0f : 300.0f);
    xo.setCrossoverFrequency (2, 9000.0f);
    xo.prepare (fs, block, chans);

    std::vector<juce::AudioBuffer<float>> bands (Crossover::kMaxBands);
    for (auto& b : bands)
        b.setSize (chans, block);

    SweepResult r;
    std::array<std::array<float, 2>, Crossover::kMaxBands> prev {};
    std::array<float, 2> prevSum {};
    juce::AudioBuffer<float> blk (chans, block);
    const int skip = (int) (0.05 * fs); // ignore start-up

    for (int start = 0; start + block <= total; start += block)
    {
        const double t = start / fs;
        if (mode == 2) // fast log sweep 300 Hz <-> 6 kHz, 8 Hz triangle
        {
            const double ph = std::fmod (t * 8.0, 1.0);
            const double tri = ph < 0.5 ? ph * 2.0 : 2.0 - ph * 2.0;
            xo.setCrossoverFrequency (1, (float) (300.0 * std::pow (20.0, tri)));
        }
        else if (mode == 3) // instantaneous target jumps every 50 ms
        {
            xo.setCrossoverFrequency (1, ((int) (t / 0.05) % 2) == 0 ? 300.0f : 6000.0f);
        }

        for (int ch = 0; ch < chans; ++ch)
            blk.copyFrom (ch, 0, input, ch, start, block);
        xo.process (blk, bands.data(), block);

        for (int ch = 0; ch < chans; ++ch)
        {
            for (int i = 0; i < block; ++i)
            {
                float s = 0.0f;
                for (int b = 0; b < nb; ++b)
                {
                    const float v = bands[(size_t) b].getSample (ch, i);
                    r.finite = r.finite && std::isfinite (v);
                    if (start + i > skip)
                        r.maxJump[(size_t) b] = std::max (r.maxJump[(size_t) b], std::abs (v - prev[(size_t) b][(size_t) ch]));
                    prev[(size_t) b][(size_t) ch] = v;
                    s += v;
                }
                if (start + i > skip)
                    r.sumJump = std::max (r.sumJump, std::abs (s - prevSum[(size_t) ch]));
                prevSum[(size_t) ch] = s;
            }
        }
    }
    return r;
}

void testCrossoverSweep()
{
    std::printf ("\n[Crossover] fast crossover sweep with noise (max sample-to-sample jump vs static)\n");
    const double fs = 48000.0;
    const int total = (int) (fs * 1.0);
    juce::AudioBuffer<float> noise (2, total);
    juce::Random rng (1234);
    for (int ch = 0; ch < 2; ++ch)
    {
        float lp = 0.0f;
        for (int i = 0; i < total; ++i)
        {
            // gently low-passed noise (one-pole @ ~1 kHz) so discontinuities stand out
            lp += 0.12f * ((rng.nextFloat() * 2.0f - 1.0f) - lp);
            noise.setSample (ch, i, 0.5f * lp);
        }
    }
    float inJump = 0.0f;
    for (int i = 1; i < total; ++i)
        inJump = std::max (inJump, std::abs (noise.getSample (0, i) - noise.getSample (0, i - 1)));

    const int nb = 4;
    for (auto slope : { Crossover::Slope::db12, Crossover::Slope::db24, Crossover::Slope::db48 })
    {
        const auto sLo = runSweep (slope, 0, noise, fs, nb);
        const auto sHi = runSweep (slope, 1, noise, fs, nb);
        for (int mode : { 2, 3 })
        {
            const auto sw = runSweep (slope, mode, noise, fs, nb);
            check (sw.finite, fmt ("sweep finite %s mode %d", slopeName (slope), mode));
            std::string line;
            float worstRatio = 0.0f;
            for (int b = 0; b < nb; ++b)
            {
                const float ref = std::max (sLo.maxJump[(size_t) b], sHi.maxJump[(size_t) b]);
                const float ratio = sw.maxJump[(size_t) b] / std::max (ref, 1.0e-9f);
                worstRatio = std::max (worstRatio, ratio);
                line += fmt (" b%d %.3f/%.3f", b, sw.maxJump[(size_t) b], ref);
                check (ratio <= 1.5f, fmt ("sweep jump %s mode %d band %d ratio %.3f", slopeName (slope), mode, b, ratio));
            }
            const float sumRatio = sw.sumJump / std::max (sLo.sumJump, sHi.sumJump);
            check (sumRatio <= 1.25f, fmt ("sweep sum jump %s mode %d ratio %.3f", slopeName (slope), mode, sumRatio));
            std::printf ("  %s %-6s worst band ratio %.3f, sum ratio %.3f (input jump %.3f) |%s\n",
                         slopeName (slope), mode == 2 ? "sweep" : "jumps", worstRatio, sumRatio, inJump, line.c_str());
        }
    }

    // Band-count / slope changes mid-stream must stay finite and bounded.
    {
        Crossover xo;
        xo.prepare (fs, 256, 2);
        std::vector<juce::AudioBuffer<float>> bands (Crossover::kMaxBands);
        for (auto& b : bands)
            b.setSize (2, 256);
        juce::AudioBuffer<float> blk (2, 256);
        bool ok = true;
        float peak = 0.0f;
        for (int start = 0, iter = 0; start + 256 <= total; start += 256, ++iter)
        {
            xo.setNumBands (1 + (iter * 3) % 8);
            xo.setSlope ((Crossover::Slope) ((iter / 5) % 3));
            for (int ch = 0; ch < 2; ++ch)
                blk.copyFrom (ch, 0, noise, ch, start, 256);
            xo.process (blk, bands.data(), 256);
            for (int b = 0; b < xo.getNumBands(); ++b)
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 256; ++i)
                    {
                        const float v = bands[(size_t) b].getSample (ch, i);
                        ok = ok && std::isfinite (v);
                        peak = std::max (peak, std::abs (v));
                    }
        }
        check (ok && peak < 4.0f, fmt ("band-count/slope switching finite & bounded (peak %.3f)", peak));
        std::printf ("  band-count/slope switching: finite=%d peak=%.3f\n", (int) ok, peak);
    }
}

//==============================================================================
double measureCompressorGainDb (UpDownCompressor::Params p, float amplitude, double fs, float freq = 1000.0f)
{
    UpDownCompressor comp;
    comp.setParams (p);
    comp.prepare (fs, 512, 2);
    const int total = (int) (fs * 1.0), block = 512;
    juce::AudioBuffer<float> buf (2, block);
    double sumIn = 0.0, sumOut = 0.0;
    const int measureFrom = total - (int) (0.25 * fs);
    for (int start = 0; start < total; start += block)
    {
        for (int i = 0; i < block; ++i)
        {
            const float x = amplitude * (float) std::sin (kTwoPi * freq * (start + i) / fs);
            buf.setSample (0, i, x);
            buf.setSample (1, i, x);
        }
        juce::AudioBuffer<float> copy (buf);
        comp.process (buf, block);
        if (start >= measureFrom)
            for (int i = 0; i < block; ++i)
            {
                sumIn += (double) copy.getSample (0, i) * copy.getSample (0, i);
                sumOut += (double) buf.getSample (0, i) * buf.getSample (0, i);
            }
    }
    return 10.0 * std::log10 (sumOut / sumIn);
}

void testCompressor()
{
    std::printf ("\n[UpDownCompressor] static curve vs measured steady-state gain\n");
    const double fs = 48000.0;

    UpDownCompressor::Params p;
    p.downThresholdDb = -20.0f; p.downRatio = 4.0f;
    p.upThresholdDb = -40.0f;   p.upRatio = 3.0f;
    p.kneeDb = 6.0f; p.maxUpwardGainDb = 24.0f;
    p.attackMs = 5.0f; p.releaseMs = 50.0f;
    p.rmsBlend = 1.0f; p.rmsWindowMs = 10.0f;
    p.depth = 1.0f;

    struct Mode { const char* name; float blend; float attack, release; double tol; double detOffsetDb; };
    const Mode modes[] = { { "RMS ", 1.0f, 5.0f, 50.0f, 0.1, -3.0103 },
                           { "peak", 0.0f, 0.2f, 500.0f, 0.3, 0.0 } };

    for (const auto& m : modes)
    {
        p.rmsBlend = m.blend; p.attackMs = m.attack; p.releaseMs = m.release;
        double worst = 0.0;
        std::string line;
        for (float levelDb = -84.0f; levelDb <= 0.0f; levelDb += 6.0f)
        {
            const float amp = dbToGain (levelDb);
            const double measured = measureCompressorGainDb (p, amp, fs);
            const double expected = UpDownCompressor::computeStaticGainDb ((float) (levelDb + m.detOffsetDb), p);
            worst = std::max (worst, std::abs (measured - expected));
            check (std::abs (measured - expected) <= m.tol,
                   fmt ("comp %s level %.0f dBFS measured %.3f expected %.3f", m.name, levelDb, measured, expected));
            line += fmt (" %.0f:%+.2f/%+.2f", levelDb, measured, expected);
        }
        std::printf ("  %s worst |measured-static| = %.4f dB (tol %.2f)\n    level:meas/static%s\n", m.name, worst, m.tol, line.c_str());
    }

    // Depth mirror
    {
        p.rmsBlend = 1.0f; p.attackMs = 5.0f; p.releaseMs = 50.0f;
        double worst = 0.0, worstStatic = 0.0;
        for (float levelDb = -72.0f; levelDb <= 0.0f; levelDb += 12.0f)
        {
            auto pPos = p, pNeg = p;
            pPos.depth = 1.0f; pNeg.depth = -1.0f;
            const double gPos = measureCompressorGainDb (pPos, dbToGain (levelDb), fs);
            const double gNeg = measureCompressorGainDb (pNeg, dbToGain (levelDb), fs);
            worst = std::max (worst, std::abs (gPos + gNeg));
            for (float x = -100.0f; x <= 10.0f; x += 0.5f)
                worstStatic = std::max (worstStatic, (double) std::abs (UpDownCompressor::computeStaticGainDb (x, pPos)
                                                                        + UpDownCompressor::computeStaticGainDb (x, pNeg)));
        }
        check (worst <= 0.1, fmt ("depth -1 mirrors +1 (measured) worst %.4f dB", worst));
        check (worstStatic <= 1.0e-5, fmt ("depth -1 mirrors +1 (static) worst %.6f dB", worstStatic));
        std::printf ("  depth mirror: measured worst |g(+1)+g(-1)| = %.4f dB, static %.2e dB\n", worst, worstStatic);
    }

    // Upward floor: smooth fade, no gain on silence-level signals, cap honoured.
    {
        UpDownCompressor::Params q;
        q.upThresholdDb = -30.0f; q.upRatio = 100.0f; q.maxUpwardGainDb = 24.0f;
        float prev = UpDownCompressor::computeStaticGainDb (-120.0f, q);
        float maxStep = 0.0f, maxGain = 0.0f;
        for (float x = -120.0f; x <= 0.0f; x += 0.1f)
        {
            const float g = UpDownCompressor::computeStaticGainDb (x, q);
            maxStep = std::max (maxStep, std::abs (g - prev));
            maxGain = std::max (maxGain, g);
            prev = g;
        }
        check (UpDownCompressor::computeStaticGainDb (-85.0f, q) == 0.0f, "no upward gain below floor");
        check (maxGain <= 24.0f + 1e-4f, fmt ("upward cap honoured (max %.3f)", maxGain));
        check (maxStep < 0.6f, fmt ("upward floor fade smooth (max step %.3f dB per 0.1 dB)", maxStep));
        std::printf ("  upward floor/cap: max gain %.2f dB, max step %.3f dB per 0.1 dB input\n", maxGain, maxStep);
    }

    // Meters & automation smoothness
    {
        UpDownCompressor comp;
        comp.prepare (fs, 256, 2);
        juce::AudioBuffer<float> buf (2, 256);
        float maxJump = 0.0f, prev = 0.0f;
        for (int blk = 0; blk < 200; ++blk)
        {
            auto q = p;
            q.downThresholdDb = (blk / 20) % 2 == 1 ? -30.0f : -6.0f; // abrupt automation
            comp.setParams (q);
            for (int i = 0; i < 256; ++i)
            {
                const float x = 0.5f * (float) std::sin (kTwoPi * 100.0 * (blk * 256 + i) / fs);
                buf.setSample (0, i, x); buf.setSample (1, i, x);
            }
            comp.process (buf, 256);
            for (int i = 0; i < 256; ++i)
            {
                const float y = buf.getSample (0, i);
                if (blk > 4) maxJump = std::max (maxJump, std::abs (y - prev));
                prev = y;
            }
        }
        const float sineMaxStep = 0.5f * (float) (kTwoPi * 100.0 / fs);
        check (maxJump < sineMaxStep * 2.0f, fmt ("threshold automation zipper-free (max step %.5f vs sine %.5f)", maxJump, sineMaxStep));
        check (comp.getGainChangeDb() < 0.0f && comp.getDetectorLevelDb() > -20.0f, "meters updated");
        std::printf ("  automation: max step %.5f (sine slope %.5f); meters gain %.2f dB level %.2f dB\n",
                     maxJump, sineMaxStep, comp.getGainChangeDb(), comp.getDetectorLevelDb());
    }
}

//==============================================================================
void benchmark()
{
    std::printf ("\n[Benchmark] (informational)\n");
    const double fs = 768000.0;
    const int block = 512 * 16, chans = 2, total = (int) (fs * 0.25);
    juce::AudioBuffer<float> in (chans, block);
    juce::Random rng (7);
    for (int ch = 0; ch < chans; ++ch)
        for (int i = 0; i < block; ++i)
            in.setSample (ch, i, rng.nextFloat() - 0.5f);
    std::vector<juce::AudioBuffer<float>> bands (Crossover::kMaxBands);
    for (auto& b : bands)
        b.setSize (chans, block);

    for (auto slope : { Crossover::Slope::db24, Crossover::Slope::db48 })
    {
        Crossover xo;
        xo.setNumBands (8);
        xo.setSlope (slope);
        for (int i = 0; i < 7; ++i)
            xo.setCrossoverFrequency (i, 60.0f * std::pow (2.0f, 1.3f * (float) i));
        xo.prepare (fs, block, chans);
        const auto t0 = std::chrono::steady_clock::now();
        for (int done = 0; done < total; done += block)
        {
            xo.setCrossoverFrequency (3, 900.0f + (float) (done % 7));
            xo.process (in, bands.data(), block);
        }
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("  Crossover 8 bands %s stereo @768 kHz: %.1f%% of real time (one core)\n", slopeName (slope), 100.0 * secs / 0.25);
    }

    {
        UpDownCompressor comp;
        comp.prepare (fs, block, chans);
        juce::AudioBuffer<float> buf (chans, block);
        const auto t0 = std::chrono::steady_clock::now();
        for (int done = 0; done < total; done += block)
        {
            buf.makeCopyOf (in, true);
            comp.process (buf, block);
        }
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("  UpDownCompressor stereo @768 kHz: %.1f%% of real time per band\n", 100.0 * secs / 0.25);
    }
}

//==============================================================================
void testOversampler()
{
    std::printf ("\n[Oversampler] reported latency vs impulse response peak\n");
    Oversampler os;
    const int block = 256, chans = 2;
    os.prepare (48000.0, block, chans);

    for (int idx = 0; idx < Oversampler::kNumFactors; ++idx)
    {
        os.setFactorIndex (idx);
        os.reset();
        const int total = block * 8, impulseAt = 300;
        std::vector<float> out ((size_t) total, 0.0f);
        juce::AudioBuffer<float> buf (chans, block);
        bool finite = true;
        for (int start = 0; start < total; start += block)
        {
            buf.clear();
            if (impulseAt >= start && impulseAt < start + block)
                for (int ch = 0; ch < chans; ++ch)
                    buf.setSample (ch, impulseAt - start, 1.0f);
            juce::dsp::AudioBlock<float> blk (buf);
            auto up = os.processUp (blk);
            check ((int) up.getNumSamples() == block * os.getFactor(), "upsampled block length");
            os.processDown (blk);
            for (int i = 0; i < block; ++i)
            {
                out[(size_t) (start + i)] = buf.getSample (0, i);
                finite = finite && std::isfinite (buf.getSample (1, i));
            }
        }
        int peakIdx = 0;
        for (int i = 0; i < total; ++i)
            if (std::abs (out[(size_t) i]) > std::abs (out[(size_t) peakIdx]))
                peakIdx = i;
        const int measured = peakIdx - impulseAt;
        const int reported = os.getLatencySamples();
        check (finite && measured == reported && reported == os.getLatencySamples (idx),
               fmt ("oversampler %dx latency reported %d measured %d", os.getFactor(), reported, measured));
        std::printf ("  %2dx: reported %3d, measured %3d, peak %.3f\n", os.getFactor(), reported, measured, out[(size_t) peakIdx]);
    }
}

//==============================================================================
void testFiltersAndUtils()
{
    std::printf ("\n[Filters/Utils] sanity checks\n");
    const double fs = 48000.0;

    // Biquad + Svf measured vs analytic on a few designs (sine steady state).
    auto measure = [&] (auto&& processFn, double f)
    {
        double si = 0, so = 0;
        const int total = (int) fs / 2;
        for (int i = 0; i < total; ++i)
        {
            const float x = (float) std::sin (kTwoPi * f * i / fs);
            const float y = processFn (x);
            if (i > total / 2) { si += x * x; so += y * y; }
        }
        return 10.0 * std::log10 (so / si);
    };

    double worstBq = 0, worstSvf = 0;
    const BiquadType bqTypes[] = { BiquadType::lowPass, BiquadType::highPass, BiquadType::peak, BiquadType::lowShelf, BiquadType::highShelf, BiquadType::bandPass };
    const SvfMode svfModes[] = { SvfMode::lowPass, SvfMode::highPass, SvfMode::bell, SvfMode::lowShelf, SvfMode::highShelf, SvfMode::bandPass };
    for (int t = 0; t < 6; ++t)
        for (double f : { 100.0, 1000.0, 7000.0 })
        {
            Biquad bq; bq.prepare (1); bq.setParameters (bqTypes[t], fs, 1000.0, 0.9, 6.0);
            const double mb = measure ([&] (float x) { return bq.processSample (0, x); }, f);
            worstBq = std::max (worstBq, std::abs (mb - toDb (Biquad::getMagnitude (bqTypes[t], fs, 1000.0, 0.9, 6.0, f))));

            Svf sv; sv.prepare (fs, 1); sv.setParameters (svfModes[t], 1000.0, 0.9, 6.0);
            const double ms = measure ([&] (float x) { return sv.processSample (0, x); }, f);
            worstSvf = std::max (worstSvf, std::abs (ms - toDb (Svf::getMagnitude (svfModes[t], fs, 1000.0, 0.9, 6.0, f))));
        }
    check (worstBq < 0.05, fmt ("biquad analytic response (err %.4f dB)", worstBq));
    check (worstSvf < 0.05, fmt ("svf analytic response (err %.4f dB)", worstSvf));

    // Svf bell/shelf gains at their characteristic points
    check (std::abs (toDb (Svf::getMagnitude (SvfMode::bell, fs, 1000.0, 1.0, 6.0, 1000.0)) - 6.0) < 1e-6, "svf bell gain at centre");
    check (std::abs (toDb (Svf::getMagnitude (SvfMode::lowShelf, fs, 1000.0, 0.707, 6.0, 5.0)) - 6.0) < 0.01, "svf low shelf gain");

    // Tilt: 0 dB at pivot, +-9 dB at the extremes.
    const double atPivot = toDb (std::abs (TiltFilter::getResponse (fs, 1000.0, 1.0f, 9.0f, 1000.0)));
    const double lowEnd = toDb (std::abs (TiltFilter::getResponse (fs, 1000.0, 1.0f, 9.0f, 5.0)));
    const double highEnd = toDb (std::abs (TiltFilter::getResponse (fs, 1000.0, 1.0f, 9.0f, 23900.0)));
    check (std::abs (atPivot) < 1e-6 && std::abs (lowEnd + 9.0) < 0.05 && highEnd > 8.0,
           fmt ("tilt pivot %.4f low %.3f high %.3f dB", atPivot, lowEnd, highEnd));

    // Tilt measured + modulation zipper check
    {
        TiltFilter tilt; tilt.prepare (fs, 1); tilt.setTiltImmediate (-1.0f);
        juce::AudioBuffer<float> b (1, 64);
        float maxJump = 0, prev = 0;
        for (int blk = 0; blk < 750; ++blk)
        {
            tilt.setTilt ((blk / 50) % 2 == 0 ? -1.0f : 1.0f);
            for (int i = 0; i < 64; ++i)
                b.setSample (0, i, 0.5f * (float) std::sin (kTwoPi * 200.0 * (blk * 64 + i) / fs));
            tilt.process (b, 64);
            for (int i = 0; i < 64; ++i)
            {
                maxJump = std::max (maxJump, std::abs (b.getSample (0, i) - prev));
                prev = b.getSample (0, i);
            }
        }
        const float sineStep = 0.5f * (float) (kTwoPi * 200.0 / fs) * dbToGain (9.0f);
        check (maxJump <= sineStep * 1.2f, fmt ("tilt modulation zipper-free (max step %.5f vs %.5f)", maxJump, sineStep));
    }

    // DelayLine
    {
        DelayLine d; d.prepare (2, 1000); d.setDelay (37);
        juce::AudioBuffer<float> b (2, 128);
        int found = -1;
        for (int blk = 0; blk < 2; ++blk)
        {
            b.clear();
            if (blk == 0) { b.setSample (0, 5, 1.0f); b.setSample (1, 5, 1.0f); }
            d.process (b, 128);
            for (int i = 0; i < 128; ++i)
                if (b.getSample (0, i) == 1.0f && b.getSample (1, i) == 1.0f) found = blk * 128 + i;
        }
        check (found == 42, fmt ("delay line (found impulse at %d, expected 42)", found));
    }

    // Utils
    {
        juce::AudioBuffer<float> b (2, 16);
        b.clear();
        b.setSample (1, 3, std::numeric_limits<float>::quiet_NaN());
        b.setSample (0, 7, std::numeric_limits<float>::infinity());
        const bool wasFinite = isFiniteBuffer (b, 16);
        check (! wasFinite && isFiniteBuffer (static_cast<const juce::AudioBuffer<float>&> (b), 16), "isFiniteBuffer sanitises");

        double worst = 0;
        for (float db = -120.0f; db <= 24.0f; db += 0.37f)
        {
            worst = std::max (worst, (double) std::abs (fastGainToDb (fastDbToGain (db)) - db));
            worst = std::max (worst, (double) std::abs (fastDbToGain (db) / dbToGain (db) - 1.0f) * 8.7);
        }
        check (worst < 1e-3, fmt ("fast dB conversions (err %.2e dB)", worst));

        const auto [l, r] = equalPowerPan (0.0f);
        check (std::abs (l - r) < 1e-6f && std::abs (l * l + r * r - 1.0f) < 1e-6f, "equal power pan");

        float m, s, l2, r2;
        encodeMidSide (0.3f, -0.7f, m, s);
        decodeMidSide (m, s, l2, r2);
        check (std::abs (l2 - 0.3f) < 1e-7f && std::abs (r2 + 0.7f) < 1e-7f, "mid/side round trip");

        LinearSmoother sm; sm.prepare (1000.0, 10.0); sm.setCurrentAndTarget (0.0f); sm.setTarget (1.0f);
        float v = 0;
        for (int i = 0; i < 10; ++i) v = sm.getNext();
        check (v == 1.0f && ! sm.isSmoothing(), "linear smoother reaches target");

        EnvelopeFollower ef; ef.prepare (fs, 1); ef.setAttackMs (1.0f); ef.setReleaseMs (50.0f); ef.setRmsBlend (1.0f); ef.setRmsWindowMs (20.0f);
        float e = 0;
        for (int i = 0; i < (int) fs; ++i)
            e = ef.processSample (0, 0.5f * (float) std::sin (kTwoPi * 1000.0 * i / fs));
        check (std::abs (gainToDb (e) - gainToDb (0.5f / std::sqrt (2.0f))) < 0.2f, fmt ("envelope RMS (%.3f dB)", gainToDb (e)));
    }

    std::printf ("  biquad err %.4f dB, svf err %.4f dB, tilt pivot %.2e / low %.3f / high %.3f dB\n",
                 worstBq, worstSvf, atPivot, lowEnd, highEnd);
}
} // namespace

int main()
{
    const auto t0 = std::chrono::steady_clock::now();
    juce::ScopedNoDenormals noDenormals;

    testFiltersAndUtils();
    testCrossoverImpulse();
    testCrossoverSweep();
    testCompressor();
    testOversampler();
    benchmark();

    const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    std::printf ("\n%d checks, %d failures (%.1f s)\n", gChecks, gFailures, secs);
    return gFailures == 0 ? 0 : 1;
}
