#pragma once

namespace st
{

/** Modal-ish text prompt drawn inside the plugin window (avoids native dialogs in hosts). */
class TextPrompt : public juce::Component
{
public:
    TextPrompt (const juce::String& title, const juce::String& initialText, const juce::String& confirmText,
                std::function<void (const juce::String&)> onConfirm);

    /** Shows the prompt covering `parent`. The prompt deletes itself when closed. */
    static void show (juce::Component& parent, const juce::String& title, const juce::String& initialText,
                      const juce::String& confirmText, std::function<void (const juce::String&)> onConfirm);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::String title;
    juce::TextEditor editor;
    PillButton ok, cancel;
    std::function<void (const juce::String&)> confirm;
    juce::Rectangle<int> panel;
    void close (bool accept);
};

//==============================================================================
/** Top bar shared by all ST plugins: brand, presets, undo/redo, A/B, settings. */
class HeaderBar : public juce::Component, private juce::Timer, private juce::ChangeListener
{
public:
    explicit HeaderBar (ProcessorBase&);
    ~HeaderBar() override;

    void setProductName (const juce::String& shortName) { product = shortName; repaint(); }
    /** Lets the plugin add its own items to the settings (gear) menu. */
    std::function<void (juce::PopupMenu&)> extendSettingsMenu;
    /** Called when the user picks a UI scale from the settings menu. */
    std::function<void (float)> onScaleChosen;
    /** Called when the user switches tooltips on / off (settings menu; saved for every ST plug-in). */
    std::function<void (bool)> onTooltipsToggled;
    /** Shows a multiview button (left of the gear) that calls `fn` (see st::MultiView). */
    void setMultiViewCallback (std::function<void()> fn);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    juce::Rectangle<int> getPresetArea() const noexcept { return presetArea; }

    /** Opens the undo history list below the Undo button (normally: rest the pointer on Undo / Redo). */
    void showUndoHistory();
    UndoHistoryList* getUndoHistory() const noexcept { return history.get(); }

private:
    ProcessorBase& proc;
    juce::String product;
    IconButton undoButton { icons::Icon::undo, "Undo" }, redoButton { icons::Icon::redo, "Redo" };
    IconButton prevButton { icons::Icon::chevronLeft, "Previous preset" }, nextButton { icons::Icon::chevronRight, "Next preset" };
    FlatButton abButton { "A", "Switch between compare slots A and B" };
    FlatButton copyButton { "Copy", "Copy the current slot to the other one" };
    FlatButton saveButton { "Save", "Save preset" };
    IconButton settingsButton { icons::Icon::gear, "Settings" };
    IconButton multiViewButton { icons::Icon::multiview, "Multiview: every ST plug-in in the session, by track" };
    juce::Rectangle<int> presetArea, brandArea;
    bool modified = false;
    std::unique_ptr<UndoHistoryList> history;
    int hoverToken = 0;

    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void showPresetMenu();
    void showSettingsMenu();
    void promptSave();
    void refreshAB();
};

//==============================================================================
/** Resizable, uniformly-scaled editor. Design your UI at a fixed size; the shell scales it. */
class EditorBase : public juce::AudioProcessorEditor, public juce::DragAndDropContainer
{
public:
    EditorBase (ProcessorBase&, int designWidth, int designHeight);
    ~EditorBase() override;

    void resized() override;
    void paint (juce::Graphics&) override;
    bool keyPressed (const juce::KeyPress&) override;

    void setScale (float scale);

    /** Lays the content out. Layout is deferred until the subclass is fully constructed:
        call this at the end of your editor's constructor (it also happens automatically
        on the next message-loop tick). */
    void relayout();
    float getScale() const noexcept { return scale; }

    static constexpr int headerHeight = 44;

protected:
    ProcessorBase& processorBase;
    LookAndFeel lnf;

    /** Add plugin UI here (design-space coordinates). */
    juce::Component& getContent() noexcept { return content; }

    /** Lay out your components inside `body` (design-space, below the header). */
    virtual void layout (juce::Rectangle<int> body) = 0;
    /** Override to customise the backdrop. Default: ST gradient + grain. */
    virtual void paintBackground (juce::Graphics&, juce::Rectangle<int> area);
    /** Paint over children (e.g. overlays). */
    virtual void paintOverlay (juce::Graphics&, juce::Rectangle<int>) {}

    HeaderBar header;

private:
    struct Content : juce::Component
    {
        explicit Content (EditorBase& o) : owner (o) { setOpaque (true); }
        void paint (juce::Graphics& g) override { owner.paintBackground (g, getLocalBounds()); }
        void paintOverChildren (juce::Graphics& g) override { owner.paintOverlay (g, getLocalBounds()); }
        void resized() override;
        EditorBase& owner;
    };

    bool layoutReady = false;
    const int designW, designH;
    float scale = 1.0f;
    Content content;
    std::unique_ptr<juce::TooltipWindow> tooltips;
public:
    /** Tooltips on / off for this editor (the settings menu saves the choice for every ST plug-in). */
    void setTooltipsEnabled (bool);
    bool areTooltipsEnabled() const noexcept { return tooltips != nullptr; }
private:
    juce::Image grain, backdrop;
    float backdropScale = 0.0f;
};

} // namespace st
