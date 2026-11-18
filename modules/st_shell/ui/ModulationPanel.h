#pragma once

namespace st
{

namespace modulation_detail
{
    class SourceTab;
    class SourceEditor;
    class RoutingRow;
}

//==============================================================================
/** Compact modulation panel: source tabs (LFOs, envelopes, macros) with live activity and drag
    handles, the selected source's editor, and its routing list.

    Drag a tab (or its handle) onto any st::Knob bound by a ModulationBinder to create a routing.
    Requires a juce::DragAndDropContainer parent (st::EditorBase is one).
    Suggested size: 600-760 x 210-260 (design pixels).
*/
class ModulationPanel : public juce::Component, private juce::ChangeListener, private juce::Timer
{
public:
    explicit ModulationPanel (mod::ModMatrix&);
    ~ModulationPanel() override;

    void selectSource (int flatIndex);
    int getSelectedSource() const noexcept { return selected; }
    /** Called after the selection changed (e.g. to persist it in ProcessorBase::uiState). */
    std::function<void (int flatIndex)> onSourceSelected;
    /** Called with the source index when a drag starts and -1 when it ends (ModulationBinder hooks this). */
    std::function<void (int flatIndex)> onSourceDrag;

    juce::Colour getSourceColour (int flatIndex) const;
    mod::ModMatrix& getMatrix() noexcept { return matrix; }

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int preferredHeight = 232;

    /** Borderless mode blends the panel into the editor backdrop (no card, softer dividers). */
    void setBorderless (bool b) { borderless = b; repaint(); }

private:
    friend class modulation_detail::SourceTab;

    mod::ModMatrix& matrix;
    juce::OwnedArray<modulation_detail::SourceTab> tabs;
    std::unique_ptr<modulation_detail::SourceEditor> editor;
    juce::Viewport viewport;
    juce::Component listContent;
    juce::OwnedArray<modulation_detail::RoutingRow> rows;
    juce::Rectangle<int> tabArea, editorArea, listArea, listHeader;
    int selected = 0;

    void rebuildEditor();
    void refreshRoutings();
    void layoutRows();
    void sourceDragChanged (int flatIndex);

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModulationPanel)
    bool borderless = false;
};

} // namespace st
