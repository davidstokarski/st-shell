#pragma once

namespace st
{

// st::ModulationDisplay (what a knob shows for modulation) lives in mod/ModTypes.h.

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

    Modulation: drop target for modulation sources dragged from st::ModulationPanel
    (wired by st::ModulationBinder through onModulationDropped).
*/
class Knob : public ParameterControl, public juce::DragAndDropTarget, private juce::Timer
{
public:
    enum class Size { large, medium, small, mini };
    /** classic = matte grey disc with white crescent; ringed = adds the coloured rim (hero controls). */
    enum class Look { classic, ringed };

    Knob (juce::RangedAudioParameter&, juce::UndoManager* = nullptr, const juce::String& label = {});
    ~Knob() override;

    void setLabel (const juce::String& l) { label = l; repaint(); }
    void setColour (juce::Colour c) { colour = c; repaint(); }
    void setBipolar (bool b) { bipolar = b; repaint(); }
    void setSize (Size s) { size = s; repaint(); }
    void setLook (Look l) { look = l; repaint(); }
    /** Where modulation connections should attach (local coords, next to the caption). */
    juce::Point<float> getModulationAnchor() const;

    /** Makes the coloured rim (ringed look) an editable second control, e.g. stereo spread or balance.
        Dragging the rim edits `rimParam`; its value is shown as L / R markers around the rim. */
    void setRimParameter (juce::RangedAudioParameter* rimParam, const juce::String& rimLabel, bool showLR = true);

    /** Dragging the outer modulation arc adjusts modulation depth (wired by ModulationBinder). */
    std::function<void()> onModulationDepthBegin;
    std::function<void (float deltaNormalised)> onModulationDepthDrag;
    std::function<void()> onModulationDepthEnd;

    void mouseMove (const juce::MouseEvent&) override;
    void setShowValueAlways (bool b) { showValueAlways = b; repaint(); }
    /** Optional override for the displayed value text. */
    std::function<juce::String (float plainValue)> valueToText;

    /** Supply this to draw modulation rings; polled at ~30 Hz while set. */
    void setModulationSource (std::function<ModulationDisplay()> fn);

    /** When set, the knob accepts modulation drags (description "stmod:<sourceIndex>[:<argb>]")
        and calls this with the source index on drop. */
    std::function<void (int sourceIndex)> onModulationDropped;
    /** Faint ring hinting that the knob is a drop target while a modulation source is dragged. */
    void setModulationDropHint (bool show, juce::Colour = {});
    /** Parses a modulation drag description. Returns false if `desc` isn't one. */
    static bool parseModulationDrag (const juce::var& desc, int& sourceIndex, juce::Colour& colour);
    static juce::String makeModulationDragDescription (int sourceIndex, juce::Colour);

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

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
    Look look = Look::classic;
    float getCaptionHeight() const noexcept;

    struct Geometry { juce::Point<float> centre; float R, modW, outerR, rimW, discR; };
    Geometry geometry() const;
    enum class Zone { value, rim, modDepth };
    Zone zoneAt (juce::Point<float>) const;
    Zone dragZone = Zone::value, hoverZone = Zone::value;

    juce::RangedAudioParameter* rimParam = nullptr;
    std::unique_ptr<juce::ParameterAttachment> rimAttachment;
    float rimNorm = 0.5f;
    juce::String rimLabel;
    bool rimLR = true;
    bool bipolar = false, hovering = false, dragging = false, showValueAlways = false;
    juce::Point<float> lastDragPos;
    std::function<ModulationDisplay()> modSource;
    ModulationDisplay modState;
    bool modDropHover = false, modDropHint = false;
    juce::Colour modDropColour;
    std::unique_ptr<juce::TextEditor> textEntry;

    void timerCallback() override;
    juce::String getValueText() const;
};

} // namespace st
