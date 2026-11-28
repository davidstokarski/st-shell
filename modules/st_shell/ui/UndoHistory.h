#pragma once

namespace st
{

/** The undo history list (every ST plug-in's header): rest the pointer on Undo or Redo for a moment and the recent
    edits drop down below it, newest first. Steps that were undone (and can be redone) are dimmed; the current state
    has a dot. Click any row to jump to that point (it undoes / redoes the steps in between). Scroll for older ones.
    Closes when the pointer leaves it (and the button it hangs from), on Esc, or on a click elsewhere.

    The rows are the UndoManager's transaction names: st::ParameterControl names every edit after its parameter and
    the new value ("Threshold -6.00 dB"); give other transactions names with beginNewTransaction (name). */
class UndoHistoryList : public juce::Component, private juce::Timer
{
public:
    /** `anchor`: the button it opened from (screen area that keeps it open), in this list's parent's coordinates. */
    UndoHistoryList (juce::UndoManager&, juce::Rectangle<int> anchor);
    ~UndoHistoryList() override;

    static constexpr int rowHeight = 24, panelWidth = 260, maxRows = 12, margin = 14;
    /** Total component size for the current history. */
    juce::Point<int> getIdealSize() const;

    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

    /** Rebuilds the rows from the UndoManager (also done automatically when it changes). */
    void refresh();
    int getNumRows() const noexcept { return (int) rows.size(); }
    /** Tests / snapshots: don't close when the pointer is elsewhere. */
    void setKeepOpen (bool b) noexcept { keepOpen = b; }

private:
    struct Row { juce::String text; int steps = 0; bool undone = false; bool current = false; };
    juce::UndoManager& undo;
    juce::Rectangle<int> anchor;
    std::vector<Row> rows;
    int hover = -1, scroll = 0, outsideTicks = 0;
    int numTransactions = -1, numUndoable = -1;
    bool keepOpen = false;
    Animated fade;

    juce::Rectangle<int> getPanel() const { return getLocalBounds().reduced (margin); }
    int rowAt (juce::Point<int>) const;
    int visibleRows() const { return juce::jmin (maxRows, (int) rows.size()); }
    void timerCallback() override;
    void close();
};

} // namespace st
