#pragma once

namespace st
{

class ModulationStrip;

namespace strip_detail
{
    class SourceColumn;
    class AddTile;
}

//==============================================================================
/** Floating translucent editor shown above a strip tile: the source's controls and its routing list.
    Lives in the strip's top-level content component (design coordinates). Esc, the close button or
    a click outside closes it. Created and owned by ModulationStrip. */
class ModulationPopover : public juce::Component
{
public:
    ModulationPopover (ModulationStrip&, int flatIndex);
    ~ModulationPopover() override;

    int getSourceIndex() const noexcept { return index; }

    /** Places the popover above `tileInParent` (rectangle in the parent's coordinates). */
    void place (juce::Rectangle<float> tileInParent, float connectorTopInParent);

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    bool hitTest (int x, int y) override;

private:
    struct OutsideClickWatcher;

    ModulationStrip& strip;
    const int index;
    juce::Colour colour;
    std::unique_ptr<juce::Component> content;
    IconButton closeButton { icons::Icon::close, "Close" };
    std::unique_ptr<OutsideClickWatcher> watcher;
    juce::Component::SafePointer<juce::Component> watchedRoot;
    Animated appear;
    std::unique_ptr<FrameClock> clock;
    juce::Rectangle<int> targetBounds;

    static constexpr int shadow = 22, pad = 14, header = 28;
    juce::Rectangle<float> getPanelArea() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModulationPopover)
};

//==============================================================================
/** Saturn-style row of modulation-source tiles.

    Each shown source gets a column: a coloured connector dot (drag it onto any st::Knob bound by a
    ModulationBinder to create a routing), a tile with a live mini display (LFO waveform + playhead,
    envelope level trace, or a vertical macro fader you can drag) and its name with a menu.
    A "+" tile adds hidden sources. Clicking a tile opens a ModulationPopover editor above it.

    Which sources are shown is stored in the modulation state (ModMatrix::setSourceShown), so it is
    saved with presets and undoable; sources with routings are always shown.
    Requires a juce::DragAndDropContainer parent (st::EditorBase is one).
    Draw the connections with st::ModulationTrails.
*/
class ModulationStrip : public juce::Component, private juce::ChangeListener
{
public:
    explicit ModulationStrip (mod::ModMatrix&);
    ~ModulationStrip() override;

    /** Design height (connectors + tiles + names). Width: anything; tiles are centred. */
    static constexpr int preferredHeight = 152;

    mod::ModMatrix& getMatrix() noexcept { return matrix; }
    juce::Colour getSourceColour (int flatIndex) const;

    /** Shown sources in display order (flat indices). */
    std::vector<int> getShownSources() const { return matrix.getShownSources(); }
    /** Centre of a shown source's connector dot in strip coordinates (nullopt if not shown). */
    std::optional<juce::Point<float>> getConnectorPosition (int flatIndex) const;
    static constexpr float connectorRadius = 7.5f;

    /** Highlighted source (-1 = none). Set by tile clicks and by drops onto knobs (ModulationBinder). */
    void selectSource (int flatIndex);
    int getSelectedSource() const noexcept { return selected; }

    /** Source currently being dragged from a connector or tile (-1 = none). */
    int getDraggingSource() const noexcept { return dragging; }
    /** Called with the source index when a drag starts and -1 when it ends (ModulationBinder hooks this). */
    std::function<void (int flatIndex)> onSourceDrag;

    /** Shows / hides a source tile (one undo step). Hiding removes the source's routings. */
    void showSource (int flatIndex);
    void removeSource (int flatIndex);

    /** Popover editor (opened by clicking a tile). */
    void showPopover (int flatIndex);
    void closePopover();
    ModulationPopover* getPopover() const noexcept { return popover.get(); }

    /** Menus (public for tests / custom triggers). */
    juce::PopupMenu createAddMenu();
    juce::PopupMenu createSourceMenu (int flatIndex);

    void resized() override;
    void paint (juce::Graphics&) override {}

private:
    friend class strip_detail::SourceColumn;
    friend class strip_detail::AddTile;
    friend class ModulationPopover;

    mod::ModMatrix& matrix;
    juce::OwnedArray<strip_detail::SourceColumn> columns;
    std::unique_ptr<strip_detail::AddTile> addTile;
    std::unique_ptr<ModulationPopover> popover;
    std::unique_ptr<FrameClock> clock;
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);
    int selected = -1, dragging = -1;
    bool firstLayout = true;

    strip_detail::SourceColumn* findColumn (int flatIndex) const;
    void syncColumns();
    void layoutColumns (bool animate);
    void setDragging (int flatIndex);
    void tick (double dt);
    juce::Component* findPopoverParent();

    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModulationStrip)
};

} // namespace st
