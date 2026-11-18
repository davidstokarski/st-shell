#pragma once

namespace st
{

/** Colour + metric tokens for the ST design language.

    Every ST plugin shares the neutral palette below; a plugin customises its
    identity only through the accent colour and the band/series palette.
    Use Theme::get() everywhere instead of hard-coded colours.
*/
struct Theme
{
    // Surfaces (darkest -> lightest). Warm, near-black with a deep red cast.
    juce::Colour bg0        { 0xff0b0708 };   // window backdrop (bottom)
    juce::Colour bg1        { 0xff140c0e };   // main surfaces
    juce::Colour bg2        { 0xff1d1517 };   // raised panels
    juce::Colour bg3        { 0xff2a2226 };   // controls / wells
    juce::Colour bg4        { 0xff3a3136 };   // hover
    juce::Colour stroke     { 0xff3a3034 };   // hairlines
    juce::Colour strokeSoft { 0xff241b1e };
    juce::Colour highlight  { 0x14ffffff };   // top-edge sheen

    // The "display" tint that fills the whole window (top of the gradient)
    juce::Colour displayTop  { 0xff3a1418 };
    juce::Colour displayGlow { 0xff8a2228 };  // selected-band column

    // Hardware-ish greys for knobs, buttons and handles
    juce::Colour knobLight   { 0xffb9b6bb };
    juce::Colour knobDark    { 0xff6c686e };
    juce::Colour buttonTop   { 0xff5d585e };
    juce::Colour buttonBottom{ 0xff433f45 };
    juce::Colour glassPanel  { 0xa6262125 };  // translucent band panel

    // Text
    juce::Colour text       { 0xffeee8e8 };
    juce::Colour textDim    { 0xffb1a7a8 };
    juce::Colour textFaint  { 0xff76696b };

    // Semantics
    juce::Colour accent     { 0xffcf2f2c };   // plugin identity colour (rims, selection)
    juce::Colour accent2    { 0xffff6a55 };   // secondary identity colour
    juce::Colour spectrum   { 0xffd9a58c };   // analyser line (warm)
    juce::Colour modulation { 0xff4fd6e0 };   // default modulation colour
    juce::Colour positive   { 0xff5ee0a5 };
    juce::Colour warning    { 0xffffcc4d };
    juce::Colour danger     { 0xffff4d4d };

    // Series palette — used for modulation sources (and anything needing distinct hues)
    std::array<juce::Colour, 8> series {{
        juce::Colour (0xff4cd07d), juce::Colour (0xff4fd6e0), juce::Colour (0xff3d8ef0), juce::Colour (0xff6b5cf0),
        juce::Colour (0xff9a4ff0), juce::Colour (0xffe04fd0), juce::Colour (0xfff09a3d), juce::Colour (0xffd8d04f) }};

    // Metrics (design-space pixels; editors are scaled as a whole)
    float radiusSmall  = 3.0f;
    float radius       = 5.0f;
    float radiusLarge  = 9.0f;
    float hairline     = 1.0f;

    juce::Colour seriesColour (int i) const noexcept { return series[(size_t) (((i % 8) + 8) % 8)]; }

    static Theme& get() noexcept;
};

/** Embedded Inter typefaces. */
namespace fonts
{
    enum class Weight { regular, medium, semibold, bold };

    juce::Typeface::Ptr typeface (Weight);
    juce::Font make (float height, Weight = Weight::regular, float tracking = 0.0f);

    inline juce::Font regular  (float h) { return make (h, Weight::regular); }
    inline juce::Font medium   (float h) { return make (h, Weight::medium); }
    inline juce::Font semibold (float h) { return make (h, Weight::semibold); }
    inline juce::Font bold     (float h) { return make (h, Weight::bold); }

    /** Small all-caps label style used for control captions. */
    inline juce::Font caption (float h = 10.5f) { return make (h, Weight::semibold, 0.08f); }

    float stringWidth (const juce::Font&, const juce::String&);
}

} // namespace st
