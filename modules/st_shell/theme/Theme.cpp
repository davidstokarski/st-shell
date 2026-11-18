#if __has_include(<STShellBinary.h>)
 #include <STShellBinary.h>
 #define ST_HAS_FONTS 1
#else
 #define ST_HAS_FONTS 0
#endif

namespace st
{

Theme& Theme::get() noexcept
{
    static Theme theme;
    return theme;
}

namespace fonts
{
    juce::Typeface::Ptr typeface (Weight w)
    {
        // Intentionally leaked: must outlive JUCE's typeface cache at shutdown.
        static auto* faces = new juce::Typeface::Ptr[4];
        auto& slot = faces[(int) w];

        if (slot == nullptr)
        {
           #if ST_HAS_FONTS
            const char* data = nullptr;
            int size = 0;

            switch (w)
            {
                case Weight::regular:  data = STShellBinary::InterRegular_ttf;  size = STShellBinary::InterRegular_ttfSize;  break;
                case Weight::medium:   data = STShellBinary::InterMedium_ttf;   size = STShellBinary::InterMedium_ttfSize;   break;
                case Weight::semibold: data = STShellBinary::InterSemiBold_ttf; size = STShellBinary::InterSemiBold_ttfSize; break;
                case Weight::bold:     data = STShellBinary::InterBold_ttf;     size = STShellBinary::InterBold_ttfSize;     break;
            }

            slot = juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
           #else
            slot = juce::Font (juce::FontOptions{}).getTypefacePtr();
           #endif
        }

        return slot;
    }

    juce::Font make (float height, Weight w, float tracking)
    {
        return juce::Font (juce::FontOptions (typeface (w)).withHeight (height).withKerningFactor (tracking));
    }

    float stringWidth (const juce::Font& f, const juce::String& s)
    {
        return juce::GlyphArrangement::getStringWidth (f, s);
    }
}

} // namespace st
