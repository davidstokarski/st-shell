#pragma once

namespace st
{

/** What a knob should show for modulation (all values normalised 0..1). */
struct ModulationDisplay
{
    bool active = false;
    float lo = 0.0f, hi = 0.0f;   // modulation range around the base value
    float current = 0.0f;         // live modulated value
    juce::Colour colour;
};

/** Base for controls bound directly to a host parameter (gestures, undo, host context menu). */
class ParameterControl : public juce::Component, public juce::SettableTooltipClient
{
public:
    ParameterControl (juce::RangedAudioParameter&, juce::UndoManager* = nullptr);

    juce::RangedAudioParameter& getParameter() noexcept { return param; }
    float getNormalised() const noexcept { return normValue; }

    /** Optional: called on right-click before the host/context menu is shown so owners can add items. */
    std::function<void (juce::PopupMenu&)> extendContextMenu;

protected:
    juce::RangedAudioParameter& param;
    juce::UndoManager* undo;
    juce::ParameterAttachment attachment;
    float normValue = 0.0f;
    bool gestureActive = false;

    void beginEdit();
    void setNormalisedDuringEdit (float);
    void endEdit();
    void setNormalisedAsGesture (float);
    void resetToDefault();
    void showContextMenu (std::function<void (juce::PopupMenu&)> addOwnItems);

    virtual void valueChanged() { repaint(); }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParameterControl)
};

//==============================================================================
/** The ST rotary control.

    Drag vertically (shift = fine), double-click or alt-click resets, cmd/ctrl-click
    types a value, mouse wheel nudges, right-click gives the host + ST menu.
*/
class Knob : public ParameterControl, private juce::Timer
{
public:
    enum class Size { large, medium, small, mini };

    Knob (juce::RangedAudioParameter&, juce::UndoManager* = nullptr, const juce::String& label = {});
    ~Knob() override;

    void setLabel (const juce::String& l) { label = l; repaint(); }
    void setColour (juce::Colour c) { colour = c; repaint(); }
    void setBipolar (bool b) { bipolar = b; repaint(); }
    void setSize (Size s) { size = s; repaint(); }
    void setShowValueAlways (bool b) { showValueAlways = b; repaint(); }
    /** Optional override for the displayed value text. */
    std::function<juce::String (float plainValue)> valueToText;

    /** Supply this to draw modulation rings; polled at ~30 Hz while set. */
    void setModulationSource (std::function<ModulationDisplay()> fn);

    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { hovering = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hovering = false; repaint(); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    /** Area of the dial itself in local coordinates. */
    juce::Rectangle<float> getDialBounds() const;

    void showTextEntry();

private:
    juce::String label;
    juce::Colour colour;
    Size size = Size::medium;
    bool bipolar = false, hovering = false, dragging = false, showValueAlways = false;
    juce::Point<float> lastDragPos;
    std::function<ModulationDisplay()> modSource;
    ModulationDisplay modState;
    std::unique_ptr<juce::TextEditor> textEntry;

    void timerCallback() override;
    juce::String getValueText() const;
};

} // namespace st
