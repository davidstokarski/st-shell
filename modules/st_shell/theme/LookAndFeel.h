#pragma once

namespace st
{

/** Vector icon set (24x24 design box, stroke-style paths). Draw with icons::draw(). */
namespace icons
{
    enum class Icon
    {
        chevronLeft, chevronRight, chevronDown, chevronUp,
        undo, redo, gear, power, copy, plus, close, save, folder,
        menu, dice, link, solo, mute, info, resize, swap, star, trash
    };

    juce::Path path (Icon);   // stroke path in a 24x24 box
    void draw (juce::Graphics&, Icon, juce::Rectangle<float> area, juce::Colour, float strokeWidth = 1.6f);
}

/** Shared drawing helpers so every plugin renders surfaces the same way. */
namespace paint
{
    /** Rounded panel with subtle vertical gradient, top sheen and hairline border. */
    void panel (juce::Graphics&, juce::Rectangle<float>, float radius, juce::Colour base, bool raised = true);
    /** Floating translucent "glass" surface for controls that hover over displays. */
    void glass (juce::Graphics&, juce::Rectangle<float>, float radius, juce::Colour tint = {}, float opacity = 0.84f);
    /** Recessed well (display areas, meters). */
    void well (juce::Graphics&, juce::Rectangle<float>, float radius);
    /** Soft blurred drop shadow approximated by stacked translucent rounded rects (cheap, no image). */
    void softShadow (juce::Graphics&, juce::Rectangle<float>, float radius, float spread = 10.0f, float alpha = 0.45f);
    /** Glow stroke: draws a path several times with decreasing width/alpha. */
    void glowStroke (juce::Graphics&, const juce::Path&, juce::Colour, float width, float glow = 6.0f);
    /** Draw text with ST caption style (uppercase, tracked). */
    void caption (juce::Graphics&, const juce::String&, juce::Rectangle<float>, juce::Colour, juce::Justification = juce::Justification::centred, float height = 10.5f);
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;

    // Popup menus
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                            bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcutKeyText, const juce::Drawable* icon,
                            const juce::Colour* textColour) override;
    void drawPopupMenuSectionHeader (juce::Graphics&, const juce::Rectangle<int>&, const juce::String&) override;
    juce::Font getPopupMenuFont() override;
    int getPopupMenuBorderSize() override { return 6; }
    void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator, int standardMenuItemHeight,
                                    int& idealWidth, int& idealHeight) override;

    // Combo boxes
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown, int buttonX, int buttonY,
                       int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    // Buttons
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool isMouseOverButton, bool isButtonDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;

    // Text editors / labels
    void fillTextEditorBackground (juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int width, int height, juce::TextEditor&) override;
    juce::Font getLabelFont (juce::Label&) override;

    // Tooltips
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;

    // Misc
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int width, int height, bool isScrollbarVertical,
                        int thumbStartPosition, int thumbSize, bool isMouseOver, bool isMouseDown) override;
    void drawCornerResizer (juce::Graphics&, int w, int h, bool isMouseOver, bool isMouseDragging) override;
    void drawAlertBox (juce::Graphics&, juce::AlertWindow&, const juce::Rectangle<int>& textArea, juce::TextLayout&) override;
    juce::Font getAlertWindowTitleFont() override;
    juce::Font getAlertWindowMessageFont() override;
    juce::Font getAlertWindowFont() override;
};

} // namespace st
