#pragma once

#include <array>
#include <complex>

#include "Filters.h"

/*  EqDesign — parametric EQ band design with analog-matched digital filters.

    Every band is described once as an ANALOG prototype (cascade of 1st/2nd-order sections in
    rad/s). From that single description you can get:
      - the exact analog response (for drawing, linear-phase FIR design, natural-phase correction)
      - digital biquads whose magnitude matches the analog response up to Nyquist (no "cramping"
        of bells/shelves near fs/2): poles are impulse-invariant, numerators are fitted to the
        analog magnitude at DC, Nyquist and the section's own frequency (after M. Vicanek,
        "Matched Second Order Digital Filters", 2016). Low sections use the prewarped bilinear
        transform, which is exact there and numerically better conditioned.

    Shapes: bell, low/high shelf (Butterworth-style 6..120 dB/oct with resonance), low/high cut
    (Butterworth 6..120 dB/oct with resonance), notch, band pass, tilt shelf, flat tilt
    (constant dB/oct across the spectrum, pivoting at `freq`) and all pass.

    All functions are allocation-free and real-time safe (coefficient math only).
*/

namespace st::dsp
{

enum class EqShape { bell, lowShelf, lowCut, highShelf, highCut, notch, bandPass, tiltShelf, flatTilt, allPass };

struct EqBandSpec
{
    EqShape shape = EqShape::bell;
    double freq = 1000.0;     // Hz
    double gainDb = 0.0;      // bell / shelves / tilts
    double q = 0.7071;        // bandwidth or resonance (0.7071 = no resonance for cuts/shelves)
    int order = 2;            // slope / 6 dB per octave, 1..20 (cuts, shelves, tilt shelf; notch/band pass/all pass cascade order/2)
};

/** H(s) = (n2 s^2 + n1 s + n0) / (s^2 + d1 s + d0)  (second order)
         = (n1 s + n0) / (s + d0)                    (first order: firstOrder = true, n2 = 0)
    s in rad/s. */
struct AnalogSection
{
    double n2 = 0.0, n1 = 0.0, n0 = 1.0, d1 = 0.0, d0 = 1.0;
    bool firstOrder = false;
    bool allPass = false;   // digital design must keep the all-pass property (non-minimum-phase zeros)

    std::complex<double> response (double omega) const noexcept;
};

namespace eq
{
    inline constexpr int kMaxSections = 12;
    inline constexpr int kMaxOrder = 20;

    struct Analog
    {
        std::array<AnalogSection, kMaxSections> sections {};
        int numSections = 0;
        double gain = 1.0;   // overall scalar

        std::complex<double> response (double hz) const noexcept;
        double magnitude (double hz) const noexcept { return std::abs (response (hz)); }
        double magnitudeDb (double hz) const noexcept;
    };

    struct Digital
    {
        std::array<BiquadCoeffs, kMaxSections> sections {};
        int numSections = 0;

        std::complex<double> response (double hz, double sampleRate) const noexcept;
    };

    /** Analog prototype of a band. */
    Analog designAnalog (const EqBandSpec&) noexcept;

    /** Digital (analog-matched) realisation. The overall gain is folded into the first section. */
    Digital designDigital (const Analog&, double sampleRate) noexcept;
    inline Digital designDigital (const EqBandSpec& s, double sampleRate) noexcept { return designDigital (designAnalog (s), sampleRate); }

    /** Matched digital biquad for one analog section (exposed for tests / custom prototypes). */
    BiquadCoeffs matchSection (const AnalogSection&, double sampleRate) noexcept;

    /** True if the shape uses gainDb. */
    bool hasGain (EqShape) noexcept;
    /** True if the shape uses order (slope). */
    bool hasSlope (EqShape) noexcept;
    /** True if the shape uses q. */
    bool hasQ (EqShape) noexcept;
}

//==============================================================================
/** Cascade of up to eq::kMaxSections biquads (transposed DF-II, double state) for one EQ band,
    with per-sample linear coefficient interpolation when the design changes (zipper free under
    automation and dynamic gain). Up to kMaxFilterChannels channels.  */
class EqCascade
{
public:
    static constexpr int kMaxChannels = kMaxFilterChannels;

    void reset() noexcept;

    /** Sets new coefficients. With `glide`, coefficients move linearly from the current design to
        this one across the next block (process() calls up to the following advance()), otherwise
        they jump. If the number of sections changes the state is reset (no glide possible). */
    void setDesign (const eq::Digital&, bool glide) noexcept;

    /** Processes channel `ch` in place. All channels processed in one block must share numSamples;
        call `advance()` once after all channels of the block have been processed. */
    void process (int ch, float* data, int numSamples) noexcept;
    void advance() noexcept;

    int getNumSections() const noexcept { return target.numSections; }
    const eq::Digital& getDesign() const noexcept { return target; }

private:
    eq::Digital current, target;
    bool ramping = false;
    std::array<std::array<double, 2>, eq::kMaxSections * kMaxChannels> state {};
};

} // namespace st::dsp
