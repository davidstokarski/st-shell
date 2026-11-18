#pragma once

namespace st
{

/** What a knob should show for modulation (all values normalised 0..1).
    Produced by st::mod::ModMatrix::getDisplay(), drawn by st::Knob. */
struct ModulationDisplay
{
    bool active = false;
    float lo = 0.0f, hi = 0.0f;   // modulation range around the base value
    float current = 0.0f;         // live modulated value
    juce::Colour colour;          // colour of the first routed source (transparent = theme default)
};

} // namespace st

namespace st::mod
{

//==============================================================================
/** Shape of the modulation system. Pass the same Config to addParameters() and ModMatrix. */
struct Config
{
    int numLfos = 4;
    int numEnvelopes = 2;
    int numMacros = 4;
    int maxRoutings = 32;
    /** Choices for each envelope follower's "input" parameter (e.g. "Input", "Sidechain", "Band 1"...). */
    juce::StringArray envelopeInputs { "Input" };
    /** Prefix of every parameter ID created by addParameters(). */
    juce::String idPrefix = "mod_";
};

//==============================================================================
enum class SourceKind { lfo, envelope, macro };

/** Identifies one modulation source. `index` is 0-based within its kind. */
struct SourceId
{
    SourceKind kind = SourceKind::lfo;
    int index = 0;

    static constexpr SourceId lfo (int i) noexcept       { return { SourceKind::lfo, i }; }
    static constexpr SourceId envelope (int i) noexcept  { return { SourceKind::envelope, i }; }
    static constexpr SourceId macro (int i) noexcept     { return { SourceKind::macro, i }; }

    /** Stable persistence key: "lfo1", "env2", "macro3" (1-based). */
    juce::String toString() const;
    static std::optional<SourceId> fromString (const juce::String&);

    bool operator== (const SourceId&) const = default;
};

//==============================================================================
/** Parameter-ID suffixes (full ID = idPrefix + "lfo1_" + suffix, macros are just idPrefix + "macro1"). */
namespace ids
{
    inline constexpr const char* rate     = "rate";
    inline constexpr const char* sync     = "sync";
    inline constexpr const char* division = "division";
    inline constexpr const char* shape    = "shape";
    inline constexpr const char* phase    = "phase";
    inline constexpr const char* unipolar = "unipolar";
    inline constexpr const char* attack   = "attack";
    inline constexpr const char* release  = "release";
    inline constexpr const char* gain     = "gain";
    inline constexpr const char* input    = "input";
}

//==============================================================================
enum class LfoShape { sine, triangle, sawUp, sawDown, square, randomStep, randomSmooth };
inline constexpr int numLfoShapes = 7;

juce::StringArray getLfoShapeNames();
juce::StringArray getLfoDivisionNames();
int getDefaultLfoDivision() noexcept;   // "1/4"

/** Length of one LFO cycle in quarter notes. `beatsPerBar` (in quarter notes) scales the "bars" divisions. */
double getLfoDivisionQuarterNotes (int divisionIndex, double quarterNotesPerBar = 4.0) noexcept;

inline bool isRandomShape (LfoShape s) noexcept { return s == LfoShape::randomStep || s == LfoShape::randomSmooth; }

/** Bipolar (-1..1) value of a shape at phase [0, 1). Random shapes interpolate randomFrom -> randomTo. */
inline float evaluateLfoShape (LfoShape shape, float p, float randomFrom = 0.0f, float randomTo = 0.0f) noexcept
{
    switch (shape)
    {
        case LfoShape::sine:      return std::sin (p * juce::MathConstants<float>::twoPi);
        case LfoShape::triangle:
        {
            float q = p + 0.75f;
            q -= std::floor (q);
            return 4.0f * std::abs (q - 0.5f) - 1.0f;   // 0 at p=0, +1 at 0.25, -1 at 0.75
        }
        case LfoShape::sawUp:     return 2.0f * p - 1.0f;
        case LfoShape::sawDown:   return 1.0f - 2.0f * p;
        case LfoShape::square:    return p < 0.5f ? 1.0f : -1.0f;
        case LfoShape::randomStep: return randomTo;
        case LfoShape::randomSmooth:
        {
            const float s = p * p * (3.0f - 2.0f * p);   // smoothstep: C1-continuous across cycle boundaries
            return randomFrom + (randomTo - randomFrom) * s;
        }
    }
    return 0.0f;
}

} // namespace st::mod
