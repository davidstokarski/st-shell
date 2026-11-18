#pragma once

namespace st
{

/** Pill toggle bound to a bool (or 2-state) parameter. */
class ToggleChip : public ParameterControl
{
public:
    ToggleChip (juce::RangedAudioParameter&, juce::UndoManager*, const juce::String& text, juce::Colour onColour = {});

    void setText (const juce::String& t) { text = t; repaint(); }
    void setIcon (std::optional<icons::Icon> i) { icon = i; repaint(); }
    void setOnColour (juce::Colour c) { onColour = c; repaint(); }
    /** Inverted chips light up when the parameter is OFF (e.g. "bypass" displayed as "active"). */
    void setInverted (bool b) { inverted = b; repaint(); }
    /** Hardware-style grey button face (Saturn-like header buttons). */
    void setGreyStyle (bool b) { grey = b; repaint(); }
    /** Text only with a small indicator light (footer style). */
    void setFlatStyle (bool b) { flat = b; repaint(); }
    bool isOn() const noexcept { return (normValue >= 0.5f) != inverted; }

    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::String text;
    juce::Colour onColour;
    std::optional<icons::Icon> icon;
    bool inverted = false, grey = false, flat = false;
};

//==============================================================================
/** Horizontal segmented selector for a choice parameter. */
class Segmented : public ParameterControl
{
public:
    Segmented (juce::RangedAudioParameter&, juce::UndoManager*, juce::StringArray labelsOverride = {});

    void setAccent (juce::Colour c) { accent = c; repaint(); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hoverIndex = -1; repaint(); }

private:
    juce::StringArray labels;
    juce::Colour accent;
    int hoverIndex = -1;
    int getNumChoices() const { return labels.size(); }
    int getIndex() const;
    int indexAt (juce::Point<float>) const;
};

//==============================================================================
/** Dropdown for choice parameters. Optionally supply a custom menu builder (e.g. categorised). */
class Dropdown : public ParameterControl
{
public:
    Dropdown (juce::RangedAudioParameter&, juce::UndoManager*);

    /** Return a menu whose item IDs are (choiceIndex + 1). */
    std::function<juce::PopupMenu (int currentIndex)> buildMenu;
    /** Optional caption drawn on the left in small caps. */
    void setCaption (const juce::String& c) { caption = c; repaint(); }
    void setAccent (juce::Colour c) { accent = c; repaint(); }
    void setLarge (bool b) { large = b; repaint(); }
    /** Show previous/next arrows inside the control. */
    void setShowArrows (bool b) { arrows = b; repaint(); }
    void setGreyStyle (bool b) { grey = b; repaint(); }
    /** Text only: "Caption  Value ▾" (footer style). */
    void setFlatStyle (bool b) { flat = b; repaint(); }

    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    juce::String caption;
    juce::Colour accent;
    bool large = false, arrows = false, grey = false, flat = false;
    int getIndex() const;
    int getNumChoices() const;
    void step (int delta);
    juce::Rectangle<float> leftArrow() const, rightArrow() const;
};

//==============================================================================
/** Draggable numeric readout (compact parameter editor). */
class NumberBox : public ParameterControl
{
public:
    NumberBox (juce::RangedAudioParameter&, juce::UndoManager*, const juce::String& caption = {});

    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    void setAccent (juce::Colour c) { accent = c; repaint(); }
    /** Text only: dim caption + value (footer style). */
    void setFlatStyle (bool b) { flat = b; repaint(); }

private:
    juce::String caption;
    juce::Colour accent;
    bool flat = false;
    float lastY = 0.0f;
    bool dragging = false;
};

//==============================================================================
/** Icon (or icon + text) button not bound to a parameter. */
class IconButton : public juce::Button
{
public:
    IconButton (icons::Icon, const juce::String& tooltip = {});

    void setIcon (icons::Icon i) { icon = i; repaint(); }
    void setText (const juce::String& t) { text = t; repaint(); }
    void setOnColour (juce::Colour c) { onColour = c; repaint(); }
    void setDrawBackground (bool b) { drawBackground = b; repaint(); }
    /** Icon only, never a background (header style). */
    void setMinimal (bool b) { minimal = b; repaint(); }

    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    icons::Icon icon;
    juce::String text;
    juce::Colour onColour;
    bool drawBackground = false, minimal = false;
};

//==============================================================================
/** Small text button with ST styling (not bound to a parameter). */
class PillButton : public juce::Button
{
public:
    explicit PillButton (const juce::String& text) : juce::Button (text) {}
    void setOnColour (juce::Colour c) { onColour = c; repaint(); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    juce::Colour onColour;
};

//==============================================================================
/** Vertical fader with a pill cap (Saturn-style tone sliders). Bipolar parameters draw from the centre. */
class Fader : public ParameterControl
{
public:
    Fader (juce::RangedAudioParameter&, juce::UndoManager*, const juce::String& caption = {});

    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override { resetToDefault(); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    juce::Rectangle<float> getCapBounds() const;

private:
    juce::String caption;
    bool dragging = false;
    float lastY = 0.0f;
    juce::Rectangle<float> getTrack() const;
};

//==============================================================================
/** Text-only button (Saturn header / footer style): no box, brightens on hover. */
class FlatButton : public juce::Button
{
public:
    explicit FlatButton (const juce::String& text, const juce::String& tooltip = {});
    void setDropdownArrow (bool b) { arrow = b; repaint(); }
    void setFontHeight (float h) { fontHeight = h; repaint(); }
    void setDimmedText (const juce::String& prefix, const juce::String& suffix = {}) { dimPrefix = prefix; dimSuffix = suffix; repaint(); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    bool arrow = false;
    float fontHeight = 13.5f;
    juce::String dimPrefix, dimSuffix;
};

} // namespace st
