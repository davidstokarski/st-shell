namespace st
{
namespace modulation_detail
{

//==============================================================================
static juce::RangedAudioParameter& sourceParam (mod::ModMatrix& m, mod::SourceId s, const char* suffix)
{
    auto* p = m.getValueTreeState().getParameter (m.getParamId (s, suffix));
    jassert (p != nullptr); // ModMatrix::addParameters() not called with the matching Config
    return *p;
}

static float rawValue (mod::ModMatrix& m, mod::SourceId s, const char* suffix, float fallback = 0.0f)
{
    if (auto* v = m.getValueTreeState().getRawParameterValue (m.getParamId (s, suffix)))
        return v->load (std::memory_order_relaxed);
    return fallback;
}

/** Small source "handle" glyph: ring + activity-driven core. */
static void drawHandle (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, float activity, bool emphasise)
{
    const auto& t = Theme::get();
    r = r.withSizeKeepingCentre (juce::jmin (r.getWidth(), r.getHeight()), juce::jmin (r.getWidth(), r.getHeight()));

    g.setColour (c.withAlpha (0.14f + 0.22f * activity));
    g.fillEllipse (r.expanded (1.5f * activity));
    g.setColour (c.withAlpha (emphasise ? 1.0f : 0.75f));
    g.drawEllipse (r.reduced (0.75f), 1.4f);

    const float core = r.getWidth() * (0.22f + 0.2f * activity);
    g.setColour (emphasise ? c.brighter (0.25f) : c.interpolatedWith (t.text, 0.1f));
    g.fillEllipse (juce::Rectangle<float> (core, core).withCentre (r.getCentre()));
}

//==============================================================================
class SourceTab : public juce::Component, public juce::SettableTooltipClient
{
public:
    SourceTab (ModulationPanel& p, int i)
        : panel (p), index (i), info (p.getMatrix().getSourceInfo (i))
    {
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        updateTooltip();
        setRepaintsOnMouseActivity (true);
    }

    void tick()
    {
        const float v = panel.getMatrix().getSourceValueNormalised (index);
        if (std::abs (v - activity) > 0.002f)
        {
            activity = v;
            repaint();
        }
    }

    void setNumRoutings (int n)
    {
        if (n != numRoutings) { numRoutings = n; updateTooltip(); repaint(); }
    }

    void updateTooltip()
    {
        setTooltip (info.name + (numRoutings > 0 ? " (" + juce::String (numRoutings) + (numRoutings == 1 ? " target)" : " targets)") : juce::String())
                    + ": click to edit, drag onto a knob to modulate it");
    }

    void paint (juce::Graphics& g) override
    {
        const auto& t = Theme::get();
        const auto c = panel.getSourceColour (index);
        const bool sel = panel.getSelectedSource() == index;
        const bool over = isMouseOver (true);
        auto r = getLocalBounds().toFloat().reduced (1.5f, 3.0f);

        if (sel || over || dragging)
        {
            g.setColour (sel ? t.bg3 : t.bg2.brighter (0.04f));
            g.fillRoundedRectangle (r, t.radiusSmall + 1.0f);
            g.setColour (sel ? c.withAlpha (0.45f) : t.strokeSoft);
            g.drawRoundedRectangle (r, t.radiusSmall + 1.0f, 1.0f);
        }

        // live activity bar along the bottom edge
        auto bar = r.reduced (6.0f, 0.0f).removeFromBottom (2.0f).translated (0.0f, -2.0f);
        g.setColour (t.strokeSoft);
        g.fillRoundedRectangle (bar, 1.0f);
        g.setColour (c.withAlpha (sel ? 0.95f : 0.6f));
        g.fillRoundedRectangle (bar.withWidth (juce::jmax (2.0f, bar.getWidth() * juce::jlimit (0.0f, 1.0f, activity))), 1.0f);

        auto content = r.reduced (5.0f, 0.0f).withTrimmedBottom (3.0f);
        const auto handleC = numRoutings > 0 || sel ? c : c.withMultipliedSaturation (0.55f).withMultipliedAlpha (0.8f);
        drawHandle (g, content.removeFromLeft (11.0f).withSizeKeepingCentre (10.0f, 10.0f), handleC, activity, sel || dragging);
        content.removeFromLeft (5.0f);

        const float h = 10.5f;
        const auto font = fonts::caption (h);
        const auto name = fonts::stringWidth (font, info.name.toUpperCase()) <= content.getWidth() ? info.name : info.shortName;
        paint::caption (g, name, content, sel ? t.text : (over ? t.text.withAlpha (0.85f) : t.textDim),
                        juce::Justification::centredLeft, h);
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        dragging = false;
        panel.selectSource (index);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging || e.getDistanceFromDragStart() < 5)
            return;

        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
        {
            dragging = true;
            container->startDragging (Knob::makeModulationDragDescription (index, panel.getSourceColour (index)),
                                      this, makeDragImage(), false, nullptr, &e.source);
            panel.sourceDragChanged (index);
            repaint();
        }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragging)
        {
            dragging = false;
            panel.sourceDragChanged (-1);
            repaint();
        }
    }

    bool isDragging() const noexcept { return dragging; }

private:
    ModulationPanel& panel;
    const int index;
    const mod::SourceInfo info;
    float activity = 0.0f;
    int numRoutings = 0;
    bool dragging = false;

    juce::ScaledImage makeDragImage()
    {
        const auto& t = Theme::get();
        const auto c = panel.getSourceColour (index);
        const float scale = juce::jmax (0.25f, juce::Component::getApproximateScaleFactorForComponent (this));
        constexpr float pixelDensity = 2.0f;

        const auto font = fonts::caption (11.0f);
        const float w = 34.0f + fonts::stringWidth (font, info.name), h = 26.0f;

        juce::Image img (juce::Image::ARGB, juce::roundToInt (w * scale * pixelDensity) + 1,
                         juce::roundToInt (h * scale * pixelDensity) + 1, true);
        {
            juce::Graphics g (img);
            g.addTransform (juce::AffineTransform::scale (scale * pixelDensity));
            auto r = juce::Rectangle<float> (w, h).reduced (1.0f);
            g.setColour (t.bg2.withAlpha (0.94f));
            g.fillRoundedRectangle (r, h * 0.5f);
            g.setColour (c.withAlpha (0.8f));
            g.drawRoundedRectangle (r, h * 0.5f, 1.2f);
            auto content = r.reduced (8.0f, 0.0f);
            drawHandle (g, content.removeFromLeft (12.0f).withSizeKeepingCentre (11.0f, 11.0f), c, 0.6f, true);
            content.removeFromLeft (6.0f);
            paint::caption (g, info.name, content, t.text, juce::Justification::centredLeft, 11.0f);
        }
        return juce::ScaledImage (img, (double) pixelDensity);
    }
};

//==============================================================================
/** Live view of a source: LFO waveform with playhead, or a scrolling trace (envelopes, random LFOs). */
class SourceDisplay : public juce::Component
{
public:
    SourceDisplay (mod::ModMatrix& m, int i, juce::Colour c)
        : matrix (m), index (i), id (m.getSourceId (i)), colour (c)
    {
        setInterceptsMouseClicks (false, false);
        history.fill (0.0f);
    }

    void tick()
    {
        history[(size_t) writePos] = matrix.getSourceValueNormalised (index);
        writePos = (writePos + 1) % (int) history.size();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto& t = Theme::get();
        auto r = getLocalBounds().toFloat();
        paint::well (g, r, t.radius);

        auto plot = r.reduced (10.0f, 9.0f).withTrimmedTop (9.0f);
        const bool isLfo = id.kind == mod::SourceKind::lfo;
        const auto shape = (mod::LfoShape) juce::jlimit (0, mod::numLfoShapes - 1,
                                                         isLfo ? juce::roundToInt (rawValue (matrix, id, mod::ids::shape)) : 0);
        const bool bipolar = matrix.isSourceBipolar (index);

        // grid
        g.setColour (t.strokeSoft);
        for (int i = 1; i < 4; ++i)
        {
            const float y = plot.getY() + plot.getHeight() * (float) i * 0.25f;
            if (i == 2 && bipolar)
            {
                juce::Path mid;
                mid.startNewSubPath (plot.getX(), y);
                mid.lineTo (plot.getRight(), y);
                const float dashes[] = { 3.0f, 3.0f };
                juce::PathStrokeType (1.0f).createDashedStroke (mid, mid, dashes, 2);
                g.setColour (t.stroke);
                g.fillPath (mid);
                g.setColour (t.strokeSoft);
            }
            else
            {
                g.fillRect (plot.getX(), y, plot.getWidth(), 1.0f);
            }
        }

        auto yFor = [&plot] (float v01) { return plot.getBottom() - juce::jlimit (0.0f, 1.0f, v01) * plot.getHeight(); };
        const float baseY = bipolar ? yFor (0.5f) : plot.getBottom();

        juce::Path curve;
        juce::Point<float> head;

        if (isLfo && ! mod::isRandomShape (shape))
        {
            const float offset = rawValue (matrix, id, mod::ids::phase) / 360.0f;
            constexpr int n = 240;
            for (int i = 0; i <= n; ++i)
            {
                const float x = (float) i / (float) n;
                float p = x + offset;
                p -= std::floor (p);
                const float v = 0.5f * (mod::evaluateLfoShape (shape, juce::jmin (p, 0.99999f)) + 1.0f);
                const juce::Point<float> pt (plot.getX() + x * plot.getWidth(), yFor (v));
                if (i == 0) curve.startNewSubPath (pt); else curve.lineTo (pt);
            }

            float raw = matrix.getLfoPhase (id.index) - offset;
            raw -= std::floor (raw);
            head = { plot.getX() + raw * plot.getWidth(), yFor (matrix.getSourceValueNormalised (index)) };
        }
        else
        {
            const int n = (int) history.size();
            for (int i = 0; i < n; ++i)
            {
                const float v = history[(size_t) ((writePos + i) % n)];
                const juce::Point<float> pt (plot.getX() + plot.getWidth() * (float) i / (float) (n - 1), yFor (v));
                if (i == 0) curve.startNewSubPath (pt); else curve.lineTo (pt);
            }
            head = curve.getCurrentPosition();
        }

        // fill
        {
            juce::Path fill (curve);
            fill.lineTo (plot.getRight(), baseY);
            fill.lineTo (plot.getX(), baseY);
            fill.closeSubPath();
            g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.24f), 0.0f, plot.getY(),
                                                     colour.withAlpha (0.02f), 0.0f, plot.getBottom(), false));
            g.fillPath (fill);
        }

        paint::glowStroke (g, curve, colour, 1.6f, 4.0f);

        // playhead
        if (isLfo && ! mod::isRandomShape (shape))
        {
            g.setColour (colour.withAlpha (0.25f));
            g.fillRect (head.x - 0.5f, plot.getY(), 1.0f, plot.getHeight());
        }
        g.setColour (colour.withAlpha (0.25f));
        g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre (head));
        g.setColour (t.text);
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (head));

        // corner caption
        juce::String tag;
        if (isLfo)
            tag = mod::getLfoShapeNames()[(int) shape] + (bipolar ? "" : "  +");
        else if (id.kind == mod::SourceKind::envelope)
            tag = matrix.getValueTreeState().getParameter (matrix.getParamId (id, mod::ids::input))->getCurrentValueAsText();
        paint::caption (g, tag, r.reduced (10.0f, 4.0f).removeFromTop (12.0f), t.textFaint, juce::Justification::centredRight, 9.5f);
    }

private:
    mod::ModMatrix& matrix;
    const int index;
    const mod::SourceId id;
    const juce::Colour colour;
    std::array<float, 150> history;
    int writePos = 0;
};

//==============================================================================
class SourceEditor : public juce::Component
{
public:
    virtual void tick() {}
};

class LfoEditor : public SourceEditor
{
public:
    LfoEditor (mod::ModMatrix& m, int flat, juce::Colour c)
        : matrix (m), id (m.getSourceId (flat)), display (m, flat, c),
          rate (sourceParam (m, id, mod::ids::rate), m.getUndoManager(), "Rate"),
          phase (sourceParam (m, id, mod::ids::phase), m.getUndoManager(), "Phase"),
          division (sourceParam (m, id, mod::ids::division), m.getUndoManager()),
          shape (sourceParam (m, id, mod::ids::shape), m.getUndoManager()),
          sync (sourceParam (m, id, mod::ids::sync), m.getUndoManager(), "SYNC", c),
          unipolar (sourceParam (m, id, mod::ids::unipolar), m.getUndoManager(), "UNI", c)
    {
        for (auto* k : { &rate, &phase })
        {
            k->setSize (Knob::Size::small);
            k->setColour (c);
            addAndMakeVisible (k);
        }
        division.setAccent (c);
        shape.setAccent (c);
        sync.setTooltip ("Sync the rate to the host tempo");
        unipolar.setTooltip ("Unipolar: modulate in one direction only (0..+depth)");
        for (juce::Component* comp : { (juce::Component*) &display, (juce::Component*) &division, (juce::Component*) &shape,
                                       (juce::Component*) &sync, (juce::Component*) &unipolar })
            addAndMakeVisible (comp);
        updateSyncVisibility();
    }

    void tick() override
    {
        updateSyncVisibility();
        display.tick();
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto controls = r.removeFromBottom (66);
        r.removeFromBottom (8);
        display.setBounds (r);

        auto rateSlot = controls.removeFromLeft (70);
        rate.setBounds (rateSlot.withSizeKeepingCentre (58, 66));
        divisionArea = rateSlot.withSizeKeepingCentre (rateSlot.getWidth(), 66);
        division.setBounds (divisionArea.withTrimmedBottom (15).withSizeKeepingCentre (rateSlot.getWidth() - 6, 26));
        phase.setBounds (controls.removeFromLeft (64).withSizeKeepingCentre (58, 66));

        // One calm row of compact controls instead of full-width blocks.
        controls.removeFromLeft (12);
        auto row = controls.withSizeKeepingCentre (controls.getWidth(), 28).translated (0, -8);
        shape.setBounds (row.removeFromLeft (juce::jmin (190, row.getWidth() / 2)));
        row.removeFromLeft (10);
        sync.setBounds (row.removeFromLeft (64));
        row.removeFromLeft (6);
        unipolar.setBounds (row.removeFromLeft (56));
    }

    void paint (juce::Graphics& g) override
    {
        if (division.isVisible())
            paint::caption (g, "Rate", divisionArea.toFloat().removeFromBottom (15.0f),
                            Theme::get().textDim, juce::Justification::centred, 10.0f);
    }

private:
    mod::ModMatrix& matrix;
    const mod::SourceId id;
    SourceDisplay display;
    Knob rate, phase;
    Dropdown division, shape;
    ToggleChip sync, unipolar;
    juce::Rectangle<int> divisionArea;

    void updateSyncVisibility()
    {
        const bool synced = rawValue (matrix, id, mod::ids::sync) >= 0.5f;
        if (division.isVisible() != synced)
        {
            division.setVisible (synced);
            rate.setVisible (! synced);
            repaint();
        }
    }
};

class EnvelopeEditor : public SourceEditor
{
public:
    EnvelopeEditor (mod::ModMatrix& m, int flat, juce::Colour c)
        : id (m.getSourceId (flat)), display (m, flat, c),
          attack (sourceParam (m, id, mod::ids::attack), m.getUndoManager(), "Attack"),
          release (sourceParam (m, id, mod::ids::release), m.getUndoManager(), "Release"),
          gain (sourceParam (m, id, mod::ids::gain), m.getUndoManager(), "Gain"),
          input (sourceParam (m, id, mod::ids::input), m.getUndoManager())
    {
        for (auto* k : { &attack, &release, &gain })
        {
            k->setSize (Knob::Size::small);
            k->setColour (c);
            addAndMakeVisible (k);
        }
        gain.setBipolar (true);
        gain.setTooltip ("Sensitivity: boosts the detected level before it drives the targets");
        input.setAccent (c);
        input.setEnabled (m.getConfig().envelopeInputs.size() > 1);
        addAndMakeVisible (display);
        addAndMakeVisible (input);
    }

    void tick() override { display.tick(); }

    void resized() override
    {
        auto r = getLocalBounds();
        auto controls = r.removeFromBottom (66);
        r.removeFromBottom (8);
        display.setBounds (r);

        for (auto* k : { &attack, &release, &gain })
            k->setBounds (controls.removeFromLeft (62).withSizeKeepingCentre (58, 66));

        controls.removeFromLeft (8);
        inputArea = controls.withSizeKeepingCentre (controls.getWidth(), 66);
        input.setBounds (inputArea.withTrimmedBottom (15).withSizeKeepingCentre (inputArea.getWidth(), 26));
    }

    void paint (juce::Graphics& g) override
    {
        paint::caption (g, "Input", inputArea.toFloat().removeFromBottom (15.0f), Theme::get().textDim,
                        juce::Justification::centred, 10.0f);
    }

private:
    const mod::SourceId id;
    SourceDisplay display;
    Knob attack, release, gain;
    Dropdown input;
    juce::Rectangle<int> inputArea;
};

class MacroEditor : public SourceEditor
{
public:
    MacroEditor (mod::ModMatrix& m, int flat, juce::Colour c)
        : id (m.getSourceId (flat)), colour (c),
          knob (sourceParam (m, id, ""), m.getUndoManager(), m.getSourceName (id))
    {
        knob.setSize (Knob::Size::large);
        knob.setColour (c);
        knob.setShowValueAlways (false);
        addAndMakeVisible (knob);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        const bool narrow = r.getWidth() < 300;
        auto hint = r.removeFromBottom (narrow ? 30 : 18);
        hintArea = hint;
        const int d = juce::jmin (r.getHeight(), narrow ? 118 : 140);
        knob.setBounds (r.withSizeKeepingCentre (d - 14, d));
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (Theme::get().textFaint);
        g.setFont (fonts::regular (11.0f));
        g.drawFittedText ("Drag this source onto any knob, then automate the macro", hintArea, juce::Justification::centred, 2, 1.0f);
    }

private:
    const mod::SourceId id;
    const juce::Colour colour;
    Knob knob;
    juce::Rectangle<int> hintArea;
};

//==============================================================================
/** Bipolar depth slider: drag horizontally, shift = fine, double-click = 0. */
class DepthSlider : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit DepthSlider (juce::Colour c) : colour (c)
    {
        setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
        setRepaintsOnMouseActivity (true);
        setTooltip ("Depth: drag to adjust (shift = fine), double-click to reset");
    }

    std::function<void()> onDragStart;
    std::function<void (float)> onChange;

    void setValue (float v)
    {
        if (! dragging && ! juce::exactlyEqual (v, value)) { value = v; repaint(); }
    }

    void setActive (bool a) { if (a != active) { active = a; repaint(); } }

    void paint (juce::Graphics& g) override
    {
        const auto& t = Theme::get();
        auto r = getLocalBounds().toFloat();
        auto textArea = r.removeFromRight (40.0f);
        auto track = r.withSizeKeepingCentre (r.getWidth(), 14.0f).reduced (1.0f, 0.0f);
        const auto c = active ? colour : t.textFaint;

        g.setColour (isMouseOver() || dragging ? t.bg4 : t.bg1);
        g.fillRoundedRectangle (track, 4.0f);
        g.setColour (t.stroke);
        g.drawRoundedRectangle (track, 4.0f, 1.0f);

        auto inner = track.reduced (2.0f);
        const float cx = inner.getCentreX();
        const float x = cx + value * inner.getWidth() * 0.5f;
        auto fill = juce::Rectangle<float>::leftTopRightBottom (juce::jmin (cx, x), inner.getY(), juce::jmax (cx, x), inner.getBottom());
        g.setColour (c.withAlpha (0.55f));
        g.fillRoundedRectangle (fill, 2.5f);

        g.setColour (t.textFaint);
        g.fillRect (cx - 0.5f, track.getY() + 2.0f, 1.0f, track.getHeight() - 4.0f);

        g.setColour (active ? t.text : t.textDim);
        g.fillRoundedRectangle (juce::Rectangle<float> (2.0f, inner.getHeight() + 2.0f).withCentre ({ x, inner.getCentreY() }), 1.0f);

        g.setColour (active ? t.text : t.textDim);
        g.setFont (fonts::semibold (11.0f));
        g.drawText (params::formatPercent (value * 100.0f, true), textArea, juce::Justification::centredRight, false);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragging = true;
        lastX = e.position.x;
        if (onDragStart) onDragStart();
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const float trackW = juce::jmax (40.0f, (float) getWidth() - 44.0f);
        const float dx = e.position.x - lastX;
        lastX = e.position.x;
        const float delta = dx / trackW * 2.0f * (e.mods.isShiftDown() ? 0.1f : 1.0f);
        set (value + delta);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        dragging = false;
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        if (onDragStart) onDragStart();
        set (0.0f);
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const float dy = std::abs (w.deltaY) > std::abs (w.deltaX) ? w.deltaY : -w.deltaX;
        if (dy == 0.0f) return;
        if (onDragStart) onDragStart();
        set (value + (dy > 0.0f ? 1.0f : -1.0f) * (e.mods.isShiftDown() ? 0.002f : 0.01f) * (w.isReversed ? -1.0f : 1.0f));
    }

private:
    juce::Colour colour;
    float value = 0.0f, lastX = 0.0f;
    bool dragging = false, active = true;

    void set (float v)
    {
        v = juce::jlimit (-1.0f, 1.0f, v);
        if (std::abs (v) < 0.004f) v = 0.0f; // detent at zero
        if (juce::exactlyEqual (v, value)) return;
        value = v;
        if (onChange) onChange (value);
        repaint();
    }
};

//==============================================================================
class RoutingRow : public juce::Component
{
public:
    RoutingRow (mod::ModMatrix& m, const mod::Routing& r, juce::Colour c)
        : matrix (m), source (r.source), targetId (r.targetId), colour (c), slider (c)
    {
        if (auto* p = m.getValueTreeState().getParameter (targetId))
            name = p->getName (64);
        else
            name = targetId;

        slider.onDragStart = [this] { matrix.beginUndoTransaction ("Modulation Depth"); };
        slider.onChange = [this] (float v) { matrix.setRoutingDepth (source, targetId, v); };
        removeButton.onClick = [this]
        {
            // defer: removing the routing destroys this row
            juce::MessageManager::callAsync ([mm = &matrix, s = source, id = targetId] { mm->removeRouting (s, id); });
        };
        addAndMakeVisible (slider);
        addAndMakeVisible (removeButton);
        update (r);
    }

    const juce::String& getTargetId() const noexcept { return targetId; }
    mod::SourceId getSource() const noexcept { return source; }

    void update (const mod::Routing& r)
    {
        slider.setValue (r.depth);
        slider.setActive (r.enabled);
        if (enabled != r.enabled) { enabled = r.enabled; repaint(); }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (4, 0);
        removeButton.setBounds (r.removeFromRight (20).withSizeKeepingCentre (20, 20));
        r.removeFromRight (4);
        slider.setBounds (r.removeFromRight (juce::jlimit (90, 170, r.getWidth() / 2 + 10)));
        nameArea = r;
    }

    void paint (juce::Graphics& g) override
    {
        const auto& t = Theme::get();
        auto r = nameArea.toFloat();
        dotArea = r.removeFromLeft (16.0f);
        auto dot = dotArea.withSizeKeepingCentre (8.0f, 8.0f);

        if (enabled) { g.setColour (colour); g.fillEllipse (dot); }
        else         { g.setColour (t.textFaint); g.drawEllipse (dot.reduced (0.5f), 1.2f); }

        r.removeFromLeft (4.0f);
        g.setColour (enabled ? t.text : t.textFaint);
        g.setFont (fonts::medium (12.0f));
        g.drawText (name, r, juce::Justification::centredLeft, true);

        g.setColour (t.strokeSoft);
        g.fillRect (4.0f, (float) getHeight() - 1.0f, (float) getWidth() - 8.0f, 1.0f);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (dotArea.expanded (2.0f).contains (e.position))
            juce::MessageManager::callAsync ([mm = &matrix, s = source, id = targetId, en = ! enabled] { mm->setRoutingEnabled (s, id, en); });
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        setMouseCursor (dotArea.expanded (2.0f).contains (e.position) ? juce::MouseCursor::PointingHandCursor
                                                                         : juce::MouseCursor::NormalCursor);
    }

private:
    mod::ModMatrix& matrix;
    const mod::SourceId source;
    const juce::String targetId;
    const juce::Colour colour;
    juce::String name;
    bool enabled = true;
    DepthSlider slider;
    IconButton removeButton { icons::Icon::close, "Remove this modulation" };
    juce::Rectangle<int> nameArea;
    juce::Rectangle<float> dotArea;
};

//==============================================================================
static juce::Colour colourForSource (mod::ModMatrix& m, int flat)
{
    const auto c = m.getSourceColour (flat);
    return c.isTransparent() ? Theme::get().seriesColour (m.getSourceColourIndex (m.getSourceId (flat))) : c;
}

/** One source's editor + its routing list (used by ModulationStrip's popover). */
class SourceEditorPanel : public juce::Component, private juce::ChangeListener, private juce::Timer
{
public:
    SourceEditorPanel (mod::ModMatrix& m, int flat)
        : matrix (m), index (flat), id (m.getSourceId (flat)), colour (colourForSource (m, flat))
    {
        switch (id.kind)
        {
            case mod::SourceKind::lfo:      editor = std::make_unique<LfoEditor> (matrix, index, colour); break;
            case mod::SourceKind::envelope: editor = std::make_unique<EnvelopeEditor> (matrix, index, colour); break;
            case mod::SourceKind::macro:    editor = std::make_unique<MacroEditor> (matrix, index, colour); break;
        }
        addAndMakeVisible (*editor);

        viewport.setViewedComponent (&listContent, false);
        viewport.setScrollBarsShown (true, false);
        viewport.setScrollBarThickness (6);
        addAndMakeVisible (viewport);

        matrix.addChangeListener (this);
        setSize ((id.kind == mod::SourceKind::macro ? 190 : 420) + gap + listWidth, 200);
        refreshRoutings();
        startTimerHz (40);
    }

    ~SourceEditorPanel() override
    {
        stopTimer();
        matrix.removeChangeListener (this);
        viewport.setViewedComponent (nullptr, false);
        rows.clear();
    }

    void resized() override
    {
        auto r = getLocalBounds();
        listArea = r.removeFromRight (listWidth);
        r.removeFromRight (gap);
        editor->setBounds (r);

        listHeader = listArea.removeFromTop (18);
        listArea.removeFromTop (6);
        viewport.setBounds (listArea);
        layoutRows();
    }

    void paint (juce::Graphics& g) override
    {
        const auto& t = Theme::get();
        g.setColour (t.strokeSoft);
        g.fillRect ((float) listArea.getX() - (float) gap * 0.5f, 2.0f, 1.0f, (float) getHeight() - 4.0f);

        auto h = listHeader.toFloat();
        paint::caption (g, "Routings", h, t.textDim, juce::Justification::centredLeft, 10.0f);
        g.setColour (t.textFaint);
        g.setFont (fonts::medium (10.5f));
        g.drawText (juce::String (matrix.getNumRoutings()) + " / " + juce::String (matrix.getConfig().maxRoutings) + " slots",
                    h, juce::Justification::centredRight, false);

        if (rows.isEmpty())
        {
            auto hint = listArea.toFloat().withSizeKeepingCentre ((float) listArea.getWidth() - 16.0f, 58.0f);
            drawHandle (g, hint.removeFromTop (16.0f).withSizeKeepingCentre (14.0f, 14.0f), colour, 0.4f, true);
            hint.removeFromTop (8.0f);
            g.setColour (t.textDim);
            g.setFont (fonts::medium (12.0f));
            g.drawText ("Drag the connector onto a knob", hint.removeFromTop (16.0f), juce::Justification::centred, true);
            g.setColour (t.textFaint);
            g.setFont (fonts::regular (11.0f));
            g.drawText ("Right-click a knob to remove modulation", hint.removeFromTop (15.0f), juce::Justification::centred, true);
        }
    }

private:
    static constexpr int listWidth = 250, gap = 22;

    mod::ModMatrix& matrix;
    const int index;
    const mod::SourceId id;
    const juce::Colour colour;
    std::unique_ptr<SourceEditor> editor;
    juce::Viewport viewport;
    juce::Component listContent;
    juce::OwnedArray<RoutingRow> rows;
    juce::Rectangle<int> listArea, listHeader;

    void refreshRoutings()
    {
        const auto mine = matrix.getRoutingsForSource (id);

        bool sameSet = (int) mine.size() == rows.size();
        for (int i = 0; sameSet && i < rows.size(); ++i)
            sameSet = rows[i]->getTargetId() == mine[(size_t) i].targetId;

        if (sameSet)
        {
            for (int i = 0; i < rows.size(); ++i)
                rows[i]->update (mine[(size_t) i]);
        }
        else
        {
            rows.clear();
            for (auto& r : mine)
                listContent.addAndMakeVisible (rows.add (new RoutingRow (matrix, r, colour)));
            layoutRows();
        }
        repaint();
    }

    void layoutRows()
    {
        constexpr int rowH = 28;
        const bool needsScroll = rows.size() * rowH > viewport.getHeight();
        const int w = viewport.getWidth() - (needsScroll ? viewport.getScrollBarThickness() + 2 : 0);
        listContent.setSize (juce::jmax (1, w), juce::jmax (1, rows.size() * rowH));
        for (int i = 0; i < rows.size(); ++i)
            rows[i]->setBounds (0, i * rowH, w, rowH);
    }

    void changeListenerCallback (juce::ChangeBroadcaster*) override { refreshRoutings(); }
    void timerCallback() override { editor->tick(); }
};

} // namespace modulation_detail

std::unique_ptr<juce::Component> createModSourceEditor (mod::ModMatrix& m, int flatIndex)
{
    if (! juce::isPositiveAndBelow (flatIndex, m.getNumSources()))
        return {};
    return std::make_unique<modulation_detail::SourceEditorPanel> (m, flatIndex);
}

//==============================================================================
using namespace modulation_detail;

ModulationPanel::ModulationPanel (mod::ModMatrix& m) : matrix (m)
{
    juce::Array<juce::Colour> palette;
    for (auto c : Theme::get().series)
        palette.add (c);
    matrix.setSourcePalette (palette);

    for (int i = 0; i < matrix.getNumSources(); ++i)
        addAndMakeVisible (tabs.add (new SourceTab (*this, i)));

    viewport.setViewedComponent (&listContent, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (6);
    addAndMakeVisible (viewport);

    matrix.addChangeListener (this);
    rebuildEditor();
    refreshRoutings();
    startTimerHz (40);
}

ModulationPanel::~ModulationPanel()
{
    stopTimer();
    matrix.removeChangeListener (this);
    viewport.setViewedComponent (nullptr, false);
    rows.clear();
    editor.reset();
}

juce::Colour ModulationPanel::getSourceColour (int i) const
{
    return Theme::get().seriesColour (matrix.getSourceColourIndex (matrix.getSourceId (i)));
}

void ModulationPanel::selectSource (int i)
{
    i = juce::jlimit (0, juce::jmax (0, matrix.getNumSources() - 1), i);
    if (i == selected && editor != nullptr)
        return;

    selected = i;
    rebuildEditor();
    refreshRoutings();
    for (auto* t : tabs) t->repaint();
    repaint();

    if (onSourceSelected) onSourceSelected (selected);
}

void ModulationPanel::rebuildEditor()
{
    editor.reset();
    if (matrix.getNumSources() == 0)
        return;

    const auto id = matrix.getSourceId (selected);
    const auto c = getSourceColour (selected);

    switch (id.kind)
    {
        case mod::SourceKind::lfo:      editor = std::make_unique<LfoEditor> (matrix, selected, c); break;
        case mod::SourceKind::envelope: editor = std::make_unique<EnvelopeEditor> (matrix, selected, c); break;
        case mod::SourceKind::macro:    editor = std::make_unique<MacroEditor> (matrix, selected, c); break;
    }

    addAndMakeVisible (*editor);
    editor->setBounds (editorArea);
}

void ModulationPanel::refreshRoutings()
{
    if (matrix.getNumSources() == 0)
        return;

    const auto all = matrix.getRoutings();

    for (int i = 0; i < tabs.size(); ++i)
    {
        const auto id = matrix.getSourceId (i);
        tabs[i]->setNumRoutings ((int) std::count_if (all.begin(), all.end(), [&] (const mod::Routing& r) { return r.source == id; }));
    }

    const auto src = matrix.getSourceId (selected);
    std::vector<mod::Routing> mine;
    for (auto& r : all)
        if (r.source == src)
            mine.push_back (r);

    bool sameSet = (int) mine.size() == rows.size();
    for (int i = 0; sameSet && i < rows.size(); ++i)
        sameSet = rows[i]->getTargetId() == mine[(size_t) i].targetId && rows[i]->getSource() == src;

    if (sameSet)
    {
        for (int i = 0; i < rows.size(); ++i)
            rows[i]->update (mine[(size_t) i]);
    }
    else
    {
        rows.clear();
        const auto c = getSourceColour (selected);
        for (auto& r : mine)
            listContent.addAndMakeVisible (rows.add (new RoutingRow (matrix, r, c)));
        layoutRows();
    }

    repaint (listHeader);
    repaint (listArea);
}

void ModulationPanel::layoutRows()
{
    constexpr int rowH = 28;
    const bool needsScroll = rows.size() * rowH > viewport.getHeight();
    const int w = viewport.getWidth() - (needsScroll ? viewport.getScrollBarThickness() + 2 : 0);
    listContent.setSize (juce::jmax (1, w), juce::jmax (1, rows.size() * rowH));
    for (int i = 0; i < rows.size(); ++i)
        rows[i]->setBounds (0, i * rowH, w, rowH);
}

void ModulationPanel::sourceDragChanged (int i)
{
    if (onSourceDrag) onSourceDrag (i);
}

void ModulationPanel::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshRoutings();
}

void ModulationPanel::timerCallback()
{
    for (auto* t : tabs) t->tick();
    if (editor != nullptr) editor->tick();
}

void ModulationPanel::paint (juce::Graphics& g)
{
    const auto& t = Theme::get();
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    if (! borderless)
    {
        g.setGradientFill (juce::ColourGradient (t.bg1.brighter (0.05f), 0.0f, r.getY(), t.bg1.darker (0.1f), 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, t.radiusLarge);
        g.setGradientFill (juce::ColourGradient (t.highlight, 0.0f, r.getY(), t.highlight.withAlpha (0.0f), 0.0f, r.getY() + t.radiusLarge * 3.0f, false));
        g.drawRoundedRectangle (r.reduced (0.5f), t.radiusLarge, 1.0f);
        g.setColour (t.stroke.withAlpha (0.9f));
        g.drawRoundedRectangle (r, t.radiusLarge, t.hairline);
    }

    // tab row divider
    g.setColour (borderless ? t.strokeSoft.withAlpha (0.5f) : t.strokeSoft);
    g.fillRect ((float) tabArea.getX(), (float) tabArea.getBottom() + 3.0f, (float) tabArea.getWidth(), 1.0f);

    // group separators between source kinds
    for (int i = 1; i < tabs.size(); ++i)
    {
        if (matrix.getSourceId (i).kind != matrix.getSourceId (i - 1).kind)
        {
            const float x = ((float) tabs[i - 1]->getRight() + (float) tabs[i]->getX()) * 0.5f;
            g.fillRect (x - 0.5f, (float) tabArea.getY() + 8.0f, 1.0f, (float) tabArea.getHeight() - 16.0f);
        }
    }

    // editor / routing divider
    g.fillRect ((float) listArea.getX() - 9.0f, (float) editorArea.getY(), 1.0f, (float) editorArea.getHeight());

    // routing list header
    if (matrix.getNumSources() > 0)
    {
        auto h = listHeader.toFloat();
        const auto c = getSourceColour (selected);
        paint::caption (g, "Routings", h, t.textDim, juce::Justification::centredLeft, 10.0f);

        const auto count = juce::String (matrix.getNumRoutings()) + " / " + juce::String (matrix.getConfig().maxRoutings) + " slots";
        g.setColour (t.textFaint);
        g.setFont (fonts::medium (10.5f));
        g.drawText (count, h, juce::Justification::centredRight, false);

        if (rows.isEmpty())
        {
            auto hint = listArea.toFloat().withSizeKeepingCentre ((float) listArea.getWidth() - 16.0f, 54.0f);
            drawHandle (g, hint.removeFromTop (16.0f).withSizeKeepingCentre (14.0f, 14.0f), c, 0.4f, true);
            hint.removeFromTop (6.0f);
            g.setColour (t.textDim);
            g.setFont (fonts::medium (12.0f));
            g.drawText ("Drag " + matrix.getSourceName (matrix.getSourceId (selected)) + " onto a knob",
                        hint.removeFromTop (16.0f), juce::Justification::centred, true);
            g.setColour (t.textFaint);
            g.setFont (fonts::regular (11.0f));
            g.drawText ("Right-click a knob to remove modulation", hint.removeFromTop (15.0f), juce::Justification::centred, true);
        }
    }
}

void ModulationPanel::resized()
{
    auto r = getLocalBounds().reduced (10, 6);

    tabArea = r.removeFromTop (34);
    r.removeFromTop (12);
    r.removeFromBottom (4);

    // tabs: equal widths with a small gap between source kinds
    {
        int groups = 0;
        for (int i = 1; i < tabs.size(); ++i)
            if (matrix.getSourceId (i).kind != matrix.getSourceId (i - 1).kind)
                ++groups;

        constexpr int groupGap = 10;
        const float w = tabs.isEmpty() ? 0.0f : (float) (tabArea.getWidth() - groups * groupGap) / (float) tabs.size();
        float x = (float) tabArea.getX();
        for (int i = 0; i < tabs.size(); ++i)
        {
            if (i > 0 && matrix.getSourceId (i).kind != matrix.getSourceId (i - 1).kind)
                x += (float) groupGap;
            tabs[i]->setBounds (juce::roundToInt (x), tabArea.getY(), juce::roundToInt (x + w) - juce::roundToInt (x), tabArea.getHeight());
            x += w;
        }
    }

    const int listW = juce::jlimit (220, 340, (int) ((float) r.getWidth() * 0.42f));
    listArea = r.removeFromRight (listW);
    r.removeFromRight (18);
    editorArea = r;

    listHeader = listArea.removeFromTop (18);
    listArea.removeFromTop (4);
    viewport.setBounds (listArea);
    layoutRows();

    if (editor != nullptr)
        editor->setBounds (editorArea);
}

} // namespace st
