// Reverb — see Reverb.h for the overview.

#include <bit>
#include <complex>

namespace st::dsp
{

namespace reverb_detail
{
constexpr int kLines = 16;
constexpr int kMaxTaps = 32;
constexpr int kMaxDiff = 8;
constexpr int kErDiff = 2;
constexpr int kSpringLoops = 3;
constexpr int kSpringSections = 48;
constexpr int kCtl = 16;                     // control-rate block (base-rate samples)
constexpr float kRt60Exp = 9.965784285f;     // log2(1000): g = 2^(-kRt60Exp * delay / (T * fs))
constexpr float kInputSilence = 3.0e-6f;     // ~ -110 dBFS
constexpr float kOutputSilence = 3.16e-5f;   // -90 dBFS
constexpr float kSlotQuiet = 1.0e-5f;        // -100 dBFS (releasing slot is dropped below this)
constexpr double kMaxPredelayMs = 250.0;
constexpr double kSpringFcHz = 4300.0;       // dispersion corner of the stretched allpasses

struct TypeDef
{
    bool  spring = false;
    float lineMinMs = 30.0f, lineMaxMs = 90.0f;   // FDN lines (spring: the 3 loop lengths) at size 0.5
    float sizeOctDown = 1.0f, sizeOctUp = 0.6f;   // size 0 -> x2^-down, size 1 -> x2^up
    float naturalDecay = 2.0f, maxStretch = 1.5f; // decays above naturalDecay lengthen the lines by
                                                  // sqrt(decay / naturalDecay) (<= maxStretch) to keep
                                                  // the modal overlap up (no metallic long tails)
    float loopApMinMs = 2.0f, loopApMaxMs = 8.0f, loopApG = 0.5f;
    int   diffStages = 4;
    float diffMinMs = 1.5f, diffMaxMs = 12.0f, diffG = 0.65f;
    int   erTaps = 16;
    float erStartMs = 5.0f, erEndMs = 60.0f;
    float erDensity = 0.7f;   // tap-time warp exponent (<1: denser later)
    float erFade = 1.5f;      // ER gain decay across the tap span (nepers)
    bool  erSwell = false;    // rising tap gains (Bloom)
    float erDecayRef = 0.0f;  // >0: tap span scales by clamp(decay / erDecayRef, 0.35, 1) so short decays stay short
    float erDiffG = 0.5f;
    float erOut = 0.5f, erToLate = 0.6f, directToLate = 0.6f, lateOut = 1.0f;
    float lowCutHz = 60.0f, highCutHz = 12000.0f, dampHz = 4500.0f, hfRatio = 0.55f;
    float bassHz = 250.0f, bassRatio = 1.1f;
    float modDepthMs = 0.4f, modRateHz = 0.5f;
    float width = 1.0f;
    float gainDb = 0.0f;
    std::uint32_t seed = 1;
};

inline const TypeDef& typeDef (Reverb::Type t) noexcept
{
    static const TypeDef defs[Reverb::kNumTypes] = {
        // Room: small, dense early reflections, short smooth tail.
        TypeDef { .lineMinMs = 8.0f, .lineMaxMs = 26.0f, .sizeOctDown = 1.2f, .sizeOctUp = 0.8f, .naturalDecay = 0.6f, .maxStretch = 2.2f,
                  .loopApMinMs = 1.2f, .loopApMaxMs = 3.5f, .loopApG = 0.5f,
                  .diffStages = 4, .diffMinMs = 0.8f, .diffMaxMs = 5.0f, .diffG = 0.62f,
                  .erTaps = 20, .erStartMs = 1.5f, .erEndMs = 32.0f, .erDensity = 0.7f, .erFade = 1.6f, .erDecayRef = 0.4f, .erDiffG = 0.55f,
                  .erOut = 0.75f, .erToLate = 0.5f, .directToLate = 0.6f, .lateOut = 1.0f,
                  .lowCutHz = 80.0f, .highCutHz = 12000.0f, .dampHz = 5500.0f, .hfRatio = 0.5f, .bassHz = 250.0f, .bassRatio = 1.0f,
                  .modDepthMs = 0.25f, .modRateHz = 0.9f, .width = 0.9f, .gainDb = 3.6f, .seed = 11 },
        // Chamber: warm, dense, slightly dark, quick build.
        TypeDef { .lineMinMs = 14.0f, .lineMaxMs = 42.0f, .sizeOctDown = 0.9f, .sizeOctUp = 0.6f, .naturalDecay = 1.0f, .maxStretch = 2.0f,
                  .loopApMinMs = 2.0f, .loopApMaxMs = 6.0f, .loopApG = 0.62f,
                  .diffStages = 4, .diffMinMs = 1.0f, .diffMaxMs = 7.0f, .diffG = 0.7f,
                  .erTaps = 14, .erStartMs = 3.0f, .erEndMs = 36.0f, .erDensity = 0.8f, .erFade = 1.2f, .erDecayRef = 0.5f, .erDiffG = 0.6f,
                  .erOut = 0.45f, .erToLate = 0.7f, .directToLate = 0.6f, .lateOut = 1.0f,
                  .lowCutHz = 70.0f, .highCutHz = 9000.0f, .dampHz = 3200.0f, .hfRatio = 0.45f, .bassHz = 300.0f, .bassRatio = 1.15f,
                  .modDepthMs = 0.35f, .modRateHz = 0.9f, .width = 1.0f, .gainDb = 4.4f, .seed = 23 },
        // Plate: very high diffusion, bright, no early reflections, fast build.
        TypeDef { .lineMinMs = 9.0f, .lineMaxMs = 36.0f, .sizeOctDown = 0.8f, .sizeOctUp = 0.6f, .naturalDecay = 1.2f, .maxStretch = 2.0f,
                  .loopApMinMs = 2.5f, .loopApMaxMs = 8.0f, .loopApG = 0.66f,
                  .diffStages = 6, .diffMinMs = 0.7f, .diffMaxMs = 7.0f, .diffG = 0.74f,
                  .erTaps = 0, .erStartMs = 1.0f, .erEndMs = 2.0f, .erDensity = 1.0f, .erFade = 1.0f, .erDiffG = 0.5f,
                  .erOut = 0.0f, .erToLate = 0.0f, .directToLate = 1.0f, .lateOut = 1.0f,
                  .lowCutHz = 110.0f, .highCutHz = 16000.0f, .dampHz = 9000.0f, .hfRatio = 0.75f, .bassHz = 200.0f, .bassRatio = 0.85f,
                  .modDepthMs = 0.5f, .modRateHz = 1.1f, .width = 1.0f, .gainDb = 4.3f, .seed = 37 },
        // Hall: lush, distinct early reflections, slow-ish build, modulated tail.
        TypeDef { .lineMinMs = 32.0f, .lineMaxMs = 100.0f, .sizeOctDown = 1.0f, .sizeOctUp = 0.6f, .naturalDecay = 2.2f, .maxStretch = 1.6f,
                  .loopApMinMs = 2.0f, .loopApMaxMs = 7.0f, .loopApG = 0.45f,
                  .diffStages = 4, .diffMinMs = 2.0f, .diffMaxMs = 14.0f, .diffG = 0.66f,
                  .erTaps = 18, .erStartMs = 9.0f, .erEndMs = 85.0f, .erDensity = 0.65f, .erFade = 1.3f, .erDecayRef = 1.0f, .erDiffG = 0.55f,
                  .erOut = 0.55f, .erToLate = 0.7f, .directToLate = 0.5f, .lateOut = 1.0f,
                  .lowCutHz = 45.0f, .highCutHz = 13000.0f, .dampHz = 4800.0f, .hfRatio = 0.55f, .bassHz = 250.0f, .bassRatio = 1.2f,
                  .modDepthMs = 0.9f, .modRateHz = 0.6f, .width = 1.0f, .gainDb = 3.4f, .seed = 41 },
        // Cathedral: huge, long, dark, slow build.
        TypeDef { .lineMinMs = 48.0f, .lineMaxMs = 185.0f, .sizeOctDown = 0.9f, .sizeOctUp = 0.55f, .naturalDecay = 4.0f, .maxStretch = 1.3f,
                  .loopApMinMs = 4.0f, .loopApMaxMs = 13.0f, .loopApG = 0.55f,
                  .diffStages = 6, .diffMinMs = 3.0f, .diffMaxMs = 28.0f, .diffG = 0.7f,
                  .erTaps = 30, .erStartMs = 18.0f, .erEndMs = 190.0f, .erDensity = 0.6f, .erFade = 1.0f, .erDecayRef = 1.5f, .erDiffG = 0.65f,
                  .erOut = 0.35f, .erToLate = 0.8f, .directToLate = 0.3f, .lateOut = 1.0f,
                  .lowCutHz = 35.0f, .highCutHz = 7500.0f, .dampHz = 2600.0f, .hfRatio = 0.38f, .bassHz = 220.0f, .bassRatio = 1.35f,
                  .modDepthMs = 1.1f, .modRateHz = 0.28f, .width = 1.1f, .gainDb = 3.6f, .seed = 53 },
        // Spring: dispersive chirpy loops, band-limited, mono-ish.
        TypeDef { .spring = true, .lineMinMs = 31.0f, .lineMaxMs = 47.0f, .sizeOctDown = 0.4f, .sizeOctUp = 0.4f, .naturalDecay = 100.0f, .maxStretch = 1.0f,
                  .loopApMinMs = 1.0f, .loopApMaxMs = 1.0f, .loopApG = 0.0f,
                  .diffStages = 0, .diffMinMs = 1.0f, .diffMaxMs = 1.0f, .diffG = 0.0f,
                  .erTaps = 0, .erStartMs = 1.0f, .erEndMs = 2.0f, .erDensity = 1.0f, .erFade = 1.0f, .erDiffG = 0.5f,
                  .erOut = 0.0f, .erToLate = 0.0f, .directToLate = 1.0f, .lateOut = 1.0f,
                  .lowCutHz = 140.0f, .highCutHz = 5000.0f, .dampHz = 3000.0f, .hfRatio = 0.3f, .bassHz = 250.0f, .bassRatio = 1.0f,
                  .modDepthMs = 0.05f, .modRateHz = 1.3f, .width = 0.45f, .gainDb = 3.4f, .seed = 67 },
        // Ambience: early-reflection heavy, very little tail.
        TypeDef { .lineMinMs = 6.0f, .lineMaxMs = 20.0f, .sizeOctDown = 1.0f, .sizeOctUp = 0.8f, .naturalDecay = 0.4f, .maxStretch = 2.2f,
                  .loopApMinMs = 1.0f, .loopApMaxMs = 3.0f, .loopApG = 0.55f,
                  .diffStages = 3, .diffMinMs = 0.6f, .diffMaxMs = 3.5f, .diffG = 0.6f,
                  .erTaps = 26, .erStartMs = 1.0f, .erEndMs = 40.0f, .erDensity = 0.75f, .erFade = 2.0f, .erDiffG = 0.62f,
                  .erOut = 0.85f, .erToLate = 0.5f, .directToLate = 0.35f, .lateOut = 0.7f,
                  .lowCutHz = 100.0f, .highCutHz = 15000.0f, .dampHz = 7000.0f, .hfRatio = 0.6f, .bassHz = 250.0f, .bassRatio = 1.0f,
                  .modDepthMs = 0.2f, .modRateHz = 1.0f, .width = 1.15f, .gainDb = 4.9f, .seed = 79 },
        // Bloom: swelling, slowly building wash (rising swell taps into a long, diffuse FDN).
        TypeDef { .lineMinMs = 40.0f, .lineMaxMs = 135.0f, .sizeOctDown = 0.9f, .sizeOctUp = 0.6f, .naturalDecay = 3.0f, .maxStretch = 1.5f,
                  .loopApMinMs = 5.0f, .loopApMaxMs = 14.0f, .loopApG = 0.62f,
                  .diffStages = 6, .diffMinMs = 4.0f, .diffMaxMs = 24.0f, .diffG = 0.7f,
                  .erTaps = 28, .erStartMs = 15.0f, .erEndMs = 600.0f, .erDensity = 1.0f, .erFade = 0.0f, .erSwell = true, .erDecayRef = 2.0f, .erDiffG = 0.5f,
                  .erOut = 0.0f, .erToLate = 1.0f, .directToLate = 0.15f, .lateOut = 1.0f,
                  .lowCutHz = 60.0f, .highCutHz = 11000.0f, .dampHz = 4200.0f, .hfRatio = 0.6f, .bassHz = 250.0f, .bassRatio = 1.1f,
                  .modDepthMs = 1.4f, .modRateHz = 0.33f, .width = 1.2f, .gainDb = 4.0f, .seed = 97 },
    };
    return defs[std::clamp ((int) t, 0, Reverb::kNumTypes - 1)];
}

struct Rng
{
    std::uint32_t s;
    float next() noexcept // [0, 1)
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (float) (s >> 8) * (1.0f / 16777216.0f);
    }
};

inline bool isPrime (int n) noexcept
{
    if (n < 2) return false;
    for (int d = 2; d * d <= n; ++d)
        if (n % d == 0) return false;
    return true;
}

/** Smallest prime >= n not already in used[0..count). */
inline int uniquePrime (int n, const int* used, int count) noexcept
{
    for (n = std::max (n, 2);; ++n)
    {
        if (! isPrime (n)) continue;
        bool taken = false;
        for (int i = 0; i < count; ++i)
            taken = taken || used[i] == n;
        if (! taken) return n;
    }
}

inline int nextPow2 (int n) noexcept { int p = 1; while (p < n) p <<= 1; return p; }
inline float hadamardSign (int r, int c) noexcept { return (std::popcount ((unsigned) (r & c)) & 1) ? -1.0f : 1.0f; }

/** Nominal (size 0.5) length of FDN line i: geometric spread with a deterministic jitter. */
inline float lineMs (const TypeDef& d, int i) noexcept
{
    const float u = (float) i / (float) (kLines - 1);
    const float jitter = 1.0f + 0.06f * std::sin ((float) i * 2.3999632f + 0.7f);
    return d.lineMinMs * std::pow (d.lineMaxMs / d.lineMinMs, u) * jitter;
}

inline float decayStretch (const TypeDef& d, float decay) noexcept
{
    return std::clamp (std::sqrt (decay / d.naturalDecay), 1.0f, d.maxStretch);
}

inline float sizeScale (const TypeDef& d, float size) noexcept
{
    const float x = (std::clamp (size, 0.0f, 1.0f) - 0.5f) * 2.0f;
    return fastExp2 (x < 0.0f ? x * d.sizeOctDown : x * d.sizeOctUp);
}

inline float readLinear (const float* buf, int mask, int idx, float delay) noexcept
{
    const int di = (int) delay;
    const float f = delay - (float) di;
    const float a = buf[(idx - di) & mask];
    const float b = buf[(idx - di - 1) & mask];
    return a + f * (b - a);
}

/** 4-point Hermite read; wp = index the current sample will be written to (not yet written), delay >= 2. */
inline float readHermite (const float* buf, int mask, int wp, float delay) noexcept
{
    const int di = (int) delay;
    const float f = delay - (float) di;
    const int i0 = wp - di;
    const float xm1 = buf[(i0 + 1) & mask], x0 = buf[i0 & mask], x1 = buf[(i0 - 1) & mask], x2 = buf[(i0 - 2) & mask];
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * f + c2) * f + c1) * f + x0;
}

inline std::complex<double> tptLowpass (double g, double w) noexcept
{
    const std::complex<double> z1 = std::polar (1.0, -w);
    return g * (1.0 + z1) / ((1.0 + g) + (g - 1.0) * z1);
}

inline float tptG (double hz, double fs) noexcept
{
    const double g = std::tan (kPi * std::clamp (hz, 1.0, 0.45 * fs) / fs);
    return (float) (g / (1.0 + g));
}

struct ToneSettings { float lowCut, highCut, damp, hfRatio, bassRatio; };

inline ToneSettings toneFor (const TypeDef& d, float tone, double fs) noexcept
{
    tone = std::clamp (tone, -1.0f, 1.0f);
    ToneSettings t;
    t.lowCut   = std::clamp (d.lowCutHz * std::exp2 (1.0f * tone), 20.0f, 400.0f);
    t.highCut  = std::min ({ d.highCutHz * std::exp2 (1.0f * tone), 20000.0f, (float) (0.45 * fs) });
    t.damp     = std::min (d.dampHz * std::exp2 (1.2f * tone), (float) (0.45 * fs));
    t.hfRatio  = std::clamp (d.hfRatio * std::exp2 (0.9f * tone), 0.08f, 1.0f);
    t.bassRatio = std::clamp (d.bassRatio * std::exp2 (-0.15f * tone), 0.5f, 1.6f);
    return t;
}
} // namespace reverb_detail

using namespace reverb_detail;

//==============================================================================
// Slot: one complete reverb engine of a given Type.
//==============================================================================
struct Reverb::Slot
{
    enum class State { idle, active, releasing, killing };

    State state = State::idle;
    Reverb::Type type = Reverb::Type::hall;
    const TypeDef* def = &typeDef (Reverb::Type::hall);
    double fs = 48000.0;

    // FDN lines
    std::vector<float> lineBuf;
    int lineOffset[kLines] {}, lineMask[kLines] {};
    int wp = 0, wpMask = 0; // wp runs modulo the largest line size; each line uses wp & lineMask[i]
    float nominal[kLines] {}, curDelay[kLines] {}, delayInc[kLines] {};
    float gMid[kLines] {}, gLowDiff[kLines] {}, hiR[kLines] {}, hiRc[kLines] {};
    float s1[kLines] {}, s2[kLines] {};
    float G1 = 0.0f, G2 = 0.0f;
    float inj[kLines] {}, outL[kLines] {}, outR[kLines] {}, mixSign[kLines] {};
    int perm[kLines] {};
    float lfoS[kLines] {}, lfoC[kLines] {}, lfoCw[kLines] {}, lfoSw[kLines] {}, modUnit[kLines] {}, modDepth[kLines] {};

    // In-loop allpasses
    std::vector<float> apBuf;
    int apSize = 1, apMask = 0, apPos = 0;
    int apLen[kLines] {};
    float apG = 0.5f;

    // Input diffusion (stereo)
    std::vector<float> diffBuf;
    int diffSize = 1, diffMask = 0, diffPos = 0, nDiff = 0;
    int diffLen[2][kMaxDiff] {};
    float diffG = 0.6f, diffGs[kMaxDiff] {}, diffGCur[kMaxDiff] {}, diffGInc[kMaxDiff] {};

    // Early reflections
    int nTaps = 0;
    float tapT[kMaxTaps] {}, tapGL[kMaxTaps] {}, tapGR[kMaxTaps] {};
    int tapCh[kMaxTaps] {};
    float maxTapT = 0.0f;
    float tapScale = 1.0f, tapScaleStart = 1.0f;
    std::vector<float> erBuf;
    int erSize = 1, erMask = 0, erPos = 0;
    int erLen[2][kErDiff] {};

    // Spring
    std::vector<float> springLine, springRing;
    int springSize = 1, springMask = 0, springWp = 0;
    int ringSize = 1, ringMask = 0, ringPos = 0, springK = 1;
    float springNominal[kSpringLoops] {}, springCur[kSpringLoops] {}, springInc[kSpringLoops] {};
    float springA[kSpringLoops] {}, springG[kSpringLoops] {}, springLp[kSpringLoops] {};
    // In-loop 2-pole Butterworth LP (TPT SVF) just above the dispersion corner: tames the
    // long-ringing ridge at fc while staying flat (< 0.02 dB) at 1 kHz.
    float fcS1[kSpringLoops] {}, fcS2[kSpringLoops] {};
    float fcA1 = 1.0f, fcA2 = 0.0f, fcA3 = 0.0f;
    float springLfoS[kSpringLoops] {}, springLfoC[kSpringLoops] {}, springLfoCw[kSpringLoops] {}, springLfoSw[kSpringLoops] {};
    float springModUnit = 0.0f, springMod = 0.0f, springGLp = 0.0f, springR = 1.0f;

    // Output
    Svf lowCut, highCut, springBandLimit; // springBandLimit: extra 2nd-order LP (spring only -> 4th order)
    float lateGain = 0.0f, lateGainInc = 0.0f, lateGainTarget = 0.0f;
    float typeGain = 1.0f;

    // Crossfade / lifetime
    float inGain = 1.0f, inTarget = 1.0f, inStep = 0.0f; int inSteps = 0;
    float outGain = 1.0f, outStep = 0.0f; int outSteps = 0;
    int offSamples = 0, quietSamples = 0;
    float pipeline = 0.0f;

    // Coefficient cache
    float lastDecay = -1.0f, lastSize = -1.0f, lastTone = -9.0f, lastMod = -1.0f;
    bool fresh = true, justConfigured = true;

    //--------------------------------------------------------------------------
    void allocate (double sampleRate)
    {
        fs = sampleRate;
        float maxLine[kLines] {}, maxAp = 0, maxDiff = 0, maxSpring = 0, maxMod = 0;
        for (int t = 0; t < Reverb::kNumTypes; ++t)
        {
            const auto& d = typeDef ((Reverb::Type) t);
            const float up = std::exp2 (d.sizeOctUp) * d.maxStretch;
            if (d.spring)
                maxSpring = std::max (maxSpring, d.lineMaxMs * up);
            else
                for (int i = 0; i < kLines; ++i)
                    maxLine[i] = std::max (maxLine[i], lineMs (d, i) * up);
            maxAp = std::max (maxAp, d.loopApMaxMs);
            maxDiff = std::max (maxDiff, d.diffMaxMs);
            maxMod = std::max (maxMod, d.modDepthMs);
        }
        const auto ms = [this] (float m) { return (int) std::ceil (m * 0.001 * fs); };
        int total = 0, largest = 1;
        for (int i = 0; i < kLines; ++i)
        {
            const int size = nextPow2 (ms (maxLine[i] * 1.03f + maxMod * 1.5f) + 16);
            lineOffset[i] = total;
            lineMask[i] = size - 1;
            total += size;
            largest = std::max (largest, size);
        }
        wpMask = largest - 1;
        lineBuf.assign ((size_t) total, 0.0f);

        apSize = nextPow2 (ms (maxAp * 1.12f) + 4);
        apMask = apSize - 1;
        apBuf.assign ((size_t) (kLines * apSize), 0.0f);

        diffSize = nextPow2 (ms (maxDiff * 1.2f) + 4);
        diffMask = diffSize - 1;
        diffBuf.assign ((size_t) (2 * kMaxDiff * diffSize), 0.0f);

        erSize = nextPow2 (ms (6.0f) + 4);
        erMask = erSize - 1;
        erBuf.assign ((size_t) (2 * kErDiff * erSize), 0.0f);

        springSize = nextPow2 (ms (maxSpring * 1.12f + 1.0f) + 16);
        springMask = springSize - 1;
        springLine.assign ((size_t) (kSpringLoops * springSize), 0.0f);
        springK = std::max (1, (int) std::lround (fs / (2.0 * kSpringFcHz)));
        ringSize = nextPow2 (springK + 1);
        ringMask = ringSize - 1;
        springRing.assign ((size_t) (kSpringLoops * kSpringSections * ringSize), 0.0f);

        lowCut.prepare (fs, 2);
        highCut.prepare (fs, 2);
        springBandLimit.prepare (fs, 2);
    }

    void clear() noexcept
    {
        std::fill (lineBuf.begin(), lineBuf.end(), 0.0f);
        std::fill (apBuf.begin(), apBuf.end(), 0.0f);
        std::fill (diffBuf.begin(), diffBuf.end(), 0.0f);
        std::fill (erBuf.begin(), erBuf.end(), 0.0f);
        std::fill (springLine.begin(), springLine.end(), 0.0f);
        std::fill (springRing.begin(), springRing.end(), 0.0f);
        std::fill (std::begin (s1), std::end (s1), 0.0f);
        std::fill (std::begin (s2), std::end (s2), 0.0f);
        std::fill (std::begin (springLp), std::end (springLp), 0.0f);
        std::fill (std::begin (fcS1), std::end (fcS1), 0.0f);
        std::fill (std::begin (fcS2), std::end (fcS2), 0.0f);
        lowCut.reset();
        highCut.reset();
        springBandLimit.reset();
        offSamples = quietSamples = 0;
        fresh = true;
    }

    void configure (Reverb::Type t) noexcept
    {
        type = t;
        def = &typeDef (t);
        const auto& d = *def;
        const auto toSamples = [this] (float m) { return (int) std::lround (m * 0.001 * fs); };

        // --- FDN line lengths: geometric spread, jittered, unique primes at the nominal size.
        int used[kLines + kLines + 2 * kMaxDiff + 8] {};
        int nUsed = 0;
        for (int i = 0; i < kLines; ++i)
        {
            const int len = uniquePrime (std::max (8, toSamples (lineMs (d, i))), used, nUsed);
            used[nUsed++] = len;
            nominal[i] = (float) len;
        }
        // In-loop allpasses: permuted so short lines don't always get short allpasses.
        for (int i = 0; i < kLines; ++i)
        {
            const int p = (i * 7 + 3) & (kLines - 1);
            const float u = (float) p / (float) (kLines - 1);
            const float m = d.loopApMinMs * std::pow (d.loopApMaxMs / d.loopApMinMs, u);
            apLen[i] = std::min (apSize - 2, uniquePrime (std::max (3, toSamples (m)), used, nUsed));
            used[nUsed++] = apLen[i];
        }
        apG = d.loopApG;

        // Injection / output / mixing sign patterns (Hadamard rows -> mutually orthogonal).
        for (int i = 0; i < kLines; ++i)
        {
            inj[i] = 0.5f * hadamardSign (5, i);
            outL[i] = 0.25f * hadamardSign (3, i);
            outR[i] = 0.25f * hadamardSign (12, i);
            mixSign[i] = 0.25f * hadamardSign (9, i); // 1/4 = Hadamard normalisation
            perm[i] = (i * 5 + 3) & (kLines - 1);
        }

        // LFOs
        for (int i = 0; i < kLines; ++i)
        {
            const float fr = std::fmod ((float) i * 0.618034f, 1.0f);
            const double rate = d.modRateHz * (0.55 + 0.9 * fr);
            const double w = kTwoPi * rate / fs;
            lfoCw[i] = (float) std::cos (w);
            lfoSw[i] = (float) std::sin (w);
            const float ph = (float) i * 2.3999632f;
            lfoS[i] = std::sin (ph);
            lfoC[i] = std::cos (ph);
            modUnit[i] = (float) (d.modDepthMs * 0.001 * fs) * (0.7f + 0.6f * std::fmod ((float) i * 0.381966f, 1.0f));
        }

        // Input diffusion
        nDiff = std::min (d.diffStages, kMaxDiff);
        diffG = d.diffG;
        for (int s = 0; s < nDiff; ++s)
        {
            const float u = nDiff > 1 ? (float) s / (float) (nDiff - 1) : 0.0f;
            const float m = d.diffMinMs * std::pow (d.diffMaxMs / d.diffMinMs, u);
            for (int ch = 0; ch < 2; ++ch)
            {
                const int len = uniquePrime (std::max (3, toSamples (m * (ch == 0 ? 1.0f : 1.083f))), used, nUsed);
                diffLen[ch][s] = std::min (diffSize - 2, len);
                used[nUsed++] = len;
            }
        }

        // Early reflections (or Bloom swell taps)
        Rng rng { d.seed * 2654435761u + 12345u };
        nTaps = std::min (d.erTaps, kMaxTaps);
        float energy = 0.0f;
        maxTapT = 0.0f;
        for (int k = 0; k < nTaps; ++k)
        {
            const float u = std::clamp (((float) k + 0.5f + 0.7f * (rng.next() - 0.5f)) / (float) nTaps, 0.0f, 1.0f);
            const float tu = std::pow (u, d.erDensity);
            tapT[k] = (float) ((d.erStartMs + (d.erEndMs - d.erStartMs) * tu) * 0.001 * fs);
            maxTapT = std::max (maxTapT, tapT[k]);
            float g = d.erSwell ? (0.12f + tu) : std::exp (-d.erFade * tu);
            g *= 0.7f + 0.3f * rng.next();
            if (k > 0 && rng.next() < 0.45f)
                g = -g;
            const float side = (k & 1) ? 1.0f : -1.0f;
            const float pan = side * (0.15f + 0.6f * rng.next());
            const auto [gl, gr] = equalPowerPan (pan);
            tapGL[k] = g * gl;
            tapGR[k] = g * gr;
            tapCh[k] = k & 1;
            energy += g * g;
        }
        if (energy > 0.0f)
        {
            const float norm = std::sqrt (1.5f / energy);
            for (int k = 0; k < nTaps; ++k) { tapGL[k] *= norm; tapGR[k] *= norm; }
        }
        static constexpr float erMs[2][kErDiff] = { { 1.3f, 3.7f }, { 1.7f, 4.3f } };
        for (int ch = 0; ch < 2; ++ch)
            for (int s = 0; s < kErDiff; ++s)
                erLen[ch][s] = std::min (erSize - 2, std::max (2, toSamples (erMs[ch][s])));

        // Spring loops
        for (int i = 0; i < kSpringLoops; ++i)
        {
            const float u = (float) i / (float) (kSpringLoops - 1);
            springNominal[i] = (float) std::max (16, toSamples (d.lineMinMs + (d.lineMaxMs - d.lineMinMs) * u));
            springA[i] = 0.66f + 0.04f * (float) i;
            const double w = kTwoPi * d.modRateHz * (0.8 + 0.3 * i) / fs;
            springLfoCw[i] = (float) std::cos (w);
            springLfoSw[i] = (float) std::sin (w);
            springLfoS[i] = std::sin ((float) i * 2.1f);
            springLfoC[i] = std::cos ((float) i * 2.1f);
        }
        springModUnit = (float) (d.modDepthMs * 0.001 * fs);

        typeGain = dbToGain (d.gainDb);
        lastDecay = lastSize = lastMod = -1.0f;
        lastTone = -9.0f;
        fresh = true;
    }

    //--------------------------------------------------------------------------
    void startInputFade (float target, int samples) noexcept
    {
        inTarget = target;
        inSteps = std::max (1, samples);
        inStep = (inTarget - inGain) / (float) inSteps;
    }

    void startKill (int samples) noexcept
    {
        state = State::killing;
        outSteps = std::max (1, samples);
        outStep = -outGain / (float) outSteps;
    }

    void activate (int fadeSamples) noexcept
    {
        state = State::active;
        inGain = 0.0f;
        startInputFade (1.0f, fadeSamples);
        outGain = 1.0f; outStep = 0.0f; outSteps = 0;
        offSamples = quietSamples = 0;
    }

    //--------------------------------------------------------------------------
    void updateCoefficients (const Control& c, int n) noexcept
    {
        const auto& d = *def;
        const float sizeOnly = sizeScale (d, c.size);
        const float sc = sizeOnly * decayStretch (d, c.decay);

        if (! juce::exactlyEqual (c.mod, lastMod))
        {
            lastMod = c.mod;
            for (int i = 0; i < kLines; ++i)
                modDepth[i] = c.mod * modUnit[i];
            springMod = c.mod * springModUnit;
        }

        const bool toneChanged = ! juce::exactlyEqual (c.tone, lastTone);
        const bool changed = toneChanged || ! juce::exactlyEqual (c.decay, lastDecay) || ! juce::exactlyEqual (c.size, lastSize);
        if (changed)
        {
            lastDecay = c.decay; lastSize = c.size; lastTone = c.tone;
            const auto ts = toneFor (d, c.tone, fs);
            const float T = std::max (0.05f, c.decay);
            const float kMid = -kRt60Exp / (float) (T * fs);
            const float kLow = kMid / ts.bassRatio;
            const float kHigh = kMid / ts.hfRatio;
            const double w1k = kTwoPi * 1000.0 / fs;

            if (toneChanged || fresh)
            {
                lowCut.setParameters (SvfMode::highPass, ts.lowCut, 0.7071);
                highCut.setParameters (SvfMode::lowPass, ts.highCut, 0.7071);
                springBandLimit.setParameters (SvfMode::lowPass, std::min (ts.highCut * 1.15, 0.45 * fs), 0.7071);
            }

            float norm;
            if (! d.spring)
            {
                G1 = tptG (d.bassHz, fs);
                G2 = tptG (ts.damp, fs);
                const auto L1 = tptLowpass (G1 / (1.0 - G1), w1k);
                const auto L2 = tptLowpass (G2 / (1.0 - G2), w1k);
                float lenSum = 0.0f;
                for (int i = 0; i < kLines; ++i)
                {
                    const float len = nominal[i] * sc + (float) apLen[i];
                    lenSum += len;
                    float gm = fastExp2 (kMid * len);
                    float gl = fastExp2 (kLow * len);
                    const float gh = fastExp2 (kHigh * len);
                    const float r = std::min (1.0f, gh / gm);
                    // Normalise so the loop gain at 1 kHz is exactly gm.
                    const auto H = ((double) gm + (double) (gl - gm) * L1) * ((double) r + (1.0 - r) * L2);
                    const float corr = (float) (gm / std::max (1.0e-9, std::abs (H)));
                    gm = std::min (gm * corr, 0.99995f);
                    gl = std::min (gl * corr, 0.99995f);
                    gMid[i] = gm;
                    gLowDiff[i] = gl - gm;
                    hiR[i] = r;
                    hiRc[i] = 1.0f - r;
                }
                // Energy normalisation: steady-state output power ~ T / mean loop length.
                const float meanLen = lenSum / (float) kLines;
                norm = std::sqrt (meanLen / (float) (0.05 * fs));
            }
            else
            {
                springGLp = tptG (ts.damp, fs);
                const auto L2 = tptLowpass (springGLp / (1.0 - springGLp), w1k);
                const double gFc = std::tan (kPi * std::min (1.05 * fs / (2.0 * springK), 0.45 * fs) / fs);
                const double kFc = 1.4142135623730951;
                fcA1 = (float) (1.0 / (1.0 + gFc * (gFc + kFc)));
                fcA2 = (float) gFc * fcA1;
                fcA3 = (float) gFc * fcA2;
                const double om = std::tan (kPi * 1000.0 / fs) / gFc;
                const double fcMag = 1.0 / std::abs (std::complex<double> (1.0 - om * om, kFc * om));
                const double theta = w1k * springK;
                float lenSum = 0.0f;
                for (int i = 0; i < kSpringLoops; ++i)
                {
                    const double a = springA[i];
                    const double gd = kSpringSections * springK * (1.0 - a * a) / (1.0 + 2.0 * a * std::cos (theta) + a * a);
                    const float len = springNominal[i] * sc + (float) gd;
                    lenSum += len;
                    const float gm = fastExp2 (kMid * len);
                    const float gh = fastExp2 (kHigh * len);
                    if (i == 0)
                        springR = std::min (1.0f, gh / gm);
                    const double hMag = std::abs ((double) springR + (1.0 - springR) * L2) * fcMag;
                    springG[i] = (float) std::min (0.9995, gm / std::max (1.0e-6, hMag));
                }
                norm = std::sqrt ((lenSum / kSpringLoops) / (float) (0.05 * fs));
            }

            // Partial decay compensation: louder for longer decays, but only +4 dB at 20 s.
            const float decayComp = std::pow (2.0f / T, 0.3f);

            // Input diffusers must not out-ring the tail: cap each stage's own RT60 at T / 3.
            for (int st = 0; st < nDiff; ++st)
                diffGs[st] = std::min (diffG, fastExp2 (3.0f * kMid * (float) diffLen[0][st]));
            lateGainTarget = d.lateOut * norm * decayComp;
        }

        // Early-reflection / swell tap span: follows size, shrinks for short decays (so the
        // build never outlasts the requested RT60), slew-limited to keep Doppler under ~10 %.
        const float decayFactor = d.erDecayRef > 0.0f ? std::clamp (c.decay / d.erDecayRef, 0.35f, 1.0f) : 1.0f;
        const float tapTarget = sizeOnly * decayFactor;
        tapScaleStart = tapScale;

        // Per-sample ramps over this control block. Delay changes are slew-limited to
        // kMaxDelaySlew samples/sample (= max pitch deviation while sweeping Size).
        constexpr float kMaxDelaySlew = 0.05f;
        const float invN = 1.0f / (float) n;
        if (fresh)
        {
            for (int i = 0; i < kLines; ++i) curDelay[i] = nominal[i] * sc;
            for (int i = 0; i < kSpringLoops; ++i) springCur[i] = springNominal[i] * sc;
            lateGain = lateGainTarget;
            tapScale = tapScaleStart = tapTarget;
            justConfigured = true;
            fresh = false;
        }
        else
        {
            const float maxStep = 0.1f * (float) n / std::max (1.0f, maxTapT);
            tapScale += std::clamp (tapTarget - tapScale, -maxStep, maxStep);
        }
        for (int i = 0; i < kLines; ++i)
            delayInc[i] = std::clamp ((nominal[i] * sc - curDelay[i]) * invN, -kMaxDelaySlew, kMaxDelaySlew);
        for (int i = 0; i < kSpringLoops; ++i)
            springInc[i] = std::clamp ((springNominal[i] * sc - springCur[i]) * invN, -kMaxDelaySlew, kMaxDelaySlew);
        lateGainInc = (lateGainTarget - lateGain) * invN;
        for (int st = 0; st < nDiff; ++st)
        {
            if (justConfigured) diffGCur[st] = diffGs[st];
            diffGInc[st] = (diffGs[st] - diffGCur[st]) * invN;
        }
        justConfigured = false;

        pipeline = maxTapT * tapScale;
        for (int s = 0; s < nDiff; ++s)
            pipeline += (float) diffLen[0][s];
    }

    //--------------------------------------------------------------------------
    inline void fdnTick (float inL, float inR, float& yL, float& yR) noexcept
    {
        float y[kLines];
        for (int i = 0; i < kLines; ++i)
        {
            curDelay[i] += delayInc[i];
            const float dly = std::max (3.0f, curDelay[i] + modDepth[i] * lfoS[i]);
            y[i] = readHermite (lineBuf.data() + lineOffset[i], lineMask[i], wp, dly);
        }
        for (int i = 0; i < kLines; ++i)
        {
            const float s = lfoS[i] * lfoCw[i] + lfoC[i] * lfoSw[i];
            lfoC[i] = lfoC[i] * lfoCw[i] - lfoS[i] * lfoSw[i];
            lfoS[i] = s;
        }
        float oL = 0.0f, oR = 0.0f;
        for (int i = 0; i < kLines; ++i)
        {
            oL += outL[i] * y[i];
            oR += outR[i] * y[i];
        }
        yL = oL;
        yR = oR;

        // In-loop allpass + 3-band decay
        float z[kLines];
        for (int i = 0; i < kLines; ++i)
        {
            float* ap = apBuf.data() + i * apSize;
            const float dd = ap[(apPos - apLen[i]) & apMask];
            const float w = y[i] - apG * dd;
            ap[apPos] = w;
            const float x = apG * w + dd;

            const float v1 = (x - s1[i]) * G1;
            const float lp1 = v1 + s1[i];
            s1[i] = lp1 + v1;
            const float a = gMid[i] * x + gLowDiff[i] * lp1;

            const float v2 = (a - s2[i]) * G2;
            const float lp2 = v2 + s2[i];
            s2[i] = lp2 + v2;
            z[i] = hiR[i] * a + hiRc[i] * lp2;
        }
        apPos = (apPos + 1) & apMask;

        // Fast Walsh-Hadamard transform (16)
        for (int h = 1; h < kLines; h <<= 1)
            for (int i = 0; i < kLines; i += h << 1)
                for (int j = i; j < i + h; ++j)
                {
                    const float a = z[j], b = z[j + h];
                    z[j] = a + b;
                    z[j + h] = a - b;
                }

        for (int i = 0; i < kLines; ++i)
        {
            const int dst = perm[i];
            const float in = (dst & 1) ? inR : inL;
            lineBuf[(size_t) (lineOffset[dst] + (wp & lineMask[dst]))] = z[i] * mixSign[i] + inj[dst] * in;
        }
        wp = (wp + 1) & wpMask;
    }

    inline void springTick (float in, float& yL, float& yR) noexcept
    {
        float y[kSpringLoops];
        for (int i = 0; i < kSpringLoops; ++i)
        {
            springCur[i] += springInc[i];
            const float dly = std::max (3.0f, springCur[i] + springMod * springLfoS[i]);
            y[i] = readHermite (springLine.data() + i * springSize, springMask, springWp, dly);
            const float s = springLfoS[i] * springLfoCw[i] + springLfoC[i] * springLfoSw[i];
            springLfoC[i] = springLfoC[i] * springLfoCw[i] - springLfoS[i] * springLfoSw[i];
            springLfoS[i] = s;
        }
        const float sum = (2.0f / 3.0f) * (y[0] + y[1] + y[2]); // 3x3 Householder coupling
        float out[kSpringLoops];
        const int rd = (ringPos - springK) & ringMask;
        for (int i = 0; i < kSpringLoops; ++i)
        {
            const float fb = y[i] - sum;
            const float v = (fb - springLp[i]) * springGLp;
            const float lp = v + springLp[i];
            springLp[i] = lp + v;
            const float sh = springR * fb + (1.0f - springR) * lp; // high-shelf damping
            const float v3 = sh - fcS2[i];
            const float v1 = fcA1 * fcS1[i] + fcA2 * v3;
            const float lp2 = fcS2[i] + fcA2 * fcS1[i] + fcA3 * v3;
            fcS1[i] = 2.0f * v1 - fcS1[i];
            fcS2[i] = 2.0f * lp2 - fcS2[i];
            float u = 0.55f * in + springG[i] * lp2;

            // Dispersion: cascade of "stretched" first-order allpasses (a + z^-K) / (1 + a z^-K).
            const float a = springA[i];
            float* ring = springRing.data() + (size_t) (i * kSpringSections * ringSize);
            for (int s = 0; s < kSpringSections; ++s, ring += ringSize)
            {
                const float dd = ring[rd];
                const float w = u - a * dd;
                ring[ringPos] = w;
                u = a * w + dd;
            }
            springLine[(size_t) (i * springSize + springWp)] = u;
            out[i] = u;
        }
        ringPos = (ringPos + 1) & ringMask;
        springWp = (springWp + 1) & springMask;
        yL = 0.8f * out[0] + 0.45f * out[2];
        yR = 0.8f * out[1] + 0.45f * out[2];
    }

    //--------------------------------------------------------------------------
    /** Processes n (<= kCtl) base-rate samples, ADDING into accL/accR. */
    void process (const Control& c, const float* hL, const float* hR, int hMask, int hIdx, int n,
                  float* accL, float* accR) noexcept
    {
        const auto& d = *def;
        updateCoefficients (c, n);

        const float invN = 1.0f / (float) n;
        const float pdInc = (c.pdEnd - c.pdStart) * invN;
        const float sc0 = tapScaleStart, scInc = (tapScale - tapScaleStart) * invN;
        const float w0 = c.widthStart * d.width, w1 = c.width * d.width;
        const float wInc = (w1 - w0) * invN;
        const bool hasEr = nTaps > 0;
        const float erOut = d.erOut, erToLate = d.erToLate, directToLate = d.directToLate;
        float peak = 0.0f;

        for (int s = 0; s < n; ++s)
        {
            const float pd = c.pdStart + pdInc * (float) s;
            const float sc = sc0 + scInc * (float) s;
            const int hi = hIdx + s;

            if (inSteps > 0)
            {
                inGain += inStep;
                if (--inSteps == 0) inGain = inTarget;
            }
            const float ig = inGain;

            float dL = 0.0f, dR = 0.0f, eL = 0.0f, eR = 0.0f;
            if (ig > 0.0f)
            {
                dL = readLinear (hL, hMask, hi, pd) * ig;
                dR = readLinear (hR, hMask, hi, pd) * ig;
                if (hasEr)
                {
                    for (int k = 0; k < nTaps; ++k)
                    {
                        const float v = readLinear (tapCh[k] ? hR : hL, hMask, hi, pd + sc * tapT[k]);
                        eL += tapGL[k] * v;
                        eR += tapGR[k] * v;
                    }
                    eL *= ig;
                    eR *= ig;
                }
            }

            float lateL, lateR;
            float inLateL = directToLate * dL + erToLate * eL;
            float inLateR = directToLate * dR + erToLate * eR;
            if (d.spring)
            {
                springTick (0.5f * (inLateL + inLateR), lateL, lateR);
            }
            else
            {
                // Input diffusion
                for (int st = 0; st < nDiff; ++st)
                {
                    float* bl = diffBuf.data() + (size_t) (st * diffSize);
                    float* br = diffBuf.data() + (size_t) ((kMaxDiff + st) * diffSize);
                    const float ddl = bl[(diffPos - diffLen[0][st]) & diffMask];
                    const float ddr = br[(diffPos - diffLen[1][st]) & diffMask];
                    diffGCur[st] += diffGInc[st];
                    const float g = diffGCur[st];
                    const float wl = inLateL - g * ddl;
                    const float wr = inLateR - g * ddr;
                    bl[diffPos] = wl;
                    br[diffPos] = wr;
                    inLateL = g * wl + ddl;
                    inLateR = g * wr + ddr;
                }
                diffPos = (diffPos + 1) & diffMask;
                fdnTick (inLateL, inLateR, lateL, lateR);
            }

            lateGain += lateGainInc;
            float oL = lateGain * lateL, oR = lateGain * lateR;

            if (erOut > 0.0f)
            {
                const float g = d.erDiffG;
                for (int st = 0; st < kErDiff; ++st)
                {
                    float* bl = erBuf.data() + (size_t) (st * erSize);
                    float* br = erBuf.data() + (size_t) ((kErDiff + st) * erSize);
                    const float ddl = bl[(erPos - erLen[0][st]) & erMask];
                    const float ddr = br[(erPos - erLen[1][st]) & erMask];
                    const float wl = eL - g * ddl;
                    const float wr = eR - g * ddr;
                    bl[erPos] = wl;
                    br[erPos] = wr;
                    eL = g * wl + ddl;
                    eR = g * wr + ddr;
                }
                erPos = (erPos + 1) & erMask;
                oL += erOut * eL;
                oR += erOut * eR;
            }

            // Tone
            oL = highCut.processSample (0, lowCut.processSample (0, oL));
            oR = highCut.processSample (1, lowCut.processSample (1, oR));
            if (d.spring)
            {
                oL = springBandLimit.processSample (0, oL);
                oR = springBandLimit.processSample (1, oR);
            }

            // Width (energy compensated)
            const float w = w0 + wInc * (float) s;
            const float mid = 0.5f * (oL + oR), side = 0.5f * (oL - oR) * w;
            const float wg = std::sqrt (2.0f / (1.0f + w * w)) * typeGain;

            if (outSteps > 0)
            {
                outGain += outStep;
                if (--outSteps == 0) outGain = 0.0f;
            }
            const float g = wg * outGain;
            const float l = g * (mid + side), r = g * (mid - side);
            accL[s] += l;
            accR[s] += r;
            peak = std::max (peak, std::max (std::abs (l), std::abs (r)));
        }

        // Flush filter states that could decay into denormals if FTZ is unavailable.
        for (int i = 0; i < kLines; ++i)
        {
            s1[i] = flushDenormal (s1[i]);
            s2[i] = flushDenormal (s2[i]);
        }
        for (int i = 0; i < kSpringLoops; ++i)
        {
            springLp[i] = flushDenormal (springLp[i]);
            fcS1[i] = flushDenormal (fcS1[i]);
            fcS2[i] = flushDenormal (fcS2[i]);
        }

        // Lifetime
        if (state == State::killing && outSteps == 0)
            state = State::idle;
        else if (state == State::releasing)
        {
            if (inSteps == 0 && inGain <= 0.0f)
                offSamples = std::min (offSamples + n, 1 << 30);
            quietSamples = peak < kSlotQuiet ? std::min (quietSamples + n, 1 << 30) : 0;
            if ((float) offSamples > c.pdEnd + pipeline + 0.02f * (float) fs && quietSamples > (int) (0.1 * fs))
                state = State::idle;
        }
        lastPeak = peak;
    }

    float lastPeak = 0.0f;
};

//==============================================================================
// Reverb
//==============================================================================
juce::StringArray Reverb::getTypeNames()
{
    return { "Room", "Chamber", "Plate", "Hall", "Cathedral", "Spring", "Ambience", "Bloom" };
}

Reverb::Reverb()
{
    for (auto& s : slots)
        s = std::make_unique<Slot>();
}

Reverb::~Reverb() = default;

size_t Reverb::getMemoryBytes() const noexcept
{
    size_t n = baseInL.size() + baseInR.size() + baseOutL.size() + baseOutR.size() + histL.size() + histR.size();
    for (const auto& s : slots)
        n += s->lineBuf.size() + s->apBuf.size() + s->diffBuf.size() + s->erBuf.size()
             + s->springLine.size() + s->springRing.size();
    return n * sizeof (float) + 2 * sizeof (Slot);
}

void Reverb::setParams (const Params& p) noexcept
{
    aType.store (std::clamp ((int) p.type, 0, kNumTypes - 1), std::memory_order_relaxed);
    aDecay.store (std::isfinite (p.decaySeconds) ? std::clamp (p.decaySeconds, 0.1f, 20.0f) : 2.0f, std::memory_order_relaxed);
    aSize.store (std::isfinite (p.size) ? std::clamp (p.size, 0.0f, 1.0f) : 0.5f, std::memory_order_relaxed);
    aPredelay.store (std::isfinite (p.predelayMs) ? std::clamp (p.predelayMs, 0.0f, 250.0f) : 0.0f, std::memory_order_relaxed);
    aTone.store (std::isfinite (p.tone) ? std::clamp (p.tone, -1.0f, 1.0f) : 0.0f, std::memory_order_relaxed);
    aWidth.store (std::isfinite (p.width) ? std::clamp (p.width, 0.0f, 1.5f) : 1.0f, std::memory_order_relaxed);
    aMod.store (std::isfinite (p.modulation) ? std::clamp (p.modulation, 0.0f, 1.0f) : 0.3f, std::memory_order_relaxed);
}

Reverb::Params Reverb::getParams() const noexcept
{
    Params p;
    p.type = (Type) aType.load (std::memory_order_relaxed);
    p.decaySeconds = aDecay.load (std::memory_order_relaxed);
    p.size = aSize.load (std::memory_order_relaxed);
    p.predelayMs = aPredelay.load (std::memory_order_relaxed);
    p.tone = aTone.load (std::memory_order_relaxed);
    p.width = aWidth.load (std::memory_order_relaxed);
    p.modulation = aMod.load (std::memory_order_relaxed);
    return p;
}

void Reverb::prepare (double newProcessRate, int newRateFactor, int maxBlockSize)
{
    processRate = newProcessRate > 0.0 ? newProcessRate : 48000.0;
    rateFactor = std::clamp (newRateFactor, 1, 16);
    baseRate = processRate / rateFactor;
    maxBlock = std::max (1, maxBlockSize);
    maxBase = maxBlock / rateFactor + 2;

    // 8th-order Butterworth low-pass for decimation / interpolation.
    static constexpr double qs[4] = { 0.50979558, 0.60134489, 0.89997622, 2.56291545 };
    const double fc = std::min (0.42 * baseRate, 21000.0);
    for (int i = 0; i < 4; ++i)
    {
        downFilter[(size_t) i].prepare (2);
        upFilter[(size_t) i].prepare (2);
        downFilter[(size_t) i].setParameters (BiquadType::lowPass, processRate, fc, qs[i]);
        upFilter[(size_t) i].setParameters (BiquadType::lowPass, processRate, fc, qs[i]);
    }

    baseInL.assign ((size_t) maxBase, 0.0f);
    baseInR.assign ((size_t) maxBase, 0.0f);
    baseOutL.assign ((size_t) maxBase, 0.0f);
    baseOutR.assign ((size_t) maxBase, 0.0f);

    float maxTapMs = 0.0f;
    for (int t = 0; t < kNumTypes; ++t)
    {
        const auto& d = typeDef ((Type) t);
        if (d.erTaps > 0)
            maxTapMs = std::max (maxTapMs, d.erEndMs * std::exp2 (d.sizeOctUp));
    }
    const int histNeeded = (int) std::ceil ((kMaxPredelayMs + maxTapMs * 1.02 + 2.0) * 0.001 * baseRate) + maxBase + 8;
    const int histSize = nextPow2 (histNeeded);
    histMask = histSize - 1;
    histL.assign ((size_t) histSize, 0.0f);
    histR.assign ((size_t) histSize, 0.0f);
    dcBlock.prepare (baseRate, 2, 5.0);

    for (auto& s : slots)
        s->allocate (baseRate);

    aSlow = (float) (1.0 - std::exp (-kCtl / (0.12 * baseRate)));
    aFast = (float) (1.0 - std::exp (-kCtl / (0.04 * baseRate)));

    prepared = true;
    reset();
}

void Reverb::reset()
{
    if (! prepared)
        return;
    for (auto& f : downFilter) f.reset();
    for (auto& f : upFilter) f.reset();
    decimPhase = 0;
    std::fill (histL.begin(), histL.end(), 0.0f);
    std::fill (histR.begin(), histR.end(), 0.0f);
    histWp = 0;
    dcBlock.reset();

    readTargets();
    snapSmoothing();

    current = 0;
    auto& cur = *slots[0];
    cur.configure ((Type) aType.load (std::memory_order_relaxed));
    cur.clear();
    cur.state = Slot::State::active;
    cur.inGain = cur.inTarget = 1.0f;
    cur.inSteps = 0;
    cur.outGain = 1.0f;
    cur.outSteps = 0;
    slots[1]->clear();
    slots[1]->state = Slot::State::idle;

    inputQuiet = outputQuiet = 0;
    silent = false;
}

void Reverb::readTargets() noexcept
{
    tLogDecay = std::log (aDecay.load (std::memory_order_relaxed));
    tSize = aSize.load (std::memory_order_relaxed);
    tPredelay = aPredelay.load (std::memory_order_relaxed);
    tTone = aTone.load (std::memory_order_relaxed);
    tWidth = aWidth.load (std::memory_order_relaxed);
    tMod = aMod.load (std::memory_order_relaxed);
}

void Reverb::snapSmoothing() noexcept
{
    sLogDecay = tLogDecay; sSize = tSize; sPredelay = tPredelay;
    sTone = tTone; sWidth = tWidth; sMod = tMod;
    needsSnap = false;
}

void Reverb::handleTypeChange() noexcept
{
    const auto want = (Type) aType.load (std::memory_order_relaxed);
    auto& cur = *slots[(size_t) current];
    if (want == cur.type && cur.state == Slot::State::active)
        return;

    auto& other = *slots[(size_t) (1 - current)];
    if (other.state == Slot::State::idle)
    {
        const int fade = (int) (0.03 * baseRate);
        other.configure (want);
        other.clear();
        other.activate (fade);
        cur.startInputFade (0.0f, fade);
        if (cur.state == Slot::State::active)
            cur.state = Slot::State::releasing;
        current = 1 - current;
    }
    else if (other.state != Slot::State::killing)
    {
        other.startKill ((int) (0.04 * baseRate));
    }
}

void Reverb::process (const float* inL, const float* inR, float* outL, float* outR, int numSamples) noexcept
{
    if (! prepared)
    {
        std::fill (outL, outL + numSamples, 0.0f);
        std::fill (outR, outR + numSamples, 0.0f);
        return;
    }
    juce::ScopedNoDenormals noDenormals;
    for (int start = 0; start < numSamples; start += maxBlock)
    {
        const int n = std::min (maxBlock, numSamples - start);
        processChunk (inL + start, inR + start, outL + start, outR + start, n);
    }
}

void Reverb::processChunk (const float* inL, const float* inR, float* outL, float* outR, int numSamples) noexcept
{
    readTargets();
    if (needsSnap)
        snapSmoothing();

    // --- Band-limit + decimate
    const int R = rateFactor;
    int nb = 0, ph = decimPhase;
    float peakIn = 0.0f;
    for (int j = 0; j < numSamples; ++j)
    {
        float l = inL[j], r = inR[j];
        peakIn = std::max (peakIn, std::max (std::abs (l), std::abs (r)));
        if (R > 1)
            for (auto& f : downFilter)
            {
                l = f.processSample (0, l);
                r = f.processSample (1, r);
            }
        if (ph == 0)
        {
            baseInL[(size_t) nb] = l;
            baseInR[(size_t) nb] = r;
            ++nb;
        }
        if (++ph == R) ph = 0;
    }
    if (! std::isfinite (peakIn))
        peakIn = 1.0f; // treated as signal; non-finite input is sanitised below

    // --- Silent fast path: nothing in flight and nothing coming in.
    if (silent && peakIn < kInputSilence)
    {
        std::fill (outL, outL + numSamples, 0.0f);
        std::fill (outR, outR + numSamples, 0.0f);
        decimPhase = ph;
        inputQuiet = std::min (inputQuiet + nb, 1 << 30);
        return;
    }
    inputQuiet = peakIn < kInputSilence ? std::min (inputQuiet + nb, 1 << 30) : 0;

    processBase (nb);

    // --- Zero-stuff + interpolate back to processRate
    ph = decimPhase;
    int k = 0;
    const float gain = (float) R;
    for (int j = 0; j < numSamples; ++j)
    {
        float l = 0.0f, r = 0.0f;
        if (ph == 0)
        {
            l = gain * baseOutL[(size_t) k];
            r = gain * baseOutR[(size_t) k];
            ++k;
        }
        if (R > 1)
            for (auto& f : upFilter)
            {
                l = f.processSample (0, l);
                r = f.processSample (1, r);
            }
        outL[j] = l;
        outR[j] = r;
        if (++ph == R) ph = 0;
    }
    decimPhase = ph;
}

void Reverb::processBase (int nb) noexcept
{
    // History (DC-blocked, sanitised)
    for (int s = 0; s < nb; ++s)
    {
        float l = baseInL[(size_t) s], r = baseInR[(size_t) s];
        if (! std::isfinite (l)) l = 0.0f;
        if (! std::isfinite (r)) r = 0.0f;
        const int idx = (histWp + s) & histMask;
        histL[(size_t) idx] = dcBlock.processSample (0, std::clamp (l, -64.0f, 64.0f));
        histR[(size_t) idx] = dcBlock.processSample (1, std::clamp (r, -64.0f, 64.0f));
    }
    const int hIdx0 = histWp;
    histWp = (histWp + nb) & histMask;

    handleTypeChange();

    std::fill (baseOutL.begin(), baseOutL.begin() + nb, 0.0f);
    std::fill (baseOutR.begin(), baseOutR.begin() + nb, 0.0f);

    const float msToSamples = (float) (0.001 * baseRate);
    const auto smooth = [] (float& s, float t, float a, float eps)
    {
        s += (t - s) * a;
        if (std::abs (t - s) < eps) s = t;
    };

    float pipeline = 0.0f;
    for (int start = 0; start < nb; start += kCtl)
    {
        const int n = std::min (kCtl, nb - start);
        Control c;
        c.sizeStart = sSize;
        c.pdStart = sPredelay * msToSamples;
        c.widthStart = sWidth;
        smooth (sLogDecay, tLogDecay, aFast, 1.0e-4f);
        smooth (sSize, tSize, aSlow, 1.0e-5f);
        {
            // Predelay glides (one-pole) with a slew limit of 0.25 samples/sample (max +-25 % pitch).
            const float prev = sPredelay;
            smooth (sPredelay, tPredelay, aSlow, 1.0e-3f);
            const float maxD = 0.25f * (float) n / msToSamples;
            sPredelay = prev + std::clamp (sPredelay - prev, -maxD, maxD);
        }
        smooth (sTone, tTone, aFast, 1.0e-4f);
        smooth (sWidth, tWidth, aFast, 1.0e-4f);
        smooth (sMod, tMod, aFast, 1.0e-4f);
        c.decay = std::exp (sLogDecay);
        c.size = sSize;
        c.pdEnd = sPredelay * msToSamples;
        c.tone = sTone;
        c.width = sWidth;
        c.mod = sMod;

        for (auto& slot : slots)
            if (slot->state != Slot::State::idle)
            {
                slot->process (c, histL.data(), histR.data(), histMask, hIdx0 + start, n,
                               baseOutL.data() + start, baseOutR.data() + start);
                pipeline = std::max (pipeline, c.pdEnd + slot->pipeline);
            }
    }

    // Safety net: a non-finite output (should never happen) clears the engines.
    float peakOut = 0.0f;
    for (int s = 0; s < nb; ++s)
        peakOut = std::max (peakOut, std::max (std::abs (baseOutL[(size_t) s]), std::abs (baseOutR[(size_t) s])));
    if (! std::isfinite (peakOut))
    {
        for (auto& slot : slots)
            slot->clear();
        std::fill (baseOutL.begin(), baseOutL.begin() + nb, 0.0f);
        std::fill (baseOutR.begin(), baseOutR.begin() + nb, 0.0f);
        peakOut = 0.0f;
    }

    outputQuiet = peakOut < kOutputSilence ? std::min (outputQuiet + nb, 1 << 30) : 0;
    silent = (float) inputQuiet > pipeline + 0.05f * (float) baseRate
             && outputQuiet > (int) (0.1 * baseRate);
}

} // namespace st::dsp
