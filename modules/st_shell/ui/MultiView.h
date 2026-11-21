#pragma once

namespace st
{

/** The multiview: every ST plug-in instance in the session, one row per track, each plug-in on the track as a
    module side by side (Options::leftmostKind first, e.g. the EQ), all editable from here.

    - Modules come from ProcessorBase::createStripView(). Instances of this plug-in embed their component
      directly; instances of other ST plug-ins (other binaries) are shown through a RemoteView: the owner
      renders the module off screen into pixels and gets the mouse events forwarded (shared registry api).
    - Rows are grouped by the host's track name (instances without one get a row each), labelled with the
      track name and colour; the row with this instance is highlighted.
    - The row under the mouse grows a little; header: row-size zoom, search, options (sort, unpin),
      track-list toggle, close (Esc). Pins keep rows at the top. The track list on the right boxes the rows
      in view and scrolls to a row on click. Rows wider than the view scroll sideways (chevrons, Shift-wheel).

    Meant to cover the whole editor (add it to the editor content at full size, delete it in onClose). */
class MultiView : public juce::Component, private juce::Timer, private InstanceRegistry::Listener
{
public:
    struct Options
    {
        juce::String leftmostKind = "ST EQ";
        /** Optional red glow (0..1) for a module of this plug-in (e.g. spectral collision with `own`). */
        std::function<float (ProcessorBase&)> glowFor;
        /** Only tracks with at least one module of this kind are shown (empty = every track). The other ST
            plug-ins on those tracks are still shown next to it (ST DC: only the drum tracks). */
        juce::String requiredKind;
        /** Optional row order: rows are sorted by the lowest key of their modules of this plug-in (rows without
            one go last, in creation order). Pins still come first. E.g. ST DC sorts by drum part. */
        std::function<int (ProcessorBase&)> orderFor;
        /** Title in the multiview header (after the "ST" mark). */
        juce::String title = "Multiview";
    };

    explicit MultiView (ProcessorBase& own);
    MultiView (ProcessorBase& own, Options);
    ~MultiView() override;

    /** The user closed the multiview: delete it (asynchronously). */
    std::function<void()> onClose;

    int getNumRows() const noexcept { return (int) rows.size(); }
    int getNumModules() const noexcept;
    /** Modules of row `row`, left to right: kinds. */
    juce::StringArray getRowKinds (int row) const;
    /** The component showing instance `uid` (its own component, or the remote view proxy). */
    juce::Component* getModuleComponent (uint64_t uid) const;
    /** Forces the magnified row (tests / renders; normally the row under the mouse). */
    void setHoveredInstance (uint64_t uid) { forcedHover = uid; }
    /** Rebuilds the rows now (normally on registry changes). */
    void refresh() { rebuildRows(); }

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    class RemoteView;
    class Module;
    class Row;
    class Sidebar;
    class ZoomSlider;
    class HeaderButton;

private:
    ProcessorBase& own;
    Options options;
    std::vector<std::unique_ptr<Row>> rows;
    std::map<uint64_t, std::unique_ptr<Module>> modules;   // by instance uid (kept across rebuilds)
    juce::Viewport viewport;
    juce::Component holder;
    std::unique_ptr<Sidebar> sidebar;
    std::unique_ptr<ZoomSlider> zoom;
    std::unique_ptr<HeaderButton> menuButton, sidebarButton, closeButton;
    juce::TextEditor search;
    bool showSidebar = true, sortByName = false;
    uint64_t forcedHover = 0;
    juce::StringArray pinned;   // row keys
    int ticks = 0;

    static constexpr int headerHeight = 44, sidebarWidth = 150, nameWidth = 150;
    float baseRowHeight() const;
    void rebuildRows();
    void layoutRows();
    void scrollToRow (int index);
    void togglePin (const juce::String& key);
    void showMenu();
    void timerCallback() override;
    void instancesChanged() override { rebuildRows(); }
    void instanceRemoved (uint64_t uid, ProcessorBase*) override;
    friend class Row;
    friend class Sidebar;
    friend class Module;
};

} // namespace st
