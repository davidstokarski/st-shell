namespace st
{

//==============================================================================
/** Small chrome button for the window header (paints its own glyph). */
class MultiView::HeaderButton : public juce::Button
{
public:
    enum class Glyph { dots, sidebar, fullScreen, close, pin, chevronLeft, chevronRight };
    HeaderButton (Glyph g, const juce::String& tip) : juce::Button (tip), glyph (g) { setTooltip (tip); setMouseCursor (juce::MouseCursor::PointingHandCursor); }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        const auto& t = st::Theme::get();
        auto r = getLocalBounds().toFloat().withSizeKeepingCentre (14.0f, 14.0f);
        const auto col = getToggleState() ? t.curve : (over || down ? t.text : t.textDim);
        g.setColour (col);
        switch (glyph)
        {
            case Glyph::dots:
                for (int i = -1; i <= 1; ++i) g.fillEllipse (juce::Rectangle<float> (3.0f, 3.0f).withCentre (r.getCentre().translated (0.0f, 5.0f * (float) i)));
                break;
            case Glyph::sidebar:
                g.drawRoundedRectangle (r.reduced (0.5f, 1.5f), 2.0f, 1.3f);
                g.fillRect (r.reduced (0.5f, 1.5f).removeFromRight (5.0f));
                break;
            case Glyph::fullScreen:
            {
                juce::Path p;
                const float a = 4.5f;
                const auto q = r.reduced (1.0f);
                p.startNewSubPath (q.getX(), q.getY() + a); p.lineTo (q.getX(), q.getY()); p.lineTo (q.getX() + a, q.getY());
                p.startNewSubPath (q.getRight() - a, q.getY()); p.lineTo (q.getRight(), q.getY()); p.lineTo (q.getRight(), q.getY() + a);
                p.startNewSubPath (q.getRight(), q.getBottom() - a); p.lineTo (q.getRight(), q.getBottom()); p.lineTo (q.getRight() - a, q.getBottom());
                p.startNewSubPath (q.getX() + a, q.getBottom()); p.lineTo (q.getX(), q.getBottom()); p.lineTo (q.getX(), q.getBottom() - a);
                g.strokePath (p, juce::PathStrokeType (1.4f));
                break;
            }
            case Glyph::close:
                st::icons::draw (g, st::icons::Icon::close, r.expanded (2.0f), col, 1.5f);
                break;
            case Glyph::chevronLeft:
            case Glyph::chevronRight:
                g.setColour (Theme::get().bg0.withAlpha (0.8f));
                g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 4.0f);
                st::icons::draw (g, glyph == Glyph::chevronLeft ? st::icons::Icon::chevronLeft : st::icons::Icon::chevronRight, r.expanded (2.0f), col, 1.6f);
                break;
            case Glyph::pin:
            {
                // a push pin, tilted
                juce::Path p;
                p.addRoundedRectangle (-2.5f, -6.0f, 5.0f, 6.0f, 1.0f);
                p.startNewSubPath (-4.5f, 0.0f); p.lineTo (4.5f, 0.0f);
                p.startNewSubPath (0.0f, 0.0f); p.lineTo (0.0f, 6.0f);
                p.applyTransform (juce::AffineTransform::rotation (0.6f).translated (r.getCentre()));
                g.strokePath (p, juce::PathStrokeType (1.2f));
                break;
            }
        }
    }

private:
    Glyph glyph;
};

//==============================================================================
/** Header zoom: row size. */
class MultiView::ZoomSlider : public juce::Component, public juce::SettableTooltipClient
{
public:
    float value = 0.35f;
    std::function<void()> onChange;
    ZoomSlider() { setTooltip ("Row size"); setMouseCursor (juce::MouseCursor::LeftRightResizeCursor); }

    void paint (juce::Graphics& g) override
    {
        const auto& t = st::Theme::get();
        auto r = getLocalBounds().toFloat().reduced (6.0f, 0.0f);
        const float y = r.getCentreY();
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.fillRoundedRectangle (r.withHeight (3.0f).withCentre ({ r.getCentreX(), y }), 1.5f);
        const float x = r.getX() + r.getWidth() * value;
        g.setColour (t.textDim);
        g.fillRoundedRectangle (juce::Rectangle<float> (r.getX(), y - 1.5f, x - r.getX(), 3.0f), 1.5f);
        g.setColour (t.text);
        g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ x, y }));
    }
    void mouseDown (const juce::MouseEvent& e) override { mouseDrag (e); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const auto r = getLocalBounds().toFloat().reduced (6.0f, 0.0f);
        value = juce::jlimit (0.0f, 1.0f, (e.position.x - r.getX()) / r.getWidth());
        repaint();
        if (onChange) onChange();
    }
};


//==============================================================================
/** A module of another ST plug-in (another binary): rendered by its owner into pixels, mouse forwarded. */
class MultiView::RemoteView : public juce::Component, private juce::Timer
{
public:
    explicit RemoteView (const InstanceRegistry::Info& info) : uid (info.uid), api (info.api)
    {
        if (api != nullptr && api->createView != nullptr) view = api->createView (info.ctx);
        startTimerHz (30);
    }
    ~RemoteView() override
    {
        stopTimer();
        if (usable()) api->destroyView (view);
    }
    bool isValid() const noexcept { return view != 0; }
    void kill() noexcept { dead = true; stopTimer(); }
    int preferredWidth (int h) const { return usable() ? api->preferredWidth (view, h) : h * 2; }

    void resized() override
    {
        if (usable()) api->setViewSize (view, getWidth(), getHeight());
        renderNow();
    }
    void paint (juce::Graphics& g) override
    {
        if (image.isValid()) g.drawImage (image, getLocalBounds().toFloat());
    }

    void mouseMove (const juce::MouseEvent& e) override        { send (shared::mouseMove, e); }
    void mouseEnter (const juce::MouseEvent& e) override       { send (shared::mouseMove, e); }
    void mouseExit (const juce::MouseEvent& e) override        { send (shared::mouseExit, e); }
    void mouseDown (const juce::MouseEvent& e) override        { send (shared::mouseDown, e); }
    void mouseDrag (const juce::MouseEvent& e) override        { send (shared::mouseDrag, e); }
    void mouseUp (const juce::MouseEvent& e) override          { send (shared::mouseUp, e); }
    void mouseDoubleClick (const juce::MouseEvent& e) override { send (shared::mouseDoubleClick, e); }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        // the module didn't use it (no band under the mouse...): scroll the list
        if (send (shared::mouseWheel, e, w.deltaX, w.deltaY * (w.isReversed ? -1.0f : 1.0f)) == 0)
            juce::Component::mouseWheelMove (e, w);
    }

private:
    uint64_t uid;
    const shared::InstanceApi* api;
    uint64_t view = 0;
    bool dead = false;
    juce::Image image;

    bool usable() const { return ! dead && view != 0 && api != nullptr && InstanceRegistry::get().isAlive (uid); }

    static uint32_t modsOf (const juce::ModifierKeys& m)
    {
        uint32_t r = 0;
        if (m.isShiftDown())   r |= shared::modShift;
        if (m.isCtrlDown())    r |= shared::modCtrl;
        if (m.isAltDown())     r |= shared::modAlt;
        if (m.isCommandDown()) r |= shared::modCmd;
        if (m.isLeftButtonDown())  r |= shared::modLeft;
        if (m.isRightButtonDown()) r |= shared::modRight;
        return r;
    }

    int send (int type, const juce::MouseEvent& e, float wx = 0.0f, float wy = 0.0f)
    {
        if (! usable()) return 0;
        const int r = api->mouse (view, type, e.position.x, e.position.y, modsOf (e.mods), wx, wy, e.getNumberOfClicks());
        renderNow();
        return r;
    }

    float renderScale() const
    {
        float s = juce::Component::getApproximateScaleFactorForComponent (const_cast<RemoteView*> (this));
        if (auto* peer = getPeer()) s *= (float) peer->getPlatformScaleFactor();
        else s *= 2.0f;
        return juce::jlimit (1.0f, 4.0f, s);
    }

    bool inView() const
    {
        if (! isVisible()) return false;
        if (auto* vp = findParentComponentOfClass<juce::Viewport>())
            if (auto* viewed = vp->getViewedComponent())
                return viewed->getLocalArea (this, getLocalBounds()).intersects (vp->getViewArea());
        return true;
    }

    void renderNow()
    {
        if (! usable() || getWidth() <= 0 || getHeight() <= 0) return;
        const float scale = renderScale();
        const int pw = juce::roundToInt ((float) getWidth() * scale), ph = juce::roundToInt ((float) getHeight() * scale);
        if (! image.isValid() || image.getWidth() != pw || image.getHeight() != ph)
            image = juce::Image (juce::Image::ARGB, pw, ph, true, juce::SoftwareImageType());
        {
            juce::Image::BitmapData bd (image, juce::Image::BitmapData::writeOnly);
            api->render (view, reinterpret_cast<uint32_t*> (bd.data), pw, ph, bd.lineStride, scale);
        }
        repaint();
    }

    void timerCallback() override { if (inView()) renderNow(); }
};

//==============================================================================
/** One plug-in instance in a row: a frame with its kind written vertically, the module inside. */
class MultiView::Module : public juce::Component
{
public:
    Module (MultiView& o, const InstanceRegistry::Info& info) : uid (info.uid), kind (info.kind), number (info.number), local (info.local), owner (o)
    {
        if (local != nullptr) content = local->createStripView();
        else
        {
            auto rv = std::make_unique<RemoteView> (info);
            if (rv->isValid()) { remote = rv.get(); content = std::move (rv); }
        }
        if (content != nullptr) addAndMakeVisible (*content);
    }

    static constexpr int labelStrip = 20;
    const MultiView& getOwner() const noexcept { return owner; }
    bool hasContent() const noexcept { return content != nullptr; }
    juce::Component* getContent() const noexcept { return content.get(); }
    void kill() { if (remote != nullptr) remote->kill(); }

    int preferredWidth (int h) const
    {
        const int inner = h - 2;
        const int w = local != nullptr ? local->getStripWidth (inner) : (remote != nullptr ? remote->preferredWidth (inner) : inner * 2);
        return w + labelStrip + 2;
    }

    void resized() override
    {
        if (content != nullptr) content->setBounds (getLocalBounds().withTrimmedLeft (labelStrip).reduced (1));
    }

    void paint (juce::Graphics& g) override
    {
        const auto& t = Theme::get();
        const auto f = getLocalBounds().toFloat();
        g.setColour (t.bg1);
        g.fillRoundedRectangle (f, 6.0f);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillRect (f.getX() + (float) labelStrip - 1.0f, f.getY() + 6.0f, 1.0f, f.getHeight() - 12.0f);
        g.saveState();
        const auto c = juce::Point<float> (f.getX() + (float) labelStrip * 0.5f, f.getBottom() - 34.0f);
        g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, c.x, c.y));
        g.setFont (fonts::semibold (8.5f));
        g.setColour (t.textFaint);
        g.drawText (kind.toUpperCase(), juce::Rectangle<float> (60.0f, 12.0f).withCentre (c), juce::Justification::centred, false);
        g.restoreState();
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        const auto& t = Theme::get();
        const auto f = getLocalBounds().toFloat();
        const float glow = local != nullptr && owner.options.glowFor ? owner.options.glowFor (*local) : 0.0f;
        g.setColour (glow > 0.02f ? t.danger.withAlpha (0.35f + 0.5f * glow) : juce::Colours::white.withAlpha (0.08f));
        g.drawRoundedRectangle (f.reduced (0.5f), 6.0f, glow > 0.02f ? 1.5f : 1.0f);
    }

    uint64_t uid;
    juce::String kind;
    int number;
    ProcessorBase* local;

private:
    MultiView& owner;
    std::unique_ptr<juce::Component> content;
    RemoteView* remote = nullptr;
};

//==============================================================================
/** A track: name chip (+ pin) on the left, its plug-ins as modules side by side (scrolls sideways if needed). */
class MultiView::Row : public juce::Component
{
public:
    Row (MultiView& o, const juce::String& k) : key (k), owner (o),
        pin (HeaderButton::Glyph::pin, "Pin this track to the top"),
        left (HeaderButton::Glyph::chevronLeft, "Scroll left"), right (HeaderButton::Glyph::chevronRight, "Scroll right")
    {
        pin.onClick = [this] { owner.togglePin (key); };
        left.onClick = [this] { scrollBy (-1); };
        right.onClick = [this] { scrollBy (1); };
        addAndMakeVisible (strip);
        strip.setInterceptsMouseClicks (false, true);
        for (auto* b : { &pin, &left, &right }) addAndMakeVisible (*b);
        height.snap (owner.baseRowHeight());
    }

    juce::String key, label;
    juce::Colour colour;
    bool isOwn = false;
    std::vector<Module*> mods;
    Animated height;

    void setModules (std::vector<Module*> m)
    {
        for (auto* old : mods) if (std::find (m.begin(), m.end(), old) == m.end()) strip.removeChildComponent (old);
        mods = std::move (m);
        for (auto* x : mods) strip.addAndMakeVisible (*x);
        resized();
    }

    void resized() override
    {
        const int h = getHeight();
        strip.setBounds (nameWidth, 0, juce::jmax (0, getWidth() - nameWidth - 6), h);
        int x = 0;
        for (auto* m : mods)
        {
            const int w = m->preferredWidth (h);
            m->setBounds (x - juce::roundToInt (scrollX), 0, w, h);
            x += w + 12;
        }
        contentWidth = juce::jmax (0, x - 12);
        const float maxScroll = (float) juce::jmax (0, contentWidth - strip.getWidth());
        if (scrollX > maxScroll) { scrollX = maxScroll; for (auto* m : mods) m->setTopLeftPosition (m->getX(), 0); }
        const bool overflow = maxScroll > 0.5f;
        left.setVisible (overflow && scrollX > 0.5f);
        right.setVisible (overflow && scrollX < maxScroll - 0.5f);
        right.setBounds (juce::Rectangle<int> (24, 32).withCentre ({ getWidth() - 18, h / 2 }));
        left.setBounds (juce::Rectangle<int> (24, 32).withCentre ({ nameWidth + 14, h / 2 }));
        pin.setBounds (juce::Rectangle<int> (22, 22).withPosition (nameWidth - 34, h / 2 - 30));
        left.toFront (false);
        right.toFront (false);
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const bool sideways = e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY);
        if (sideways && contentWidth > strip.getWidth())
        {
            const float d = std::abs (w.deltaX) > std::abs (w.deltaY) ? w.deltaX : w.deltaY;
            scrollX = juce::jlimit (0.0f, (float) (contentWidth - strip.getWidth()), scrollX - d * 300.0f);
            resized();
            return;
        }
        juce::Component::mouseWheelMove (e, w);
    }

    void paint (juce::Graphics& g) override
    {
        const auto& t = Theme::get();
        const auto font = fonts::semibold (12.0f);
        const float tw = fonts::stringWidth (font, label) + 30.0f;
        const float cw = juce::jmin (tw, (float) nameWidth - 16.0f);
        const auto chip = juce::Rectangle<float> (cw, 20.0f).withPosition ((float) nameWidth - 12.0f - cw, (float) getHeight() * 0.5f - 2.0f);
        g.setColour (isOwn ? t.bg4 : t.bg3);
        g.fillRoundedRectangle (chip, 10.0f);
        g.setFont (font);
        g.setColour (isOwn ? t.text : t.textDim);
        g.drawFittedText (label, chip.reduced (10.0f, 0.0f).withTrimmedRight (10.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
        g.setColour (colour);
        g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ chip.getRight() - 11.0f, chip.getCentreY() }));
        if (owner.pinned.contains (key))
        {
            g.setColour (t.curve.withAlpha (0.8f));
            g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre ({ (float) nameWidth - 23.0f, (float) getHeight() * 0.5f - 34.0f }));
        }
    }

private:
    MultiView& owner;
    juce::Component strip;
    HeaderButton pin, left, right;
    float scrollX = 0.0f;
    int contentWidth = 0;

    void scrollBy (int dir)
    {
        scrollX = juce::jlimit (0.0f, (float) juce::jmax (0, contentWidth - strip.getWidth()), scrollX + (float) dir * (float) strip.getWidth() * 0.6f);
        resized();
    }
};

//==============================================================================
/** Track list on the right: every row with its colour; rows in view are boxed; click scrolls to one. */
class MultiView::Sidebar : public juce::Component
{
public:
    explicit Sidebar (MultiView& w) : owner (w) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    static constexpr float lineH = 18.0f, top = 10.0f;

    void paint (juce::Graphics& g) override
    {
        const auto& t = Theme::get();
        const auto view = owner.viewport.getViewArea();
        int first = -1, last = -1;
        for (int i = 0; i < (int) owner.rows.size(); ++i)
        {
            auto& r = *owner.rows[(size_t) i];
            if (r.isVisible() && r.getBounds().intersects (view)) { if (first < 0) first = i; last = i; }
        }
        if (first >= 0)
        {
            auto box = juce::Rectangle<float> (4.0f, top + lineH * (float) first - 2.0f, (float) getWidth() - 10.0f, lineH * (float) (last - first + 1) + 4.0f);
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.fillRoundedRectangle (box, 3.0f);
            g.setColour (juce::Colours::white.withAlpha (0.18f));
            g.drawRoundedRectangle (box.reduced (0.5f), 3.0f, 1.0f);
        }
        for (int i = 0; i < (int) owner.rows.size(); ++i)
        {
            auto& r = *owner.rows[(size_t) i];
            const auto line = juce::Rectangle<float> (8.0f, top + lineH * (float) i, (float) getWidth() - 16.0f, lineH);
            g.setColour (r.colour.withAlpha (r.isVisible() ? 1.0f : 0.35f));
            g.fillRect (line.withWidth (2.5f).reduced (0.0f, 3.0f));
            const bool inView = i >= first && i <= last;
            g.setFont (r.isOwn ? fonts::semibold (11.5f) : fonts::regular (11.5f));
            g.setColour (! r.isVisible() ? t.textFaint.withAlpha (0.5f) : (inView || i == hover ? t.text : t.textDim));
            g.drawText (r.label, line.withTrimmedLeft (8.0f), juce::Justification::centredLeft, true);
        }
    }
    void mouseMove (const juce::MouseEvent& e) override { const int h = indexAt (e.position.y); if (h != hover) { hover = h; repaint(); } }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown (const juce::MouseEvent& e) override { if (const int i = indexAt (e.position.y); i >= 0) owner.scrollToRow (i); }

private:
    MultiView& owner;
    int hover = -1;
    int indexAt (float y) const { const int i = (int) std::floor ((y - top) / lineH); return juce::isPositiveAndBelow (i, (int) owner.rows.size()) ? i : -1; }
};

//==============================================================================
MultiView::MultiView (ProcessorBase& o) : MultiView (o, Options {}) {}

MultiView::MultiView (ProcessorBase& o, Options opts) : own (o), options (std::move (opts))
{
    setOpaque (true);
    setWantsKeyboardFocus (true);

    viewport.setViewedComponent (&holder, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (7);
    addAndMakeVisible (viewport);

    sidebar = std::make_unique<Sidebar> (*this);
    addAndMakeVisible (*sidebar);

    zoom = std::make_unique<ZoomSlider>();
    zoom->onChange = [this] { for (auto& r : rows) r->height.snap (baseRowHeight()); layoutRows(); };
    addAndMakeVisible (*zoom);

    const auto& t = Theme::get();
    search.setTextToShowWhenEmpty ("Search", t.textFaint);
    search.setJustification (juce::Justification::centred);
    search.setFont (fonts::regular (13.0f));
    search.setColour (juce::TextEditor::backgroundColourId, t.bg2);
    search.setColour (juce::TextEditor::textColourId, t.text);
    search.setColour (juce::TextEditor::outlineColourId, juce::Colours::white.withAlpha (0.08f));
    search.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::white.withAlpha (0.2f));
    search.onTextChange = [this] { layoutRows(); };
    search.onEscapeKey = [this] { search.clear(); layoutRows(); grabKeyboardFocus(); };
    addAndMakeVisible (search);

    menuButton = std::make_unique<HeaderButton> (HeaderButton::Glyph::dots, "Options");
    sidebarButton = std::make_unique<HeaderButton> (HeaderButton::Glyph::sidebar, "Show / hide the track list");
    closeButton = std::make_unique<HeaderButton> (HeaderButton::Glyph::close, "Back (Esc)");
    menuButton->onClick = [this] { showMenu(); };
    sidebarButton->onClick = [this] { showSidebar = ! showSidebar; resized(); };
    closeButton->onClick = [this] { if (onClose) onClose(); };
    for (auto* b : { menuButton.get(), sidebarButton.get(), closeButton.get() }) addAndMakeVisible (*b);

    InstanceRegistry::get().addListener (this);
    rebuildRows();
    setSize (1040, 620);
    startTimerHz (60);
}

MultiView::~MultiView()
{
    InstanceRegistry::get().removeListener (this);
    stopTimer();
    rows.clear();
    modules.clear();
}

int MultiView::getNumModules() const noexcept { return (int) modules.size(); }

juce::StringArray MultiView::getRowKinds (int row) const
{
    juce::StringArray k;
    if (juce::isPositiveAndBelow (row, (int) rows.size()))
        for (auto* m : rows[(size_t) row]->mods) k.add (m->kind);
    return k;
}

bool MultiView::isRowPinned (int row) const
{
    return juce::isPositiveAndBelow (row, (int) rows.size()) && pinned.contains (rows[(size_t) row]->key);
}

void MultiView::setRowPinned (int row, bool shouldBePinned)
{
    if (juce::isPositiveAndBelow (row, (int) rows.size()) && isRowPinned (row) != shouldBePinned)
        togglePin (rows[(size_t) row]->key);
}

juce::Component* MultiView::getModuleComponent (uint64_t uid) const
{
    auto it = modules.find (uid);
    return it == modules.end() ? nullptr : it->second->getContent();
}

float MultiView::baseRowHeight() const
{
    return 70.0f + 170.0f * (zoom != nullptr ? zoom->value : 0.35f);
}

void MultiView::instanceRemoved (uint64_t uid, ProcessorBase*)
{
    // never call into a removed instance again: kill its module before anything else
    if (auto it = modules.find (uid); it != modules.end())
    {
        it->second->kill();
        for (auto& r : rows)
        {
            auto m = r->mods;
            m.erase (std::remove (m.begin(), m.end(), it->second.get()), m.end());
            r->setModules (m);
        }
        modules.erase (it);
    }
    rebuildRows();
}

void MultiView::rebuildRows()
{
    auto& registry = InstanceRegistry::get();
    const auto infos = registry.getAllInstances();
    const uint64_t ownUid = own.getInstanceUid();

    auto rowKey = [] (const InstanceRegistry::Info& info)
    {
        return info.track.isNotEmpty() ? "t:" + info.track : "u:" + juce::String ((juce::int64) info.uid);
    };

    // Pins are shared by every ST plug-in. Drop the ones whose track has no ST plug-in left at all (checked
    // against every instance, not just the rows shown here: with requiredKind a pinned track without that
    // kind is hidden here but stays pinned everywhere else).
    {
        std::set<juce::String> allRows;
        for (auto& info : infos) allRows.insert (rowKey (info));
        juce::StringArray stale;
        for (auto& k : registry.getPinnedRows())
            if (allRows.count (k) == 0) stale.add (k);
        if (! stale.isEmpty() && ! infos.empty()) registry.unpinRows (stale);
        pinned = registry.getPinnedRows();
    }
    // Options::requiredKind: only tracks that have a module of that kind
    std::set<juce::String> wantedRows;
    for (auto& info : infos)
        if (options.requiredKind.isEmpty() || info.kind == options.requiredKind) wantedRows.insert (rowKey (info));

    // modules: keep the ones we have, create new ones, drop the gone ones (and the ones on unwanted tracks)
    std::set<uint64_t> alive;
    for (auto& info : infos)
    {
        if (wantedRows.count (rowKey (info)) == 0) continue;
        alive.insert (info.uid);
        if (modules.count (info.uid) == 0)
        {
            auto m = std::make_unique<Module> (*this, info);
            if (m->hasContent()) modules[info.uid] = std::move (m);
        }
    }
    for (auto it = modules.begin(); it != modules.end();)
    {
        if (alive.count (it->first) == 0)
        {
            it->second->kill();
            for (auto& r : rows)
            {
                auto m = r->mods;
                m.erase (std::remove (m.begin(), m.end(), it->second.get()), m.end());
                r->setModules (m);
            }
            it = modules.erase (it);
        }
        else ++it;
    }

    // group by track (no track name: a row each), keep row objects (their size animation) by key
    struct Group { juce::String key, label; juce::Colour colour; std::vector<Module*> mods; uint64_t firstUid = ~0ull; bool own = false;
                   int order = std::numeric_limits<int>::max(); };
    std::vector<Group> groups;
    for (auto& info : infos)
    {
        auto mit = modules.find (info.uid);
        if (mit == modules.end()) continue;
        const auto key = rowKey (info);
        auto g = std::find_if (groups.begin(), groups.end(), [&] (const Group& x) { return x.key == key; });
        if (g == groups.end())
        {
            groups.push_back ({ key, info.label, info.trackColour, {}, info.uid, false });
            g = groups.end() - 1;
        }
        g->mods.push_back (mit->second.get());
        g->firstUid = std::min (g->firstUid, info.uid);
        g->own |= info.uid == ownUid;
        if (options.orderFor && info.local != nullptr) g->order = std::min (g->order, options.orderFor (*info.local));
        if (g->colour.isTransparent() && ! info.trackColour.isTransparent()) g->colour = info.trackColour;
    }
    const auto& t = Theme::get();
    for (size_t i = 0; i < groups.size(); ++i)
    {
        auto& g = groups[i];
        if (g.colour.isTransparent()) g.colour = t.seriesColour ((int) (g.firstUid % 8));
        else g.colour = g.colour.withAlpha (1.0f);
        // the leftmost kind first (e.g. the EQ), then the others in creation order
        std::stable_sort (g.mods.begin(), g.mods.end(), [this] (Module* a, Module* b)
        {
            const bool la = a->kind == options.leftmostKind, lb = b->kind == options.leftmostKind;
            if (la != lb) return la;
            return a->uid < b->uid;
        });
    }
    std::stable_sort (groups.begin(), groups.end(), [this] (const Group& a, const Group& b)
    {
        const bool pa = pinned.contains (a.key), pb = pinned.contains (b.key);
        if (pa != pb) return pa;
        if (sortByName) return a.label.compareNatural (b.label) < 0;
        if (a.order != b.order) return a.order < b.order;
        return a.firstUid < b.firstUid;
    });

    std::vector<std::unique_ptr<Row>> next;
    for (auto& g : groups)
    {
        std::unique_ptr<Row> row;
        for (auto& r : rows)
            if (r != nullptr && r->key == g.key) { row = std::move (r); break; }
        if (row == nullptr) row = std::make_unique<Row> (*this, g.key);
        row->label = g.label;
        row->colour = g.colour;
        row->isOwn = g.own;
        row->setModules (g.mods);
        next.push_back (std::move (row));
    }
    for (auto& r : rows) if (r != nullptr) r->setModules ({});   // rows that went away let go of their modules
    rows = std::move (next);
    for (auto& r : rows) holder.addAndMakeVisible (*r);
    layoutRows();
    repaint();
}

void MultiView::layoutRows()
{
    const auto filter = search.getText().trim();
    const int w = viewport.getMaximumVisibleWidth();
    int y = 12;
    for (auto& r : rows)
    {
        bool show = filter.isEmpty() || r->label.containsIgnoreCase (filter);
        for (auto* m : r->mods) show |= filter.isNotEmpty() && m->kind.containsIgnoreCase (filter);
        r->setVisible (show);
        if (! show) continue;
        const int h = juce::roundToInt (r->height.current);
        r->setBounds (0, y, w, h);
        y += h + 14;
    }
    holder.setSize (w, y + 12);
    if (sidebar != nullptr) sidebar->repaint();
}

void MultiView::scrollToRow (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) rows.size())) return;
    auto& r = *rows[(size_t) index];
    if (r.isVisible()) viewport.setViewPosition (0, juce::jmax (0, r.getY() - 12));
    sidebar->repaint();
}

void MultiView::togglePin (const juce::String& key)
{
    InstanceRegistry::get().setRowPinned (key, ! pinned.contains (key));   // every ST plug-in's multiview follows
    rebuildRows();
}

void MultiView::showMenu()
{
    juce::PopupMenu m;
    m.addItem (juce::PopupMenu::Item ("Sort by Track Name").setTicked (sortByName).setAction ([this] { sortByName = ! sortByName; rebuildRows(); }));
    m.addItem (juce::PopupMenu::Item ("Show Track List").setTicked (showSidebar).setAction ([this] { showSidebar = ! showSidebar; resized(); }));
    m.addSeparator();
    // Unpin All: the rows shown here (pinned tracks this multiview filters out stay pinned elsewhere)
    juce::StringArray shownPins;
    for (auto& r : rows) if (pinned.contains (r->key)) shownPins.add (r->key);
    m.addItem (juce::PopupMenu::Item ("Unpin All").setEnabled (! shownPins.isEmpty()).setAction ([this, shownPins]
    {
        InstanceRegistry::get().unpinRows (shownPins);
        rebuildRows();
    }));
    st::showMenu (m, menuButton.get());
}

void MultiView::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (headerHeight).reduced (14, 8);
    closeButton->setBounds (header.removeFromRight (26));
    header.removeFromRight (6);
    sidebarButton->setBounds (header.removeFromRight (26));
    sidebarButton->setToggleState (showSidebar, juce::dontSendNotification);

    const int cx = getWidth() / 2;
    search.setBounds (juce::Rectangle<int> (200, 26).withCentre ({ cx + 60, header.getCentreY() }));
    zoom->setBounds (juce::Rectangle<int> (90, 20).withCentre ({ search.getX() - 70, header.getCentreY() }));
    menuButton->setBounds (juce::Rectangle<int> (26, 26).withCentre ({ search.getRight() + 26, header.getCentreY() }));

    sidebar->setVisible (showSidebar);
    if (showSidebar) sidebar->setBounds (r.removeFromRight (sidebarWidth));
    viewport.setBounds (r.reduced (8, 0).withTrimmedBottom (8));
    layoutRows();
}

void MultiView::paint (juce::Graphics& g)
{
    const auto& t = Theme::get();
    g.fillAll (t.bg0);
    auto header = getLocalBounds().removeFromTop (headerHeight).toFloat();
    g.setColour (t.bg1);
    g.fillRect (header);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.fillRect (header.removeFromBottom (1.0f));

    g.setFont (fonts::bold (14.0f));
    g.setColour (t.text);
    g.drawText ("ST", juce::Rectangle<float> (18.0f, 0.0f, 30.0f, (float) headerHeight), juce::Justification::centredLeft, false);
    g.setFont (fonts::medium (14.0f));
    g.setColour (t.accent);
    const float titleW = std::ceil (fonts::stringWidth (fonts::medium (14.0f), options.title)) + 12.0f;
    g.drawText (options.title, juce::Rectangle<float> (42.0f, 0.0f, titleW, (float) headerHeight), juce::Justification::centredLeft, false);
    g.setFont (fonts::regular (13.0f));
    g.setColour (t.textDim);
    g.drawText (juce::String (rows.size()) + (rows.size() == 1 ? " track" : " tracks") + ", " + juce::String (modules.size()) + " plug-ins",
                juce::Rectangle<float> (42.0f + titleW, 0.0f, 200.0f, (float) headerHeight), juce::Justification::centredLeft, false);

    if (showSidebar)
    {
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillRect ((float) getWidth() - (float) sidebarWidth, (float) headerHeight, 1.0f, (float) (getHeight() - headerHeight));
    }
}

bool MultiView::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey) { if (onClose) onClose(); return true; }
    return false;
}

void MultiView::timerCallback()
{
    // the row under the mouse grows a little (animated)
    Row* hovered = nullptr;
    for (auto& r : rows)
        for (auto* m : r->mods)
            if (forcedHover != 0 && m->uid == forcedHover) hovered = r.get();
    if (hovered == nullptr && forcedHover == 0 && isShowing())
    {
        const auto pos = holder.getMouseXYRelative();
        if (viewport.getViewArea().contains (pos))
            for (auto& r : rows)
                if (r->isVisible() && r->getBounds().contains (pos)) hovered = r.get();
    }
    bool moving = false;
    const float base = baseRowHeight();
    for (auto& r : rows)
    {
        r->height.setTarget (r.get() == hovered ? base * 1.3f : base);
        moving |= r->height.tick (1.0 / 60.0, 14.0f);
    }
    if (moving) layoutRows();

    if (++ticks % 6 == 0)
    {
        for (auto& r : rows) for (auto* m : r->mods) if (m->isVisible()) m->repaint();
        sidebar->repaint();
    }
}

} // namespace st
