#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <vector>

#include "Filters.h"
#include "Utils.h"

/*  Reverb — algorithmic stereo reverb (8 characters), designed to run per band inside an
    oversampled multiband processor.

    Signal flow (all at the BASE rate = processRate / rateFactor):

        in ──(8th-order Butterworth AA + decimate, if rateFactor > 1)──► DC block ──► history
        history ──► predelay read ──────────────┬──► input diffusion (allpass chain) ──► 16-line FDN ──┐
        history ──► early-reflection taps ──────┤        (or 3 coupled dispersive spring loops)      │
                       └──► ER diffusion ───────┴───────────────────────────────────────── + ◄───────┘
                                                                                             │
                                          tone low-cut / high-cut ◄──────────────────────────┘
                                          width (M/S, energy-compensated) ──► zero-stuff + AA ──► out

    FDN: 16 delay lines, Hadamard mixing followed by a fixed permutation/sign flip (so the
    feedback matrix is orthogonal but not involutory -> no "two-pass" return), per-line
    in-loop allpass for echo-density growth, per-line 3-band decay (low shelf / mid / high
    shelf, TPT one-poles) designed from the RT60 and normalised so the decay at 1 kHz is
    exactly the requested RT60, Hermite-interpolated delay reads with per-line sine LFOs.

    Type changes use two engine "slots": the new type starts in a fresh slot while the old
    one stops receiving input (30 ms crossfade) and keeps ringing out naturally. If the type
    changes again while a tail is still ringing, that tail is faded out over 40 ms first.

    Threading:
      - setParams() / getParams(): lock-free (atomics), callable from any thread at any time.
      - prepare() / reset(): not concurrently with process() (normal prepareToPlay contract).
      - process() / isSilent(): audio thread.
    Denormals: process() installs juce::ScopedNoDenormals itself.
    Memory: everything allocated in prepare(); ~2.6 MB per instance at a 44.1/48 kHz base rate
    (two slots), ~5.3 MB at 96 kHz (see getMemoryBytes()).
    CPU (Apple M3 Pro, one core, 48 kHz base): ~0.7 % per instance (0.9 % with rateFactor 4);
    roughly double while an old tail rings out after a Type change.
*/

namespace st::dsp
{

class Reverb
{
public:
    enum class Type { room, chamber, plate, hall, cathedral, spring, ambience, bloom };
    static constexpr int kNumTypes = 8;

    /** "Room","Chamber","Plate","Hall","Cathedral","Spring","Ambience","Bloom" (enum order). */
    static juce::StringArray getTypeNames();

    struct Params
    {
        Type  type = Type::hall;
        float decaySeconds = 2.0f;   // RT60 (mid band, 1 kHz) of the tail, 0.1 .. 20 s
        float size = 0.5f;           // 0..1 room size (delay-length scaling / early-reflection spread)
        float predelayMs = 10.0f;    // 0..250
        float tone = 0.0f;           // -1 dark .. +1 bright (HF damping + low cut balance)
        float width = 1.0f;          // 0..1.5 stereo width of the wet signal (0 = mono)
        float modulation = 0.3f;     // 0..1 delay-line modulation (chorus-y smoothing of the tail)
    };

    Reverb();
    ~Reverb();
    Reverb (const Reverb&) = delete;
    Reverb& operator= (const Reverb&) = delete;

    /** processRate = the rate process() is called at; rateFactor = processRate / baseRate
        (1..8). The reverb core runs at the base rate. Allocates. */
    void prepare (double processRate, int rateFactor, int maxBlockSize);

    /** Clears all state (tails, filters) and snaps smoothed parameters to their targets. */
    void reset();

    /** Lock-free; any thread. Values are clamped. Continuous parameters are smoothed on the
        audio thread (size and predelay glide, ~120 ms; others ~40 ms); a Type change
        crossfades to a fresh engine while the old tail rings out. */
    void setParams (const Params&) noexcept;
    Params getParams() const noexcept;

    /** Wet-only stereo output at processRate. in and out must not alias. Any numSamples
        (blocks larger than maxBlockSize are split internally). */
    void process (const float* inL, const float* inR, float* outL, float* outR, int numSamples) noexcept;

    /** True once the input has been silent long enough for everything in flight to have
        arrived and the output has stayed below -90 dBFS for 100 ms. While silent and the
        input stays silent, process() writes zeros without running the network. */
    bool isSilent() const noexcept { return silent; }

    int getLatencySamples() const noexcept { return 0; }

    double getBaseRate() const noexcept { return baseRate; }
    /** Heap memory held by this instance after prepare() (bytes). */
    size_t getMemoryBytes() const noexcept;
    int getRateFactor() const noexcept  { return rateFactor; }

private:
    struct Slot;
    struct Control
    {
        float decay = 2.0f, size = 0.5f, tone = 0.0f, mod = 0.3f, width = 1.0f;
        float sizeStart = 0.5f;      // size at the start of the control block (ramps to size)
        float widthStart = 1.0f;     // width at the start of the control block (ramps to width)
        float pdStart = 0.0f, pdEnd = 0.0f; // predelay (base-rate samples)
    };

    void processChunk (const float* inL, const float* inR, float* outL, float* outR, int numSamples) noexcept;
    void processBase (int numBase) noexcept;
    void handleTypeChange() noexcept;
    void snapSmoothing() noexcept;
    void readTargets() noexcept;

    double processRate = 48000.0, baseRate = 48000.0;
    int rateFactor = 1, maxBlock = 0, maxBase = 0;
    bool prepared = false;

    // Rate conversion: 8th-order Butterworth (4 biquads) down and up, stereo.
    std::array<Biquad, 4> downFilter, upFilter;
    int decimPhase = 0;

    std::vector<float> baseInL, baseInR, baseOutL, baseOutR;

    // Shared input history (predelay + early-reflection taps read from here).
    std::vector<float> histL, histR;
    int histMask = 0, histWp = 0;
    DcBlocker dcBlock;

    std::array<std::unique_ptr<Slot>, 2> slots;
    int current = 0;

    // Parameters (atomics written by setParams, read once per block by the audio thread).
    std::atomic<int>   aType { (int) Type::hall };
    std::atomic<float> aDecay { 2.0f }, aSize { 0.5f }, aPredelay { 10.0f }, aTone { 0.0f },
                       aWidth { 1.0f }, aMod { 0.3f };

    // Targets and smoothed values (audio thread).
    float tLogDecay = 1.0f, tSize = 0.5f, tPredelay = 0.0f, tTone = 0.0f, tWidth = 1.0f, tMod = 0.3f;
    float sLogDecay = 1.0f, sSize = 0.5f, sPredelay = 0.0f, sTone = 0.0f, sWidth = 1.0f, sMod = 0.3f;
    float aSlow = 1.0f, aFast = 1.0f; // per-control-block one-pole coefficients
    bool needsSnap = true;

    // Silence detection (base-rate sample counts).
    int inputQuiet = 0, outputQuiet = 0;
    bool silent = false;
};

} // namespace st::dsp
