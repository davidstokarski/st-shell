namespace st
{

UndoHistoryList::UndoHistoryList (juce::UndoManager& um, juce::Rectangle<int> a) : undo (um), anchor (a)
{
    setWantsKeyboardFocus (true);
    refresh();
    fade.snap (0.0f);
    fade.setTarget (1.0f);
    startTimerHz (60);
}

UndoHistoryList::~UndoHistoryList() { stopTimer(); }

void UndoHistoryList::refresh()
{
    const auto undos = undo.getUndoDescriptions();   // newest first: [0] = the state we are in now
    const auto redos = undo.getRedoDescriptions();   // next redo first
    numUndoable = undos.size();
    numTransactions = undos.size() + redos.size();

    auto name = [] (const juce::String& s) { return s.isNotEmpty() ? s : juce::String ("Edit"); };
    rows.clear();
    for (int j = redos.size(); j >= 1; --j)
        rows.push_back ({ name (redos[j - 1]), j, true, false });
    for (int i = 0; i < undos.size(); ++i)
        rows.push_back ({ name (undos[i]), -i, false, i == 0 });
    rows.push_back ({ "Start of history", -undos.size(), false, undos.isEmpty() });

    // keep the current state in view
    int current = 0;
    for (int i = 0; i < (int) rows.size(); ++i) if (rows[(size_t) i].current) current = i;
    scroll = juce::jlimit (0, juce::jmax (0, (int) rows.size() - maxRows), juce::jmax (scroll, current - maxRows + 1));
    scroll = juce::jmin (scroll, current);
    repaint();
}

juce::Point<int> UndoHistoryList::getIdealSize() const
{
    return { panelWidth + 2 * margin, visibleRows() * rowHeight + 12 + 2 * margin };
}

int UndoHistoryList::rowAt (juce::Point<int> p) const
{
    auto panel = getPanel().reduced (0, 6);
    if (! panel.contains (p)) return -1;
    const int i = scroll + (p.y - panel.getY()) / rowHeight;
    return i < (int) rows.size() ? i : -1;
}

void UndoHistoryList::paint (juce::Graphics& g)
{
    const auto& t = Theme::get();
    g.setOpacity (1.0f);
    auto panel = getPanel().toFloat();

    paint::softShadow (g, panel, t.radiusLarge, 10.0f, 0.55f * fade.current);
    g.setColour (t.bg2.withAlpha (0.97f * fade.current));
    g.fillRoundedRectangle (panel, t.radiusLarge);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.11f * fade.current), 0.0f, panel.getY(),
                                             juce::Colours::white.withAlpha (0.03f * fade.current), 0.0f, panel.getY() + 60.0f, false));
    g.drawRoundedRectangle (panel.reduced (0.5f), t.radiusLarge, 1.0f);

    auto list = panel.reduced (0.0f, 6.0f);
    g.saveState();
    g.reduceClipRegion (list.toNearestInt());
    for (int v = 0; v < visibleRows(); ++v)
    {
        const int i = scroll + v;
        if (i >= (int) rows.size()) break;
        const auto& row = rows[(size_t) i];
        auto r = juce::Rectangle<float> (list.getX(), list.getY() + (float) (v * rowHeight), list.getWidth(), (float) rowHeight);
        if (i == hover)
        {
            g.setColour (juce::Colours::white.withAlpha (0.07f * fade.current));
            g.fillRoundedRectangle (r.reduced (6.0f, 1.0f), t.radiusSmall);
        }
        auto dot = r.removeFromLeft (26.0f);
        if (row.current)
        {
            g.setColour (t.accent.withAlpha (fade.current));
            g.fillEllipse (dot.withSizeKeepingCentre (6.0f, 6.0f).translated (4.0f, 0.0f));
        }
        const auto colour = row.undone ? t.textFaint : (row.current ? t.text : t.textDim);
        g.setColour (colour.withMultipliedAlpha (fade.current));
        g.setFont (row.current ? fonts::semibold (12.5f) : fonts::medium (12.5f));
        g.drawText (row.text, r.withTrimmedRight (12.0f), juce::Justification::centredLeft, true);
    }
    g.restoreState();

    // fades hint at more rows above / below
    if (scroll > 0)
        paint::fadedLine (g, { list.getX() + 20.0f, list.getY() + 0.5f }, { list.getRight() - 20.0f, list.getY() + 0.5f },
                          juce::Colours::white.withAlpha (0.12f * fade.current), 1.0f, 0.3f);
    if (scroll + maxRows < (int) rows.size())
        paint::fadedLine (g, { list.getX() + 20.0f, list.getBottom() - 0.5f }, { list.getRight() - 20.0f, list.getBottom() - 0.5f },
                          juce::Colours::white.withAlpha (0.12f * fade.current), 1.0f, 0.3f);
}

void UndoHistoryList::mouseMove (const juce::MouseEvent& e)
{
    const int h = rowAt (e.getPosition());
    if (h != hover) { hover = h; repaint(); }
}

void UndoHistoryList::mouseExit (const juce::MouseEvent&)
{
    if (hover != -1) { hover = -1; repaint(); }
}

void UndoHistoryList::mouseDown (const juce::MouseEvent& e)
{
    const int i = rowAt (e.getPosition());
    if (i < 0) { if (! getPanel().contains (e.getPosition())) close(); return; }
    const int steps = rows[(size_t) i].steps;
    for (int k = 0; k < -steps; ++k) undo.undo();
    for (int k = 0; k < steps; ++k) undo.redo();
    refresh();
}

void UndoHistoryList::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const int s = juce::jlimit (0, juce::jmax (0, (int) rows.size() - maxRows), scroll - (w.deltaY > 0 ? 1 : (w.deltaY < 0 ? -1 : 0)));
    if (s != scroll) { scroll = s; repaint(); }
}

bool UndoHistoryList::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey) { close(); return true; }
    return false;
}

void UndoHistoryList::timerCallback()
{
    if (fade.tick (1.0 / 60.0, 18.0f)) repaint();

    // the history changed underneath (Cmd-Z, an edit): follow it
    if (undo.getUndoDescriptions().size() != numUndoable
        || undo.getUndoDescriptions().size() + undo.getRedoDescriptions().size() != numTransactions)
        refresh();

    // close once the pointer has left the list and the button it opened from for a moment
    if (auto* parent = getParentComponent(); parent != nullptr && ! keepOpen)
    {
        const auto mouse = parent->getLocalPoint (nullptr, juce::Desktop::getMousePosition());
        const bool inside = getBounds().reduced (margin - 4).contains (mouse) || anchor.expanded (4).contains (mouse);
        outsideTicks = inside ? 0 : outsideTicks + 1;
        if (outsideTicks > 18) close();   // ~0.3 s
    }
}

void UndoHistoryList::close()
{
    stopTimer();
    if (onClose) onClose();
}

} // namespace st
