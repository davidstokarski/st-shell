// ST Shell reverb tests. Console app; exit code = number of failed checks.
//
//   StReverbTests                 run all checks (RT60, density, ringing, mono, level, clicks,
//                                 rate factors, stability, silence, CPU) and print tables
//   StReverbTests --render <dir>  additionally write 24-bit WAVs of every type on a generated
//                                 drum loop and a sustained chord, plus objective analysis
//   StReverbTests --quick         skip the long (60 s) stability runs
//   StReverbTests --diag          ringing-metric diagnostics (peaks vs size / modulation)

#include <st_shell/st_shell.h>

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

using namespace st::dsp;
using RType = Reverb::Type;

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
    char buf[1024];
    va_list args;
    va_start (args, f);
    std::vsnprintf (buf, sizeof (buf), f, args);
    va_end (args);
    return buf;
}

constexpr double kBase = 48000.0;
const RType kTypes[] = { RType::room, RType::chamber, RType::plate, RType::hall,
                         RType::cathedral, RType::spring, RType::ambience, RType::bloom };

const char* typeName (RType t) { static auto names = Reverb::getTypeNames(); static std::string s[8];
                                 s[(int) t] = names[(int) t].toStdString(); return s[(int) t].c_str(); }

struct Stereo
{
    std::vector<float> l, r;
    size_t size() const { return l.size(); }
};

Reverb::Params makeParams (RType t, float decay, float size = 0.5f, float pd = 0.0f, float mod = 0.3f,
                           float tone = 0.0f, float width = 1.0f)
{
    Reverb::Params p;
    p.type = t; p.decaySeconds = decay; p.size = size; p.predelayMs = pd;
    p.modulation = mod; p.tone = tone; p.width = width;
    return p;
}

/** Runs the reverb over in -> returns wet output. paramsAt (sample) may change params per block. */
template <typename F>
Stereo runReverb (Reverb& rv, const Stereo& in, int block, F&& perBlock)
{
    Stereo out;
    out.l.assign (in.size(), 0.0f);
    out.r.assign (in.size(), 0.0f);
    for (size_t i = 0; i < in.size(); i += (size_t) block)
    {
        const int n = (int) std::min ((size_t) block, in.size() - i);
        perBlock ((int) i);
        rv.process (in.l.data() + i, in.r.data() + i, out.l.data() + i, out.r.data() + i, n);
    }
    return out;
}

Stereo runReverb (Reverb& rv, const Stereo& in, int block = 256)
{
    return runReverb (rv, in, block, [] (int) {});
}

Stereo impulseResponse (const Reverb::Params& p, double seconds, int R = 1, double base = kBase)
{
    Reverb rv;
    rv.prepare (base * R, R, 512);
    rv.setParams (p);
    rv.reset();
    Stereo in;
    const size_t n = (size_t) (seconds * base * R);
    in.l.assign (n, 0.0f);
    in.r.assign (n, 0.0f);
    in.l[0] = in.r[0] = (float) R; // same base-band energy at every rate
    return runReverb (rv, in);
}

bool allFinite (const Stereo& s)
{
    for (size_t i = 0; i < s.size(); ++i)
        if (! std::isfinite (s.l[i]) || ! std::isfinite (s.r[i]))
            return false;
    return true;
}

double peakAbs (const Stereo& s, size_t from = 0, size_t to = SIZE_MAX)
{
    double p = 0;
    for (size_t i = from; i < std::min (to, s.size()); ++i)
        p = std::max (p, (double) std::max (std::abs (s.l[i]), std::abs (s.r[i])));
    return p;
}

double rms (const std::vector<float>& x, size_t from = 0, size_t to = SIZE_MAX)
{
    to = std::min (to, x.size());
    double e = 0;
    for (size_t i = from; i < to; ++i) e += (double) x[i] * x[i];
    return std::sqrt (e / (double) std::max<size_t> (1, to - from));
}

double toDb (double x) { return 20.0 * std::log10 (std::max (x, 1.0e-12)); }

/** Band-limited (LR4 high-pass lo, LR4 low-pass hi) energy envelope l^2 + r^2. */
std::vector<double> bandEnergy (const Stereo& s, double fs, double lo, double hi)
{
    Biquad f[4];
    for (int i = 0; i < 4; ++i)
    {
        f[i].prepare (2);
        f[i].setParameters (i < 2 ? BiquadType::highPass : BiquadType::lowPass, fs, i < 2 ? lo : hi, 0.70710678);
    }
    std::vector<double> e (s.size());
    for (size_t n = 0; n < s.size(); ++n)
    {
        float l = s.l[n], r = s.r[n];
        for (auto& b : f) { l = b.processSample (0, l); r = b.processSample (1, r); }
        e[n] = (double) l * l + (double) r * r;
    }
    return e;
}

/** Schroeder backward integration + linear fit between hiDb and loDb. Returns RT60 (s), -1 if range not reached. */
double rt60 (const std::vector<double>& e, double fs, double hiDb = -5.0, double loDb = -35.0)
{
    std::vector<double> edc (e.size());
    double acc = 0;
    for (size_t i = e.size(); i-- > 0;) { acc += e[i]; edc[i] = acc; }
    if (acc <= 0) return -1;
    size_t i0 = SIZE_MAX, i1 = SIZE_MAX;
    for (size_t i = 0; i < edc.size(); ++i)
    {
        const double db = 10.0 * std::log10 (std::max (edc[i] / acc, 1.0e-30));
        if (i0 == SIZE_MAX && db <= hiDb) i0 = i;
        if (db <= loDb) { i1 = i; break; }
    }
    if (i0 == SIZE_MAX || i1 == SIZE_MAX || i1 <= i0 + 10) return -1;
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    int cnt = 0;
    const size_t step = std::max<size_t> (1, (i1 - i0) / 2000);
    for (size_t i = i0; i <= i1; i += step)
    {
        const double x = (double) i / fs, y = 10.0 * std::log10 (std::max (edc[i] / acc, 1.0e-30));
        sx += x; sy += y; sxx += x * x; sxy += x * y; ++cnt;
    }
    const double slope = (cnt * sxy - sx * sy) / (cnt * sxx - sx * sx);
    return slope < 0 ? -60.0 / slope : -1;
}

/** Mean normalised echo density (Abel & Huang) over [t0, t1], 20 ms windows. ~1 = Gaussian-dense. */
double echoDensity (const std::vector<float>& h, double fs, double t0, double t1, double* minOut = nullptr)
{
    const int win = (int) (0.02 * fs);
    double sum = 0, mn = 1e9;
    int cnt = 0;
    for (double t = t0; t <= t1; t += 0.01)
    {
        const size_t c = (size_t) (t * fs);
        if (c + (size_t) win / 2 >= h.size() || c < (size_t) win / 2) break;
        double e = 0;
        for (size_t i = c - win / 2; i < c + win / 2; ++i) e += (double) h[i] * h[i];
        const double sigma = std::sqrt (e / win);
        if (sigma <= 0) continue;
        int over = 0;
        for (size_t i = c - win / 2; i < c + win / 2; ++i) over += std::abs (h[i]) > sigma;
        const double ned = ((double) over / win) / 0.3173105;
        sum += ned; mn = std::min (mn, ned); ++cnt;
    }
    if (minOut) *minOut = mn;
    return cnt ? sum / cnt : 0;
}

/** Flutter metric: autocorrelation of the decay-flattened energy envelope (1 ms smoothing) over
    [t0, t1]; returns the largest normalised peak at lags 4..80 ms. Periodic echoes -> high values. */
double flutter (const Stereo& ir, double fs, double T, double t0, double t1)
{
    const int hop = (int) (0.001 * fs);
    std::vector<double> env;
    for (size_t i = (size_t) (t0 * fs); i + (size_t) hop < std::min (ir.size(), (size_t) (t1 * fs)); i += (size_t) hop)
    {
        double e = 0;
        for (size_t j = i; j < i + (size_t) hop; ++j) e += ir.l[j] * ir.l[j] + ir.r[j] * ir.r[j];
        env.push_back (e);
    }
    juce::ignoreUnused (T);
    // Detrend: divide by a centred 101 ms moving average (removes decay, build-up and swell).
    std::vector<double> fl (env.size(), 0.0);
    for (size_t i = 0; i < env.size(); ++i)
    {
        const size_t a = i >= 50 ? i - 50 : 0, b = std::min (env.size(), i + 51);
        double m = 0;
        for (size_t j = a; j < b; ++j) m += env[j];
        m /= (double) (b - a);
        fl[i] = m > 0 ? env[i] / m - 1.0 : 0.0;
    }
    env.swap (fl);
    double r0 = 0;
    for (double v : env) r0 += v * v;
    double best = 0;
    for (size_t lag = 4; lag <= 60 && lag < env.size() / 2; ++lag)
    {
        double r = 0;
        for (size_t i = 0; i + lag < env.size(); ++i) r += env[i] * env[i + lag];
        best = std::max (best, r / r0);
    }
    return best;
}

/** Magnitude-squared spectrum of x (zero padded to 2^order). */
std::vector<double> powerSpectrum (const float* x, size_t n, int order, const std::vector<float>* window = nullptr)
{
    const int N = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> buf ((size_t) (2 * N), 0.0f);
    for (size_t i = 0; i < std::min (n, (size_t) N); ++i)
        buf[i] = x[i] * (window ? (*window)[i] : 1.0f);
    fft.performFrequencyOnlyForwardTransform (buf.data(), true);
    std::vector<double> p ((size_t) (N / 2 + 1));
    for (size_t i = 0; i < p.size(); ++i) p[i] = (double) buf[i] * buf[i];
    return p;
}

bool gRingDump = false;

/** Tonal-ringing metric: Welch PSD of the decay-flattened tail; max excess (dB) over the local
    third-octave median, 200 Hz .. 6 kHz. Metallic / ringing tails show persistent narrow peaks. */
double ringingDb (const Stereo& ir, double fs, double T, double t0)
{
    const int order = 12, N = 1 << order;
    std::vector<float> win ((size_t) N);
    for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * i / (N - 1));
    const size_t a = (size_t) (t0 * fs);
    const size_t b = std::min (ir.size(), a + (size_t) (std::max (0.6, std::min (T, 2.5)) * fs));
    std::vector<double> psd ((size_t) (N / 2 + 1), 0.0);
    int segs = 0;
    std::vector<float> seg ((size_t) N);
    for (size_t s = a; s + (size_t) N <= b; s += (size_t) N / 4)
        for (int ch = 0; ch < 2; ++ch)
        {
            const auto& x = ch == 0 ? ir.l : ir.r;
            for (int i = 0; i < N; ++i)
            {
                const double t = (double) (s + (size_t) i) / fs;
                seg[(size_t) i] = (float) (x[s + (size_t) i] * std::pow (10.0, 3.0 * t / T));
            }
            const auto p = powerSpectrum (seg.data(), (size_t) N, order, &win);
            for (size_t i = 0; i < psd.size(); ++i) psd[i] += p[i];
            ++segs;
        }
    if (segs == 0) return 0;
    double worst = 0;
    const double binHz = fs / N;
    std::vector<double> nb;
    for (size_t i = (size_t) (200 / binHz); i < (size_t) (6000 / binHz); ++i)
    {
        const double f = i * binHz;
        const size_t lo = (size_t) (f / 1.122 / binHz), hi = (size_t) (f * 1.122 / binHz);
        nb.clear();
        for (size_t j = lo; j <= hi; ++j) nb.push_back (psd[j]);
        std::nth_element (nb.begin(), nb.begin() + (long) nb.size() / 2, nb.end());
        const double med = nb[nb.size() / 2];
        if (med > 0) worst = std::max (worst, 10.0 * std::log10 (psd[i] / med));
        if (gRingDump && med > 0 && 10.0 * std::log10 (psd[i] / med) > 6.0)
            std::printf ("      peak %7.1f Hz  +%.1f dB\n", f, 10.0 * std::log10 (psd[i] / med));
    }
    return worst;
}

/** Octave-band energies (per second of signal) of the stereo signal via one big FFT. */
std::vector<double> octaveBands (const Stereo& s, double fs, const std::vector<double>& centres)
{
    int order = 1;
    while ((1u << order) < s.size()) ++order;
    std::vector<double> out (centres.size(), 0.0);
    for (int ch = 0; ch < 2; ++ch)
    {
        const auto p = powerSpectrum ((ch == 0 ? s.l : s.r).data(), s.size(), order);
        const double binHz = fs / (1 << order);
        for (size_t c = 0; c < centres.size(); ++c)
        {
            const size_t lo = (size_t) (centres[c] / 1.414 / binHz), hi = (size_t) (centres[c] * 1.414 / binHz);
            for (size_t i = lo; i <= hi && i < p.size(); ++i) out[c] += p[i];
        }
    }
    for (auto& v : out) v /= fs * (double) (1 << order); // Parseval: band sum scales with fs * N
    return out;
}

Stereo whiteNoise (size_t n, float amp, unsigned seed, bool mono = false)
{
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> d (-1.0f, 1.0f);
    Stereo s;
    s.l.resize (n); s.r.resize (n);
    for (size_t i = 0; i < n; ++i)
    {
        s.l[i] = amp * d (rng);
        s.r[i] = mono ? s.l[i] : amp * d (rng);
    }
    return s;
}

Stereo pinkNoise (size_t n, float rmsTarget, unsigned seed, bool mono = true)
{
    std::mt19937 rng (seed);
    std::normal_distribution<float> d (0.0f, 1.0f);
    Stereo s;
    s.l.resize (n); s.r.resize (n);
    for (int ch = 0; ch < (mono ? 1 : 2); ++ch)
    {
        float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
        auto& x = ch == 0 ? s.l : s.r;
        for (size_t i = 0; i < n; ++i)
        {
            const float w = d (rng);
            b0 = 0.99886f * b0 + w * 0.0555179f; b1 = 0.99332f * b1 + w * 0.0750759f;
            b2 = 0.96900f * b2 + w * 0.1538520f; b3 = 0.86650f * b3 + w * 0.3104856f;
            b4 = 0.55000f * b4 + w * 0.5329522f; b5 = -0.7616f * b5 - w * 0.0168980f;
            x[i] = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f;
            b6 = w * 0.115926f;
        }
        const double r = rms (x);
        for (auto& v : x) v = (float) (v * rmsTarget / r);
    }
    if (mono) s.r = s.l;
    return s;
}

//==============================================================================
// Test material for rendering
//==============================================================================
Stereo makeDrumLoop (double fs, double seconds)
{
    Stereo s;
    const size_t n = (size_t) (seconds * fs);
    s.l.assign (n, 0.0f); s.r.assign (n, 0.0f);
    std::mt19937 rng (7);
    std::uniform_real_distribution<float> d (-1.0f, 1.0f);
    const double beat = 0.5; // 120 bpm
    const double loopLen = 4.0; // 2 bars of 4/4
    const double playFor = seconds - 3.0;
    for (double t0 = 0; t0 < playFor; t0 += loopLen)
    {
        // kick on 1, 2.5(and of 2), 3 ; snare 2, 4 ; hats 8ths
        const double kicks[] = { 0, 1.5, 2, 4, 5.5, 6 };
        const double snares[] = { 1, 3, 5, 7 };
        for (double k : kicks)
        {
            const size_t st = (size_t) ((t0 + k * beat) * fs);
            double ph = 0;
            for (size_t i = 0; i < (size_t) (0.45 * fs) && st + i < n; ++i)
            {
                const double t = i / fs;
                const double f = 48 + 110 * std::exp (-t * 30);
                ph += 2 * juce::MathConstants<double>::pi * f / fs;
                const float v = (float) (0.8 * std::sin (ph) * std::exp (-t * 7) + 0.3 * d (rng) * std::exp (-t * 400));
                s.l[st + i] += v; s.r[st + i] += v;
            }
        }
        for (double k : snares)
        {
            const size_t st = (size_t) ((t0 + k * beat) * fs);
            float lp = 0, hp = 0;
            for (size_t i = 0; i < (size_t) (0.3 * fs) && st + i < n; ++i)
            {
                const double t = i / fs;
                const float nz = d (rng);
                lp += 0.35f * (nz - lp);
                hp = nz - lp;
                const float v = (float) (0.45 * (lp * 0.6 + hp) * std::exp (-t * 18)
                                         + 0.35 * std::sin (2 * juce::MathConstants<double>::pi * 190 * t) * std::exp (-t * 25));
                s.l[st + i] += v * 0.95f; s.r[st + i] += v;
            }
        }
        for (int h = 0; h < 16; ++h)
        {
            const size_t st = (size_t) ((t0 + h * beat * 0.5) * fs);
            float prev = 0;
            const float amp = (h % 2 == 0) ? 0.18f : 0.11f;
            for (size_t i = 0; i < (size_t) (0.06 * fs) && st + i < n; ++i)
            {
                const double t = i / fs;
                const float nz = d (rng);
                const float hp = nz - prev; prev = nz;
                const float v = (float) (amp * hp * std::exp (-t * 70));
                s.l[st + i] += v * 0.8f; s.r[st + i] += v;
            }
        }
    }
    return s;
}

Stereo makeChord (double fs, double seconds)
{
    Stereo s;
    const size_t n = (size_t) (seconds * fs);
    s.l.assign (n, 0.0f); s.r.assign (n, 0.0f);
    const double notes[] = { 130.81, 164.81, 196.0, 246.94, 293.66 }; // Cmaj9-ish
    const double hold = 2.5, rel = 0.4;
    for (size_t i = 0; i < n; ++i)
    {
        const double t = i / fs;
        double env = t < 0.08 ? t / 0.08 : (t < hold ? 1.0 : std::max (0.0, 1.0 - (t - hold) / rel));
        if (env <= 0) continue;
        double l = 0, r = 0;
        for (int k = 0; k < 5; ++k)
        {
            double v = 0;
            for (int h = 1; h <= 8; ++h)
                v += std::sin (2 * juce::MathConstants<double>::pi * notes[k] * h * t * (1.0 + 0.0007 * k)) / (h * 1.3);
            l += v * (0.6 + 0.08 * k);
            r += v * (0.92 - 0.08 * k);
        }
        s.l[i] = (float) (0.06 * env * l);
        s.r[i] = (float) (0.06 * env * r);
    }
    return s;
}

bool writeWav (const juce::File& file, const Stereo& s, double fs)
{
    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);
    if (! static_cast<juce::FileOutputStream*> (stream.get())->openedOk())
        return false;
    juce::WavAudioFormat wav;
    auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}
                                                   .withSampleRate (fs)
                                                   .withNumChannels (2)
                                                   .withBitsPerSample (24));
    if (writer == nullptr)
        return false;
    const float* chans[2] = { s.l.data(), s.r.data() };
    return writer->writeFromFloatArrays (chans, 2, (int) s.size());
}

//==============================================================================
// Tests
//==============================================================================
void testRt60()
{
    std::printf ("\n[RT60] mid-band (707-1414 Hz) T30 fit (-5..-35 dB) vs requested, size 0.5, R=1, 48 kHz\n");
    std::printf ("  %-10s %16s %16s %16s\n", "type", "0.5 s", "2 s", "8 s");
    for (auto t : kTypes)
    {
        std::string row = fmt ("  %-10s", typeName (t));
        for (float T : { 0.5f, 2.0f, 8.0f })
        {
            const auto ir = impulseResponse (makeParams (t, T), T * 1.4 + 0.7);
            check (allFinite (ir), fmt ("%s T=%.1f IR finite", typeName (t), T));
            const auto e = bandEnergy (ir, kBase, 707, 1414);
            const double m = rt60 (e, kBase);
            const double err = (m / T - 1.0) * 100.0;
            row += fmt ("  %5.2f (%+5.1f%%)", m, err);
            check (m > 0 && std::abs (err) <= 20.0, fmt ("%s RT60 %.2f s requested %.2f s measured", typeName (t), T, m));
        }
        std::printf ("%s\n", row.c_str());
    }
}

void testRt60Broadband()
{
    std::printf ("\n[RT60 vs frequency] decay 2 s: octave-band T30 (s) — shows the per-type frequency-dependent decay\n");
    std::printf ("  %-10s %7s %7s %7s %7s %7s %7s\n", "type", "125", "250", "500", "1k", "2k", "4k");
    for (auto t : kTypes)
    {
        const auto ir = impulseResponse (makeParams (t, 2.0f), 3.6);
        std::string row = fmt ("  %-10s", typeName (t));
        for (double fc : { 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0 })
            row += fmt (" %7.2f", rt60 (bandEnergy (ir, kBase, fc / 1.414, fc * 1.414), kBase));
        std::printf ("%s\n", row.c_str());
    }
}

void testCharacter()
{
    std::printf ("\n[Character] decay 2 s, size 0.5, predelay 0 (IR analysis)\n");
    {
        // Reference for the ringing metric: an ideal reverb tail (exponentially decaying Gaussian noise).
        std::mt19937 rng (1234);
        std::normal_distribution<float> nd (0.0f, 1.0f);
        Stereo ref;
        ref.l.resize ((size_t) (3 * kBase)); ref.r.resize (ref.l.size());
        for (size_t i = 0; i < ref.size(); ++i)
        {
            const float env = (float) std::pow (10.0, -3.0 * (double) i / kBase / 2.0);
            ref.l[i] = nd (rng) * env; ref.r[i] = nd (rng) * env;
        }
        std::printf ("  reference (ideal Gaussian tail): ringing %.1f dB, flutter %.2f\n", ringingDb (ref, kBase, 2.0, 0.15),
                     flutter (ref, kBase, 2.0, 0.1, 0.8));
        // Sanity: a fluttery tail (noise gated into 3 ms bursts every 23 ms) must score high.
        Stereo bad = ref;
        for (size_t i = 0; i < bad.size(); ++i)
            if ((i % (size_t) (0.023 * kBase)) > (size_t) (0.003 * kBase)) { bad.l[i] *= 0.2f; bad.r[i] *= 0.2f; }
        const double badFlutter = flutter (bad, kBase, 2.0, 0.1, 0.8);
        std::printf ("  sanity (synthetic 23 ms flutter): flutter %.2f\n", badFlutter);
        check (badFlutter > 0.5, "flutter metric detects synthetic flutter");
    }
    std::printf ("  %-10s %7s %7s %7s %7s %8s %7s %7s %8s %7s %7s\n",
                 "type", "NED", "NEDmin", "ring", "flutter", "centroid", "build", "ER%", "corrLR", "mono", "level");
    for (auto t : kTypes)
    {
        const double T = 2.0;
        const auto ir = impulseResponse (makeParams (t, (float) T), 3.0);
        double nedMin = 0;
        const double ned = (echoDensity (ir.l, kBase, 0.1, 0.6, &nedMin) + echoDensity (ir.r, kBase, 0.1, 0.6)) * 0.5;
        const double ring = ringingDb (ir, kBase, T, 0.15);
        const double flut = flutter (ir, kBase, T, 0.1, 0.8);

        // spectral centroid of the IR
        const auto bands = octaveBands (ir, kBase, { 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 });
        double num = 0, den = 0;
        const double cs[] = { 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };
        for (size_t i = 0; i < bands.size(); ++i) { num += bands[i] * std::log2 (cs[i]); den += bands[i]; }
        const double centroid = std::exp2 (num / den);

        // build time: time of max of the 10 ms RMS envelope
        const int w = (int) (0.01 * kBase);
        double best = 0; size_t bestI = 0;
        for (size_t i = 0; i + (size_t) w < ir.size() && i < (size_t) (1.5 * kBase); i += (size_t) w)
        {
            double e = 0;
            for (size_t j = i; j < i + (size_t) w; ++j) e += ir.l[j] * ir.l[j] + ir.r[j] * ir.r[j];
            if (e > best) { best = e; bestI = i; }
        }
        // ER fraction: energy in the first 60 ms
        double e60 = 0, eAll = 0;
        for (size_t i = 0; i < ir.size(); ++i)
        {
            const double e = ir.l[i] * ir.l[i] + ir.r[i] * ir.r[i];
            eAll += e;
            if (i < (size_t) (0.06 * kBase)) e60 += e;
        }
        // stereo correlation + mono sum on noise
        const auto noise = whiteNoise ((size_t) (6 * kBase), 0.1f, 11, true);
        Reverb rv;
        rv.prepare (kBase, 1, 512);
        rv.setParams (makeParams (t, (float) T));
        rv.reset();
        const auto wet = runReverb (rv, noise);
        double sl = 0, sr = 0, slr = 0, sm = 0;
        for (size_t i = (size_t) (2 * kBase); i < wet.size(); ++i)
        {
            sl += wet.l[i] * wet.l[i]; sr += wet.r[i] * wet.r[i]; slr += wet.l[i] * wet.r[i];
            const double m = wet.l[i] + wet.r[i];
            sm += m * m;
        }
        const double corr = slr / std::sqrt (sl * sr);
        const double monoRatio = sm / (sl + sr); // 1 = uncorrelated, 2 = mono, 0 = anti-phase

        // level on pink noise (mono source), wet RMS per channel vs input RMS
        const auto pink = pinkNoise ((size_t) (7 * kBase), 0.1f, 3);
        rv.reset();
        const auto wp = runReverb (rv, pink);
        const double lvl = toDb (std::sqrt (0.5 * (std::pow (rms (wp.l, (size_t) (3 * kBase)), 2) + std::pow (rms (wp.r, (size_t) (3 * kBase)), 2))) / 0.1);

        std::printf ("  %-10s %7.2f %7.2f %6.1fdB %7.2f %7.0fHz %5.0fms %6.1f%% %8.2f %6.1fdB %+6.1fdB\n",
                     typeName (t), ned, nedMin, ring, flut, centroid, bestI / kBase * 1000.0, 100.0 * e60 / eAll,
                     corr, 10 * std::log10 (monoRatio), lvl);

        if (t != RType::spring)
        {
            check (ned > 0.8, fmt ("%s echo density after 100 ms %.2f (want > 0.8)", typeName (t), ned));
            check (nedMin > 0.55, fmt ("%s min echo density %.2f (want > 0.55)", typeName (t), nedMin));
            check (flut < 0.3, fmt ("%s flutter (envelope periodicity) %.2f (want < 0.3)", typeName (t), flut));
            check (ring < 9.0, fmt ("%s tail ringing %.1f dB over local median (want < 9)", typeName (t), ring));
        }
        check (monoRatio > 0.7, fmt ("%s mono sum ratio %.2f (want > 0.7, i.e. no deep cancellation)", typeName (t), monoRatio));
        check (std::abs (lvl) < 3.5, fmt ("%s level %+.1f dB vs input (want within +-3.5 dB)", typeName (t), lvl));
        check (allFinite (wet) && allFinite (wp), fmt ("%s noise output finite", typeName (t)));
    }
}

void testLevelVsDecay()
{
    std::printf ("\n[Level vs decay] wet RMS re input (mono pink noise), size 0.5\n  %-10s", "type");
    const float decays[] = { 0.3f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 20.0f };
    for (float d : decays) std::printf (" %6.1fs", d);
    std::printf ("\n");
    const auto pink = pinkNoise ((size_t) (12 * kBase), 0.1f, 3);
    for (auto t : kTypes)
    {
        std::printf ("  %-10s", typeName (t));
        for (float d : decays)
        {
            Reverb rv;
            rv.prepare (kBase, 1, 512);
            rv.setParams (makeParams (t, d));
            rv.reset();
            const auto w = runReverb (rv, pink);
            const size_t a = (size_t) (std::min (10.0, 1.0 + 0.5 * d) * kBase);
            const double lvl = toDb (std::sqrt (0.5 * (std::pow (rms (w.l, a), 2) + std::pow (rms (w.r, a), 2))) / 0.1);
            std::printf (" %+6.1f ", lvl);
        }
        std::printf ("\n");
    }
}

/** Max |second difference| — clicks show up as large spikes for low-frequency material. */
double maxSecondDiff (const Stereo& s, size_t from, size_t to)
{
    double m = 0;
    for (size_t i = std::max<size_t> (from, 2); i < std::min (to, s.size()); ++i)
        for (int ch = 0; ch < 2; ++ch)
        {
            const auto& x = ch == 0 ? s.l : s.r;
            m = std::max (m, (double) std::abs (x[i] - 2 * x[i - 1] + x[i - 2]));
        }
    return m;
}

Stereo lowChord (size_t n, double fs)
{
    Stereo s;
    s.l.resize (n); s.r.resize (n);
    for (size_t i = 0; i < n; ++i)
    {
        const double t = i / fs;
        const double v = 0.25 * (std::sin (2 * juce::MathConstants<double>::pi * 110 * t)
                                 + std::sin (2 * juce::MathConstants<double>::pi * 138.59 * t)
                                 + std::sin (2 * juce::MathConstants<double>::pi * 164.81 * t));
        s.l[i] = (float) v;
        s.r[i] = (float) (v * 0.9);
    }
    return s;
}

/** Spring dispersion: energy-weighted arrival time within the first 28 ms (before the first loop
    return) per narrow band. */
void testSpringDispersion()
{
    std::printf ("\n[Spring dispersion] first-arrival time per band (chirp: HF arrives later)\n");
    const auto ir = impulseResponse (makeParams (RType::spring, 2.0f, 0.5f, 0.0f, 0.0f), 0.5);
    std::string row = "  ";
    double tLo = 0, tHi = 0;
    for (double f : { 300.0, 700.0, 1500.0, 2500.0, 3500.0 })
    {
        Biquad bp;
        bp.prepare (1);
        bp.setParameters (BiquadType::bandPass, kBase, f, 6.0);
        double e = 0, et = 0;
        for (size_t i = 0; i < (size_t) (0.028 * kBase); ++i)
        {
            const double y = bp.processSample (0, ir.l[i] + ir.r[i]);
            e += y * y;
            et += y * y * (double) i;
        }
        const double ms = et / e / kBase * 1000.0;
        row += fmt ("%4.0f Hz: %5.1f ms   ", f, ms);
        if (f == 700.0) tLo = ms;
        if (f == 3500.0) tHi = ms;
    }
    std::printf ("%s\n", row.c_str());
    check (tHi - tLo > 5.0, fmt ("spring dispersion: 3.5 kHz arrives %.1f ms after 700 Hz (want > 5 ms)", tHi - tLo));
}

void testClicks()
{
    std::printf ("\n[Clicks] max |2nd difference| on a low chord (switching / sweeping vs steady state)\n");
    const double fs = kBase;
    const auto in = lowChord ((size_t) (14 * fs), fs);

    // Baseline: each type steady.
    double base = 0;
    for (auto t : kTypes)
    {
        Reverb rv;
        rv.prepare (fs, 1, 512);
        rv.setParams (makeParams (t, 2.0f, 0.5f, 10.0f));
        rv.reset();
        Stereo shortIn { std::vector<float> (in.l.begin(), in.l.begin() + (long) (4 * fs)),
                         std::vector<float> (in.r.begin(), in.r.begin() + (long) (4 * fs)) };
        const auto out = runReverb (rv, shortIn);
        base = std::max (base, maxSecondDiff (out, (size_t) (2 * fs), out.size()));
    }

    // Type switching every 350 ms through all types (twice), includes rapid re-switching.
    {
        Reverb rv;
        rv.prepare (fs, 1, 512);
        rv.setParams (makeParams (RType::hall, 2.0f, 0.5f, 10.0f));
        rv.reset();
        int idx = 0;
        size_t next = (size_t) (2 * fs);
        const auto out = runReverb (rv, in, 256, [&] (int pos)
        {
            if ((size_t) pos >= next)
            {
                idx = (idx + 3) % 8;
                rv.setParams (makeParams (kTypes[idx], 2.0f, 0.5f, 10.0f));
                next += (size_t) ((idx % 3 == 0 ? 0.05 : 0.35) * fs); // some very fast changes
            }
        });
        const double sw = maxSecondDiff (out, (size_t) (2 * fs), out.size());
        std::printf ("  baseline %.2e   type switching %.2e  (ratio %.2f)\n", base, sw, sw / base);
        check (allFinite (out), "type switching output finite");
        check (sw < 2.5 * base, fmt ("type switching clicks: ratio %.2f (want < 2.5)", sw / base));
    }

    // Parameter sweeps (fast, full range).
    struct Sweep { const char* name; std::function<void (Reverb::Params&, float)> apply; };
    const Sweep sweeps[] = {
        { "decay",     [] (Reverb::Params& p, float x) { p.decaySeconds = 0.1f + 19.9f * x * x; } },
        { "size",      [] (Reverb::Params& p, float x) { p.size = x; } },
        { "predelay",  [] (Reverb::Params& p, float x) { p.predelayMs = 250.0f * x; } },
        { "tone",      [] (Reverb::Params& p, float x) { p.tone = 2.0f * x - 1.0f; } },
        { "width",     [] (Reverb::Params& p, float x) { p.width = 1.5f * x; } },
        { "mod",       [] (Reverb::Params& p, float x) { p.modulation = x; } },
    };
    for (auto t : { RType::hall, RType::room, RType::spring, RType::bloom })
    {
        std::string row = fmt ("  %-9s", typeName (t));
        for (const auto& sw : sweeps)
        {
            Reverb rv;
            rv.prepare (fs, 1, 512);
            auto p = makeParams (t, 2.0f, 0.5f, 10.0f);
            rv.setParams (p);
            rv.reset();
            Stereo shortIn { std::vector<float> (in.l.begin(), in.l.begin() + (long) (6 * fs)),
                             std::vector<float> (in.r.begin(), in.r.begin() + (long) (6 * fs)) };
            const auto out = runReverb (rv, shortIn, 256, [&] (int pos)
            {
                if (pos < (int) (2 * fs)) return;
                const double ph = std::fmod ((pos - 2 * fs) / fs, 1.0); // 1 s triangle: 0->1->0
                const float x = (float) (ph < 0.5 ? 2 * ph : 2 - 2 * ph);
                auto q = p;
                sw.apply (q, x);
                rv.setParams (q);
            });
            // Steady reference for this type
            const double ref = std::max (base * 0.25, maxSecondDiff (out, (size_t) (1.5 * fs), (size_t) (2 * fs)));
            const double m = maxSecondDiff (out, (size_t) (2 * fs), out.size());
            row += fmt (" %s %.2f", sw.name, m / ref);
            check (allFinite (out), fmt ("%s %s sweep finite", typeName (t), sw.name));
            // Sweeping decay up raises the level (more energy) and predelay/size glides pitch-shift; allow headroom.
            check (m < 4.5 * ref, fmt ("%s %s sweep click ratio %.2f (want < 4.5)", typeName (t), sw.name, m / ref));
        }
        std::printf ("%s\n", row.c_str());
    }
}

void testRateFactors()
{
    std::printf ("\n[Rate factors] base 48 kHz, R = 1/2/4/8: mid RT60 and octave-band energy (dB re R=1)\n");
    const std::vector<double> centres { 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };
    for (auto t : { RType::hall, RType::plate, RType::spring, RType::ambience })
    {
        std::vector<double> ref;
        double refRt = 0;
        for (int R : { 1, 2, 4, 8 })
        {
            const auto ir = impulseResponse (makeParams (t, 2.0f), 3.2, R);
            const double fs = kBase * R;
            const double m = rt60 (bandEnergy (ir, fs, 707, 1414), fs);
            const auto b = octaveBands (ir, fs, centres);
            if (R == 1) { ref = b; refRt = m; }
            std::string row = fmt ("  %-9s R=%d  RT60 %.2f s  bands:", typeName (t), R, m);
            double worst = 0;
            for (size_t i = 0; i < b.size(); ++i)
            {
                const double d = 10 * std::log10 (b[i] / ref[i]);
                row += fmt (" %+5.2f", d);
                if (centres[i] <= 8000) worst = std::max (worst, std::abs (d));
            }
            std::printf ("%s\n", row.c_str());
            check (allFinite (ir), fmt ("%s R=%d finite", typeName (t), R));
            check (std::abs (m / refRt - 1.0) < 0.08, fmt ("%s R=%d RT60 %.2f vs %.2f at R=1", typeName (t), R, m, refRt));
            check (worst < 1.0, fmt ("%s R=%d band deviation %.2f dB (want < 1 dB up to 8 kHz)", typeName (t), R, worst));
            const double d16 = 10 * std::log10 (b.back() / ref.back());
            check (std::abs (d16) < 2.0, fmt ("%s R=%d 16 kHz band %.2f dB (want within 2 dB)", typeName (t), R, d16));
        }
    }

    // Passband of the decimate/interpolate pair itself: sine through a 0 s-predelay plate is not
    // a clean measurement, so check the AA filter design directly at 16 kHz for a 48 kHz base.
    for (int R : { 2, 4, 8 })
    {
        const double fsR = kBase * R;
        double mag = 1;
        static constexpr double qs[4] = { 0.50979558, 0.60134489, 0.89997622, 2.56291545 };
        for (double q : qs)
            mag *= BiquadCoeffs::design (BiquadType::lowPass, fsR, std::min (0.42 * kBase, 21000.0), q).magnitude (16000, fsR);
        const double db = 2 * toDb (mag); // down + up
        std::printf ("  AA pair response at 16 kHz, R=%d: %+.2f dB\n", R, db);
        check (db > -1.5, fmt ("AA passband at 16 kHz R=%d: %.2f dB", R, db));
    }
}

void testStability (bool quick)
{
    std::printf ("\n[Stability] decay 20 s, size 1, mod 1, tone +1 and -1, %s of white noise per type\n", quick ? "15 s" : "60 s");
    const double seconds = quick ? 15.0 : 60.0;
    const auto noise = whiteNoise ((size_t) (seconds * kBase), 0.5f, 99);
    for (auto t : kTypes)
        for (float tone : { 1.0f, -1.0f })
        {
            Reverb rv;
            rv.prepare (kBase, 1, 512);
            rv.setParams (makeParams (t, 20.0f, 1.0f, 30.0f, 1.0f, tone, 1.5f));
            rv.reset();
            const auto out = runReverb (rv, noise, 512);
            const double pk = peakAbs (out);
            const size_t n = out.size();
            const double late = rms (out.l, n - (size_t) (5 * kBase), n);
            const double mid = rms (out.l, n - (size_t) (10 * kBase), n - (size_t) (5 * kBase));
            const bool fin = allFinite (out);
            std::printf ("  %-10s tone %+.0f  peak %6.2f  rms(last 5 s) %6.1f dB  growth %+5.2f dB\n",
                         typeName (t), tone, pk, toDb (late), toDb (late / mid));
            check (fin, fmt ("%s stability finite", typeName (t)));
            check (pk < 30.0, fmt ("%s stability peak %.1f (want bounded < 30)", typeName (t), pk));
            check (std::abs (toDb (late / mid)) < 1.5 || quick, fmt ("%s stability rms still changing %.2f dB", typeName (t), toDb (late / mid)));
        }
}

void testSilence()
{
    std::printf ("\n[isSilent]\n");
    for (auto t : { RType::hall, RType::bloom, RType::spring, RType::ambience })
    {
        for (int R : { 1, 4 })
        {
            const double fs = kBase * R;
            Reverb rv;
            rv.prepare (fs, R, 512);
            rv.setParams (makeParams (t, 1.5f, 0.5f, 80.0f));
            rv.reset();
            const int block = 256;
            std::vector<float> inL (block), inR (block), oL (block), oR (block);
            std::mt19937 rng (5);
            std::uniform_real_distribution<float> d (-0.3f, 0.3f);
            bool silentDuringNoise = false, silentEarly = false;
            double silentAt = -1;
            double maxAfterSilent = 0;
            const int total = (int) (8 * fs / block);
            for (int b = 0; b < total; ++b)
            {
                const double t0 = b * block / fs;
                for (int i = 0; i < block; ++i)
                {
                    inL[(size_t) i] = t0 < 1.0 ? d (rng) : 0.0f;
                    inR[(size_t) i] = inL[(size_t) i];
                }
                rv.process (inL.data(), inR.data(), oL.data(), oR.data(), block);
                if (t0 < 1.0 && rv.isSilent()) silentDuringNoise = true;
                if (t0 >= 1.0 && t0 < 1.2 && rv.isSilent()) silentEarly = true;
                if (rv.isSilent() && silentAt < 0) silentAt = t0;
                if (silentAt >= 0)
                    for (int i = 0; i < block; ++i)
                        maxAfterSilent = std::max (maxAfterSilent, (double) std::max (std::abs (oL[(size_t) i]), std::abs (oR[(size_t) i])));
            }
            // RT60 1.5 s: -90 dB after ~2.25 s (+ predelay/ER/build), so expect silence within ~4 s of input stop.
            std::printf ("  %-9s R=%d silent after %.2f s of input stop, max output afterwards %.1f dBFS\n",
                         typeName (t), R, silentAt - 1.0, toDb (maxAfterSilent));
            check (! silentDuringNoise, fmt ("%s R=%d not silent while input present", typeName (t), R));
            check (! silentEarly, fmt ("%s R=%d not silent right after input stops", typeName (t), R));
            check (silentAt > 0 && silentAt - 1.0 < 4.5, fmt ("%s R=%d becomes silent (at %.2f s)", typeName (t), R, silentAt - 1.0));
            check (maxAfterSilent < 3.2e-5, fmt ("%s R=%d output below -90 dBFS once silent", typeName (t), R));

            // Wakes up immediately on new input.
            for (int i = 0; i < block; ++i) { inL[(size_t) i] = inR[(size_t) i] = (i == 0 ? 0.5f : 0.0f); }
            rv.process (inL.data(), inR.data(), oL.data(), oR.data(), block);
            check (! rv.isSilent(), fmt ("%s R=%d wakes up on input", typeName (t), R));
        }
    }
}

void testCpu()
{
    std::printf ("\n[CPU] %% of one core (realtime), white noise, block 256\n");
    const double seconds = 10.0;
    std::string header = fmt ("  %-10s", "type");
    for (const char* c : { "48k R1", "44k R1", "96k R1", "48k R2", "48k R4", "48k R8" }) header += fmt (" %8s", c);
    std::printf ("%s\n", header.c_str());
    for (auto t : kTypes)
    {
        std::string row = fmt ("  %-10s", typeName (t));
        for (auto [base, R] : { std::pair { 48000.0, 1 }, { 44100.0, 1 }, { 96000.0, 1 }, { 48000.0, 2 }, { 48000.0, 4 }, { 48000.0, 8 } })
        {
            const double fs = base * R;
            const size_t n = (size_t) (seconds * fs);
            const auto noise = whiteNoise (n, 0.3f, 1);
            Reverb rv;
            rv.prepare (fs, R, 256);
            rv.setParams (makeParams (t, 2.5f, 0.6f, 20.0f, 0.5f));
            rv.reset();
            Stereo out;
            out.l.resize (n); out.r.resize (n);
            const auto t0 = std::chrono::high_resolution_clock::now();
            for (size_t i = 0; i < n; i += 256)
            {
                const int m = (int) std::min<size_t> (256, n - i);
                rv.process (noise.l.data() + i, noise.r.data() + i, out.l.data() + i, out.r.data() + i, m);
            }
            const double el = std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
            row += fmt (" %7.2f%%", 100.0 * el / seconds);
        }
        std::printf ("%s\n", row.c_str());
    }
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        Reverb rv;
        rv.prepare (fs, 1, 512);
        std::printf ("  memory per instance at %.0f Hz base: %.2f MB\n", fs, rv.getMemoryBytes() / 1048576.0);
    }

    // Transition cost: both slots running (old tail ringing out while the new type builds).
    {
        const double fs = 48000.0;
        const size_t n = (size_t) (10 * fs);
        const auto noise = whiteNoise (n, 0.3f, 1);
        Reverb rv;
        rv.prepare (fs, 1, 256);
        rv.setParams (makeParams (RType::hall, 8.0f, 0.6f, 20.0f, 0.5f));
        rv.reset();
        Stereo out;
        out.l.resize (n); out.r.resize (n);
        double worst = 0;
        for (size_t i = 0; i < n; i += 256)
        {
            if (i % (size_t) (2 * fs) < 256)
                rv.setParams (makeParams ((i / (size_t) (2 * fs)) % 2 ? RType::cathedral : RType::hall, 8.0f, 0.6f, 20.0f, 0.5f));
            const auto t0 = std::chrono::high_resolution_clock::now();
            rv.process (noise.l.data() + i, noise.r.data() + i, out.l.data() + i, out.r.data() + i, 256);
            const double el = std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
            worst = std::max (worst, el);
        }
        std::printf ("  hall<->cathedral switching every 2 s (both slots busy): worst block %.1f us of %.0f us budget\n",
                     worst * 1e6, 256.0 / fs * 1e6);
    }
}

void testTypeSwitchTail()
{
    std::printf ("\n[Type switch tail] old tail keeps ringing after a switch\n");
    Reverb rv;
    rv.prepare (kBase, 1, 512);
    rv.setParams (makeParams (RType::hall, 4.0f));
    rv.reset();
    Stereo in;
    in.l.assign ((size_t) (2 * kBase), 0.0f);
    in.r = in.l;
    for (size_t i = 0; i < (size_t) (0.3 * kBase); ++i) in.l[i] = in.r[i] = (i % 97 < 3) ? 0.3f : 0.0f;
    bool switched = false;
    const auto out = runReverb (rv, in, 256, [&] (int pos)
    {
        if (! switched && pos >= (int) (0.5 * kBase)) { switched = true; rv.setParams (makeParams (RType::plate, 4.0f)); }
    });
    const double before = rms (out.l, (size_t) (0.4 * kBase), (size_t) (0.5 * kBase));
    const double after = rms (out.l, (size_t) (0.6 * kBase), (size_t) (0.7 * kBase));
    std::printf ("  RMS 100 ms before switch %.1f dB, 100..200 ms after %.1f dB\n", toDb (before), toDb (after));
    check (after > before * 0.5, "old tail not chopped on type switch (>-6 dB 100 ms later at RT60 4 s)");
}

//==============================================================================
// Render + objective listen-check
//==============================================================================
void render (const juce::File& dir)
{
    std::printf ("\n[Render] writing to %s\n", dir.getFullPathName().toRawUTF8());
    dir.createDirectory();
    const double fs = 48000.0;
    const auto drums = makeDrumLoop (fs, 11.0);
    const auto chord = makeChord (fs, 9.0);
    writeWav (dir.getChildFile ("dry_drums.wav"), drums, fs);
    writeWav (dir.getChildFile ("dry_chord.wav"), chord, fs);

    for (auto t : kTypes)
    {
        const auto ir = impulseResponse (makeParams (t, 2.0f, 0.5f, 0.0f, 0.3f), 3.0);
        writeWav (dir.getChildFile ("ir_" + juce::String (typeName (t)).toLowerCase() + ".wav"), ir, kBase);
    }

    struct Setting { const char* tag; float decay, size, pd, tone, mod; };
    const Setting settings[] = { { "short", 1.2f, 0.4f, 8.0f, 0.0f, 0.3f }, { "long", 4.0f, 0.75f, 20.0f, 0.0f, 0.5f } };

    std::printf ("  %-10s %-6s %-6s %8s %8s %8s %8s %9s\n", "type", "set", "src", "wetRMS", "centroid", "corrLR", "tail-1s", "wet/dry");
    for (auto t : kTypes)
        for (const auto& st : settings)
            for (int src = 0; src < 2; ++src)
            {
                const auto& dry = src == 0 ? drums : chord;
                Reverb rv;
                rv.prepare (fs, 1, 512);
                rv.setParams (makeParams (t, st.decay, st.size, st.pd, st.mod, st.tone));
                rv.reset();
                const auto wet = runReverb (rv, dry);
                Stereo mix = wet;
                for (size_t i = 0; i < mix.size(); ++i) { mix.l[i] = dry.l[i] + 0.5f * wet.l[i]; mix.r[i] = dry.r[i] + 0.5f * wet.r[i]; }
                const auto base = juce::String (typeName (t)).toLowerCase() + "_" + (src == 0 ? "drums_" : "chord_") + st.tag;
                writeWav (dir.getChildFile (base + "_wet.wav"), wet, fs);
                writeWav (dir.getChildFile (base + "_mix.wav"), mix, fs);

                const auto bands = octaveBands (wet, fs, { 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 });
                const double cs[] = { 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };
                double num = 0, den = 0;
                for (size_t i = 0; i < bands.size(); ++i) { num += bands[i] * std::log2 (cs[i]); den += bands[i]; }
                double sl = 0, sr = 0, slr = 0;
                for (size_t i = 0; i < wet.size(); ++i) { sl += wet.l[i] * wet.l[i]; sr += wet.r[i] * wet.r[i]; slr += wet.l[i] * wet.r[i]; }
                // tail: level 1 s after the source ends relative to wet RMS while playing
                const size_t endSrc = src == 0 ? (size_t) (8.0 * fs) : (size_t) (2.9 * fs);
                const double playing = rms (wet.l, (size_t) (0.5 * fs), endSrc);
                const double tail = rms (wet.l, endSrc + (size_t) (0.9 * fs), endSrc + (size_t) (1.1 * fs));
                const double dryRms = rms (dry.l, (size_t) (0.5 * fs), endSrc);
                std::printf ("  %-10s %-6s %-6s %7.1fdB %7.0fHz %8.2f %7.1fdB %8.1fdB\n", typeName (t), st.tag,
                             src == 0 ? "drums" : "chord", toDb (rms (wet.l)), std::exp2 (num / den), slr / std::sqrt (sl * sr),
                             toDb (tail / playing), toDb (playing / dryRms));
                check (allFinite (wet), fmt ("render %s finite", base.toRawUTF8()));
                check (peakAbs (wet) < 4.0, fmt ("render %s peak %.2f sane", base.toRawUTF8(), peakAbs (wet)));
            }
}

void diagRinging()
{
    std::printf ("\n[Diag] ringing metric vs modulation / size (decay 2 s)\n");
    gRingDump = true;
    for (auto t : { RType::room, RType::hall })
        for (float sz : { 0.4f, 0.5f, 0.6f })
            for (float T : { 2.0f, 5.0f })
            {
                std::printf ("  %s size %.1f T %.0f peaks:\n", typeName (t), sz, T);
                ringingDb (impulseResponse (makeParams (t, T, sz), T * 1.5), kBase, T, 0.15);
            }
    gRingDump = false;
    for (auto t : { RType::room, RType::hall, RType::plate, RType::ambience })
        for (float mod : { 0.0f, 0.3f, 1.0f })
            for (float size : { 0.2f, 0.5f, 1.0f })
            {
                const auto ir = impulseResponse (makeParams (t, 2.0f, size, 0.0f, mod), 3.0);
                std::printf ("  %-9s mod %.1f size %.1f  ring %.1f dB  late-window ring %.1f dB\n", typeName (t), mod, size,
                             ringingDb (ir, kBase, 2.0, 0.15), ringingDb (ir, kBase, 2.0, 1.2));
            }
}

} // namespace

int main (int argc, char** argv)
{
    bool quick = false;
    juce::String renderDir;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp (argv[i], "--quick") == 0) quick = true;
        else if (std::strcmp (argv[i], "--render") == 0 && i + 1 < argc) renderDir = argv[++i];
    }

    for (int i = 1; i < argc; ++i)
        if (std::strcmp (argv[i], "--diag") == 0) { diagRinging(); return 0; }
    std::printf ("ST Reverb tests\n");
    testRt60();
    testRt60Broadband();
    testCharacter();
    testSpringDispersion();
    testLevelVsDecay();
    testClicks();
    testTypeSwitchTail();
    testRateFactors();
    testSilence();
    testStability (quick);
    testCpu();
    if (renderDir.isNotEmpty())
        render (juce::File::getCurrentWorkingDirectory().getChildFile (renderDir));

    std::printf ("\n%d checks, %d failures\n", gChecks, gFailures);
    return gFailures;
}
