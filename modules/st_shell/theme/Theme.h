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
    // Surfaces (darkest -> lightest)
    juce::Colour bg0        { 0xff08090c };   // window backdrop
    juce::Colour bg1        { 0xff0e1014 };   // main panels
    juce::Colour bg2        { 0xff14171d };   // raised panels
    juce::Colour bg3        { 0xff1b1f27 };   // controls / wells
    juce::Colour bg4        { 0xff242933 };   // hover
    juce::Colour stroke     { 0xff262b35 };   // hairlines
    juce::Colour strokeSoft { 0xff1a1e25 };
    juce::Colour highlight  { 0x14ffffff };   // top-edge sheen

    // Text
    juce::Colour text       { 0xffe9ebf1 };
    juce::Colour textDim    { 0xff9aa1b0 };
    juce::Colour textFaint  { 0xff5b6272 };

    // Semantics
    juce::Colour accent     { 0xffffb547 };   // plugin identity colour
    juce::Colour accent2    { 0xffff7a59 };   // secondary identity colour
    juce::Colour modulation { 0xff4fd1ff };   // modulation rings / sources
    juce::Colour positive   { 0xff5ee0a5 };
    juce::Colour warning    { 0xffffcc4d };
    juce::Colour danger     { 0xffff5a63 };

    // Series / band palette (bands, layers, sources)
    std::array<juce::Colour, 8> series {{
        juce::Colour (0xffffb547), juce::Colour (0xffff7a59), juce::Colour (0xffff5c9a),
        juce::Colour (0xffb57cff), juce::Colour (0xff6f8cff), juce::Colour (0xff3ec6ff),
        juce::Colour (0xff4be3a6), juce::Colour (0xffd7e35b) }};

    // Metrics (design-space pixels; editors are scaled as a whole)
    float radiusSmall  = 4.0f;
    float radius       = 7.0f;
    float radiusLarge  = 12.0f;
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
