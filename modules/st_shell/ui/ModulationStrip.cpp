namespace st
{
namespace strip_detail
{

//==============================================================================
// Geometry (design pixels, relative to the top of a column; the column is centred vertically).
static constexpr float columnWidth = 104.0f;
static constexpr float connectorY  = 16.0f;
static constexpr float tileTop     = 39.0f;
static constexpr float tileW       = 76.0f;
static constexpr float tileH       = 70.0f;
static constexpr float macroW      = 46.0f;
static constexpr float macroBottom = 127.0f;
static constexpr float nameTop     = 129.0f;
static constexpr float nameH       = 17.0f;
static constexpr float tileRadius  = 8.0f;
static constexpr float pitch       = 112.0f;   // distance between tile centres
static constexpr float addPitch    = 88.0f;    // last tile -> "+" tile
static constexpr float addSize     = 34.0f;

static float verticalOffset (const juce::Component& c)
{
    return juce::jmax (0.0f, ((float) c.getHeight() - (float) ModulationStrip::preferredHeight) * 0.5f);
}

/** Dark rounded tile with an outer shadow, inner top shadow and hairline border (Saturn-style). */
static void paintTileFrame (juce::Graphics& g, juce::Rectangle<float> r, float radius, juce::Colour border)
{
    const auto& t = Theme::get();
    paint::softShadow (g, r, radius, 9.0f, 0.7f);

    g.setGradientFill (juce::ColourGradient (t.bg2.brighter (0.03f), 0.0f, r.getY(),
                                             t.bg1.darker (0.25f), 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle (r, radius);

    {
        juce::Graphics::ScopedSaveState state (g);
        juce::Path clip;
        clip.addRoundedRectangle (r, radius);
        g.reduceClipRegion (clip);

        // inner shadow along the top and a faint vignette on the sides
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.6f), 0.0f, r.getY(),
                                                 juce::Colours::transparentBlack, 0.0f, r.getY() + 13.0f, false));
        g.fillRect (r.withHeight (13.0f));
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.25f), r.getX(), 0.0f,
                                                 juce::Colours::transparentBlack, r.getX() + 8.0f, 0.0f, false));
        g.fillRect (r.withWidth (8.0f));
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.25f), r.getRight(), 0.0f,
                                                 juce::Colours::transparentBlack, r.getRight() - 8.0f, 0.0f, false));
        g.fillRect (r.withLeft (r.getRight() - 8.0f));
    }

    g.setColour (border);
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);

    // lower bevel catch-light
    g.setColour (juce::Colours::white.withAlpha (0.045f));
    g.drawLine (r.getX() + radius, r.getBottom() - 1.0f, r.getRight() - radius, r.getBottom() - 1.0f, 1.0f);
}

static juce::RangedAudioParameter* findParam (mod::ModMatrix& m, mod::SourceId s, const char* suffix)
{
    return m.getValueTreeState().getParameter (m.getParamId (s, suffix));
}

static float rawParam (mod::ModMatrix& m, mod::SourceId s, const char* suffix, float fallback = 0.0f)
{
    if (auto* v = m.getValueTreeState().getRawParameterValue (m.getParamId (s, suffix)))
        return v->load (std::memory_order_relaxed);
    return fallback;
}

static void setParamPlain (juce::RangedAudioParameter* p, float plain)
{
    if (p == nullptr) return;
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->convertTo0to1 (plain));
    p->endChangeGesture();
}

//==============================================================================
class SourceColumn : public juce::Component, public juce::SettableTooltipClient
{
public:
    SourceColumn (ModulationStrip& s, int flat)
        : strip (s), matrix (s.getMatrix()), index (flat), id (matrix.getSourceId (flat)),
          info (matrix.getSourceInfo (flat)), colour (s.getSourceColour (flat))
    {
        history.fill (0.0f);
        setRepaintsOnMouseActivity (false);

        if (isMacro())
        {
            if (auto* p = findParam (matrix, id, ""))
            {
                macroParam = p;
                attachment = std::make_unique<juce::ParameterAttachment> (*p, [this] (float plain)
                {
                    macroValue = macroParam->convertTo0to1 (plain);
                    repaint();
                }, matrix.getUndoManager());
                attachment->sendInitialUpdate();
            }
        }

        setTooltip (info.name + (isMacro() ? ": drag the dot onto a knob, drag the fader to set the value, click to edit"
                                           : ": drag the dot onto a knob to modulate it, click to edit"));
    }

    int getIndex() const noexcept { return index; }
    bool isMacro() const noexcept { return id.kind == mod::SourceKind::macro; }

    Animated xPos, alpha;

    juce::Point<float> getConnectorCentre() const
    {
        return { (float) getWidth() * 0.5f, verticalOffset (*this) + connectorY };
    }

    juce::Rectangle<float> getTileArea() const
    {
        const float y0 = verticalOffset (*this);
        const float cx = (float) getWidth() * 0.5f;
        if (isMacro())
            return juce::Rectangle<float>::leftTopRightBottom (cx - macroW * 0.5f, y0 + connectorY, cx + macroW * 0.5f, y0 + macroBottom);
        return { cx - tileW * 0.5f, y0 + tileTop, tileW, tileH };
    }

    juce::Rectangle<float> getNameArea() const
    {
        return { 0.0f, verticalOffset (*this) + nameTop, (float) getWidth(), nameH };
    }

    void tick (double dt)
    {
        const float v = matrix.getSourceValueNormalised (index);
        bool dirty = false;

        if (id.kind == mod::SourceKind::lfo)
        {
            const int shape = juce::roundToInt (rawParam (matrix, id, mod::ids::shape));
            const float phaseOffset = rawParam (matrix, id, mod::ids::phase);
            const float phase = matrix.getLfoPhase (id.index);
            const bool bip = matrix.isSourceBipolar (index);
            if (shape != lastShape || ! juce::exactlyEqual (phaseOffset, lastPhaseOffset) || bip != lastBipolar)
            {
                lastShape = shape; lastPhaseOffset = phaseOffset; lastBipolar = bip;
                dirty = true;
            }
            if (std::abs (phase - lastPhase) > 0.0005f || std::abs (v - lastValue) > 0.001f)
                dirty = true;
            lastPhase = phase;
        }

        // Scrolling trace for envelopes and random LFOs (sampled at a fixed rate, independent of fps).
        sampleClock += dt;
        while (sampleClock >= samplePeriod)
        {
            sampleClock -= samplePeriod;
            history[(size_t) writePos] = v;
            writePos = (writePos + 1) % (int) history.size();
            if (usesTrace())
            {
                const float prev = history[(size_t) ((writePos + (int) history.size() - 2) % (int) history.size())];
                if (std::abs (v - prev) > 0.0005f || std::abs (v - history[(size_t) writePos]) > 0.0005f)
                    dirty = true;
            }
        }

        if (std::abs (v - lastValue) > 0.002f && ! usesTrace())
            dirty = true;
        lastValue = v;

        // live halo around the connector
        if (std::abs (v - haloValue) > 0.01f)
        {
            haloValue = v;
            repaint (getConnectorArea().toNearestInt());
        }

        if (dirty)
            repaint (getTileArea().expanded (2.0f).toNearestInt());
    }

    void paint (juce::Graphics& g) override
    {
        const auto& t = Theme::get();
        const bool open = strip.getPopover() != nullptr && strip.getPopover()->getSourceIndex() == index;
        const bool sel = strip.getSelectedSource() == index;
        const auto tile = getTileArea();

        auto border = t.stroke;
        if (hoverPart == Part::tile || pressPart == Part::tile) border = t.bg4.brighter (0.2f);
        if (sel)  border = colour.withAlpha (0.38f).interpolatedWith (t.stroke, 0.35f);
        if (open) border = colour.withAlpha (0.75f);

        paintTileFrame (g, tile, isMacro() ? tileRadius + 1.0f : tileRadius, border);

        {
            juce::Graphics::ScopedSaveState state (g);
            juce::Path clip;
            clip.addRoundedRectangle (tile.reduced (1.0f), tileRadius - 1.0f);
            g.reduceClipRegion (clip);

            if (isMacro())                paintMacro (g, tile);
            else if (usesTrace())         paintTrace (g, tile);
            else                          paintLfo (g, tile);
        }

        // connector (drawn after the tile so it can sit on a tall macro tile's top edge)
        const auto c = getConnectorCentre();
        const bool connHover = hoverPart == Part::connector || strip.getDraggingSource() == index;
        g.setColour (colour.withAlpha (0.03f + 0.08f * haloValue));
        g.fillEllipse (juce::Rectangle<float> (ModulationStrip::connectorRadius * 4.2f, ModulationStrip::connectorRadius * 4.2f).withCentre (c));
        paint::connectorDot (g, c, ModulationStrip::connectorRadius, colour, connHover);

        // name + chevron
        auto nameArea = getNameArea();
        const auto font = fonts::semibold (12.5f);
        const float textW = fonts::stringWidth (font, info.name);
        const float chevW = 9.0f, gap = 5.0f;
        auto row = nameArea.withSizeKeepingCentre (juce::jmin (nameArea.getWidth(), textW + gap + chevW), nameArea.getHeight());
        const bool nameHover = hoverPart == Part::name;
        g.setColour (nameHover || open ? t.text : t.text.withAlpha (0.82f));
        g.setFont (font);
        g.drawText (info.name, row.removeFromLeft (textW + 1.0f), juce::Justification::centredLeft, false);
        row.removeFromLeft (gap - 1.0f);
        icons::draw (g, icons::Icon::chevronDown, row.withSizeKeepingCentre (chevW, chevW).translated (0.0f, 0.5f),
                     nameHover ? t.text : t.textFaint, 1.7f);
    }

    bool hitTest (int x, int y) override
    {
        return partAt ({ (float) x, (float) y }) != Part::none;
    }

    void mouseMove (const juce::MouseEvent& e) override { setHover (partAt (e.position)); }
    void mouseExit (const juce::MouseEvent&) override   { setHover (Part::none); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        pressPart = partAt (e.position);
        dragStarted = macroDragging = false;
        lastY = e.position.y;

        if (e.mods.isPopupMenu() || pressPart == Part::name)
        {
            pressPart = Part::none;
            showMenu();
            return;
        }
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (pressPart == Part::none || dragStarted)
            return;

        if (pressPart == Part::tile && isMacro())
        {
            if (! macroDragging)
            {
                if (e.getDistanceFromDragStart() < 3 || attachment == nullptr)
                    return;
                macroDragging = true;
                attachment->beginGesture();
                lastY = e.position.y;
            }
            const float travel = juce::jmax (30.0f, getTileArea().getHeight() - 12.0f);
            const float delta = (lastY - e.position.y) / travel * (e.mods.isShiftDown() ? 0.1f : 1.0f);
            lastY = e.position.y;
            macroValue = juce::jlimit (0.0f, 1.0f, macroValue + delta);
            attachment->setValueAsPartOfGesture (macroParam->convertFrom0to1 (macroValue));
            repaint();
            return;
        }

        if (e.getDistanceFromDragStart() >= 4)
            startModulationDrag (e);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        const auto part = pressPart;
        pressPart = Part::none;

        if (dragStarted)
        {
            dragStarted = false;
            strip.setDragging (-1);
        }
        else if (macroDragging)
        {
            macroDragging = false;
            attachment->endGesture();
        }
        else if ((part == Part::tile || part == Part::connector) && e.getNumberOfClicks() < 2 && ! e.mouseWasDraggedSinceMouseDown())
        {
            strip.selectSource (index);
            if (strip.getPopover() != nullptr && strip.getPopover()->getSourceIndex() == index)
                strip.closePopover();
            else
                strip.showPopover (index);
        }
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (isMacro() && attachment != nullptr && partAt (e.position) == Part::tile)
        {
            attachment->beginGesture();
            attachment->setValueAsPartOfGesture (macroParam->convertFrom0to1 (macroParam->getDefaultValue()));
            attachment->endGesture();
        }
    }

    void showMenu()
    {
        auto menu = strip.createSourceMenu (index);
        ::st::showMenu (menu, this, {}, true);
    }

private:
    enum class Part { none, connector, tile, name };

    ModulationStrip& strip;
    mod::ModMatrix& matrix;
    const int index;
    const mod::SourceId id;
    const mod::SourceInfo info;
    const juce::Colour colour;

    Part hoverPart = Part::none, pressPart = Part::none;
    bool dragStarted = false, macroDragging = false;
    float lastY = 0.0f;

    // live display state
    std::array<float, 56> history;
    int writePos = 0;
    double sampleClock = 0.0;
    static constexpr double samplePeriod = 1.0 / 30.0;
    float lastValue = -1.0f, lastPhase = -1.0f, lastPhaseOffset = 0.0f, haloValue = 0.0f;
    int lastShape = -1;
    bool lastBipolar = true;

    // macro
    juce::RangedAudioParameter* macroParam = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    float macroValue = 0.0f;

    bool usesTrace() const
    {
        if (id.kind == mod::SourceKind::envelope) return true;
        if (id.kind == mod::SourceKind::lfo)
            return mod::isRandomShape ((mod::LfoShape) juce::jlimit (0, mod::numLfoShapes - 1, juce::roundToInt (rawParam (matrix, id, mod::ids::shape))));
        return false;
    }

    juce::Rectangle<float> getConnectorArea() const
    {
        const float d = ModulationStrip::connectorRadius * 4.4f;
        return juce::Rectangle<float> (d, d).withCentre (getConnectorCentre());
    }

    Part partAt (juce::Point<float> p) const
    {
        if (p.getDistanceFrom (getConnectorCentre()) <= ModulationStrip::connectorRadius + 5.0f) return Part::connector;
        if (getTileArea().expanded (1.0f).contains (p)) return Part::tile;
        if (getNameArea().reduced (8.0f, 0.0f).contains (p)) return Part::name;
        return Part::none;
    }

    void setHover (Part p)
    {
        if (p == hoverPart) return;
        hoverPart = p;
        setMouseCursor (p == Part::connector ? juce::MouseCursor::DraggingHandCursor
                      : p == Part::tile && isMacro() ? juce::MouseCursor::UpDownResizeCursor
                      : p == Part::none ? juce::MouseCursor::NormalCursor
                                        : juce::MouseCursor::PointingHandCursor);
        repaint();
    }

    void startModulationDrag (const juce::MouseEvent& e)
    {
        auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this);
        if (container == nullptr)
            return;

        dragStarted = true;
        container->startDragging (Knob::makeModulationDragDescription (index, colour), this, makeDragImage(), false, nullptr, &e.source);
        strip.setDragging (index);
        repaint();
    }

    juce::ScaledImage makeDragImage() const
    {
        const float scale = juce::jmax (0.25f, juce::Component::getApproximateScaleFactorForComponent (const_cast<SourceColumn*> (this)));
        constexpr float density = 2.0f;
        const float size = ModulationStrip::connectorRadius * 4.0f;
        const int px = juce::roundToInt (size * scale * density) + 1;
        juce::Image img (juce::Image::ARGB, px, px, true);
        {
            juce::Graphics g (img);
            g.addTransform (juce::AffineTransform::scale (scale * density));
            paint::connectorDot (g, { size * 0.5f, size * 0.5f }, ModulationStrip::connectorRadius, colour, true);
        }
        return juce::ScaledImage (img, (double) density);
    }

    //==============================================================================
    juce::Rectangle<float> plotArea (juce::Rectangle<float> tile) const { return tile.reduced (9.0f, 13.0f); }

    void paintLfo (juce::Graphics& g, juce::Rectangle<float> tile)
    {
        const auto& t = Theme::get();
        const auto plot = plotArea (tile);
        const auto shape = (mod::LfoShape) juce::jlimit (0, mod::numLfoShapes - 1, juce::roundToInt (rawParam (matrix, id, mod::ids::shape)));
        const float offset = rawParam (matrix, id, mod::ids::phase) / 360.0f;
        const bool bipolar = matrix.isSourceBipolar (index);
        auto yFor = [&plot] (float v01) { return plot.getBottom() - juce::jlimit (0.0f, 1.0f, v01) * plot.getHeight(); };

        // faint centre line
        g.setColour (t.strokeSoft.withAlpha (0.8f));
        g.fillRect (plot.getX(), (float) juce::roundToInt (bipolar ? yFor (0.5f) : plot.getBottom()) - 0.5f, plot.getWidth(), 1.0f);

        juce::Path curve;
        constexpr int n = 96;
        for (int i = 0; i <= n; ++i)
        {
            const float xn = (float) i / (float) n;
            float p = xn + offset;
            p -= std::floor (p);
            const float v = 0.5f * (mod::evaluateLfoShape (shape, juce::jmin (p, 0.99999f)) + 1.0f);
            const juce::Point<float> pt (plot.getX() + xn * plot.getWidth(), yFor (v));
            if (i == 0) curve.startNewSubPath (pt); else curve.lineTo (pt);
        }

        // whisper of fill under the curve
        {
            juce::Path fill (curve);
            fill.lineTo (plot.getRight(), plot.getBottom());
            fill.lineTo (plot.getX(), plot.getBottom());
            fill.closeSubPath();
            g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.10f), 0.0f, plot.getY(),
                                                     colour.withAlpha (0.0f), 0.0f, plot.getBottom(), false));
            g.fillPath (fill);
        }

        paint::glowStroke (g, curve, colour, 1.5f, 3.0f);

        // playhead
        float raw = matrix.getLfoPhase (id.index) - offset;
        raw -= std::floor (raw);
        const juce::Point<float> head (plot.getX() + raw * plot.getWidth(), yFor (matrix.getSourceValueNormalised (index)));
        {
            juce::ColourGradient line (colour.withAlpha (0.0f), head.x, tile.getY() + 6.0f, colour.withAlpha (0.0f), head.x, tile.getBottom() - 6.0f, false);
            line.addColour (0.5, colour.withAlpha (0.28f));
            g.setGradientFill (line);
            g.fillRect (head.x - 0.5f, tile.getY() + 6.0f, 1.0f, tile.getHeight() - 12.0f);
        }
        drawHead (g, head);
    }

    void paintTrace (juce::Graphics& g, juce::Rectangle<float> tile)
    {
        const auto& t = Theme::get();
        const auto plot = plotArea (tile);
        auto yFor = [&plot] (float v01) { return plot.getBottom() - juce::jlimit (0.0f, 1.0f, v01) * plot.getHeight(); };

        g.setColour (t.strokeSoft.withAlpha (0.8f));
        g.fillRect (plot.getX(), plot.getBottom() - 0.5f, plot.getWidth(), 1.0f);

        juce::Path curve;
        const int n = (int) history.size();
        for (int i = 0; i < n; ++i)
        {
            const float v = history[(size_t) ((writePos + i) % n)];
            const juce::Point<float> pt (plot.getX() + plot.getWidth() * (float) i / (float) (n - 1), yFor (v));
            if (i == 0) curve.startNewSubPath (pt); else curve.lineTo (pt);
        }
        const auto head = curve.getCurrentPosition();

        juce::Path fill (curve);
        fill.lineTo (plot.getRight(), plot.getBottom());
        fill.lineTo (plot.getX(), plot.getBottom());
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.22f), 0.0f, plot.getY(),
                                                 colour.withAlpha (0.02f), 0.0f, plot.getBottom(), false));
        g.fillPath (fill);

        paint::glowStroke (g, curve, colour, 1.5f, 3.0f);
        drawHead (g, head);
    }

    void paintMacro (juce::Graphics& g, juce::Rectangle<float> tile)
    {
        const auto& t = Theme::get();
        auto inner = tile.reduced (3.0f);
        inner.removeFromTop (ModulationStrip::connectorRadius + 4.0f);
        const float level = inner.getBottom() - macroValue * inner.getHeight();
        auto fill = inner.withTop (level);

        if (fill.getHeight() > 0.0f)
        {
            juce::ColourGradient grad (colour.withAlpha (0.55f), 0.0f, level, colour.withAlpha (0.12f), 0.0f, inner.getBottom() + 8.0f, false);
            grad.addColour (0.15, colour.withAlpha (0.36f));
            g.setGradientFill (grad);
            g.fillRoundedRectangle (fill.withTrimmedBottom (-4.0f), tileRadius - 3.0f);

            // soft bloom above the level line
            g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.20f), 0.0f, level, colour.withAlpha (0.0f), 0.0f, level - 14.0f, false));
            g.fillRect (inner.withBottom (level).withTop (level - 14.0f));

            g.setColour (colour.brighter (0.35f).withAlpha (0.95f));
            g.fillRect (inner.getX() + 1.0f, level - 0.75f, inner.getWidth() - 2.0f, 1.5f);
        }

        if (hoverPart == Part::tile || macroDragging)
        {
            auto label = tile.withTrimmedTop (ModulationStrip::connectorRadius + 6.0f).removeFromTop (16.0f);
            g.setColour (t.text.withAlpha (0.9f));
            g.setFont (fonts::semibold (10.5f));
            g.drawText (macroParam != nullptr ? macroParam->getCurrentValueAsText() : juce::String(), label, juce::Justification::centred, false);
        }
    }

    void drawHead (juce::Graphics& g, juce::Point<float> head) const
    {
        g.setColour (colour.withAlpha (0.25f));
        g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre (head));
        g.setColour (Theme::get().text);
        g.fillEllipse (juce::Rectangle<float> (4.5f, 4.5f).withCentre (head));
    }
};

//==============================================================================
class AddTile : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit AddTile (ModulationStrip& s) : strip (s)
    {
        setTooltip ("Add a modulation source");
        setRepaintsOnMouseActivity (true);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    Animated xPos;
    static constexpr float width = 48.0f;

    juce::Rectangle<float> getTileArea() const
    {
        const float cy = verticalOffset (*this) + tileTop + tileH * 0.5f;
        return juce::Rectangle<float> (addSize, addSize).withCentre ({ (float) getWidth() * 0.5f, cy });
    }

    void setAvailable (bool b) { if (b != available) { available = b; repaint(); } }

    bool hitTest (int x, int y) override { return getTileArea().expanded (2.0f).contains ((float) x, (float) y); }

    void paint (juce::Graphics& g) override
    {
        const auto& t = Theme::get();
        const bool over = isMouseOver() && available;
        const auto r = getTileArea();
        paintTileFrame (g, r, 6.0f, over ? t.bg4.brighter (0.2f) : t.stroke);
        icons::draw (g, icons::Icon::plus, r.withSizeKeepingCentre (17.0f, 17.0f),
                     ! available ? t.textFaint.withAlpha (0.5f) : (over ? t.text : t.textDim), 1.8f);
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        auto menu = strip.createAddMenu();
        ::st::showMenu (menu, this, {}, true);
    }

private:
    ModulationStrip& strip;
    bool available = true;
};

} // namespace strip_detail

using namespace strip_detail;

//==============================================================================
struct ModulationPopover::OutsideClickWatcher : public juce::MouseListener
{
    explicit OutsideClickWatcher (ModulationPopover& p) : owner (p) {}

    void mouseDown (const juce::MouseEvent& e) override
    {
        auto* c = e.eventComponent;
        if (c == nullptr || c == &owner || owner.isParentOf (c))
            return;
        if (c == &owner.strip || owner.strip.isParentOf (c))
            return; // the strip toggles popovers itself

        juce::MessageManager::callAsync ([s = juce::Component::SafePointer<ModulationStrip> (&owner.strip),
                                          p = juce::Component::SafePointer<ModulationPopover> (&owner)]
        {
            if (s != nullptr && p != nullptr && s->getPopover() == p.getComponent())
                s->closePopover();
        });
    }

    ModulationPopover& owner;
};

ModulationPopover::ModulationPopover (ModulationStrip& s, int flat)
    : strip (s), index (flat), colour (s.getSourceColour (flat))
{
    content = createModSourceEditor (strip.getMatrix(), flat);
    if (content != nullptr)
        addAndMakeVisible (*content);

    closeButton.onClick = [this]
    {
        juce::MessageManager::callAsync ([st = juce::Component::SafePointer<ModulationStrip> (&strip)]
        {
            if (st != nullptr) st->closePopover();
        });
    };
    addAndMakeVisible (closeButton);

    const int cw = content != nullptr ? content->getWidth() : 300;
    const int ch = content != nullptr ? content->getHeight() : 160;
    setSize (cw + 2 * pad + 2 * shadow, ch + header + 2 * pad + 2 * shadow);
    setWantsKeyboardFocus (true);

    watcher = std::make_unique<OutsideClickWatcher> (*this);
    appear.snap (0.0f);
    appear.setTarget (1.0f);
    setAlpha (0.0f);
    clock = std::make_unique<FrameClock> (*this, [this] (double dt)
    {
        if (! juce::exactlyEqual (appear.current, appear.target))
        {
            appear.tick (dt, 16.0f);
            setAlpha (juce::jlimit (0.0f, 1.0f, appear.current));
            setTopLeftPosition (targetBounds.getX(), targetBounds.getY() + juce::roundToInt ((1.0f - appear.current) * 8.0f));
        }
    });
}

ModulationPopover::~ModulationPopover()
{
    clock.reset();
    if (auto* r = watchedRoot.getComponent())
        r->removeMouseListener (watcher.get());
}

juce::Rectangle<float> ModulationPopover::getPanelArea() const
{
    return getLocalBounds().toFloat().reduced ((float) shadow);
}

void ModulationPopover::place (juce::Rectangle<float> tile, float connectorTop)
{
    auto* parent = getParentComponent();
    if (parent == nullptr)
        return;

    if (watchedRoot == nullptr)
    {
        watchedRoot = parent;
        parent->addMouseListener (watcher.get(), true);
    }

    const auto pb = parent->getLocalBounds();
    const int pw = getWidth() - 2 * shadow, ph = getHeight() - 2 * shadow;
    constexpr int margin = 8;

    int x = juce::roundToInt (tile.getCentreX()) - pw / 2;
    x = juce::jlimit (pb.getX() + margin, juce::jmax (pb.getX() + margin, pb.getRight() - margin - pw), x);
    int y = juce::roundToInt (connectorTop) - 12 - ph;
    y = juce::jmax (pb.getY() + margin, y);

    targetBounds = juce::Rectangle<int> (x - shadow, y - shadow, getWidth(), getHeight());
    setBounds (targetBounds.translated (0, juce::roundToInt ((1.0f - appear.current) * 8.0f)));
}

bool ModulationPopover::hitTest (int x, int y)
{
    return getPanelArea().contains ((float) x, (float) y);
}

void ModulationPopover::resized()
{
    auto r = getPanelArea().toNearestInt().reduced (pad);
    auto head = r.removeFromTop (header);
    closeButton.setBounds (head.removeFromRight (22).withSizeKeepingCentre (22, 22).translated (4, -4));
    if (content != nullptr)
        content->setBounds (r.getX(), r.getY(), content->getWidth(), content->getHeight());
}

void ModulationPopover::paint (juce::Graphics& g)
{
    const auto& t = Theme::get();
    const auto panel = getPanelArea();
    paint::glass (g, panel, t.radiusLarge + 1.0f, {}, 0.985f);

    // faint source-coloured light along the top edge
    {
        juce::Graphics::ScopedSaveState state (g);
        juce::Path clip;
        clip.addRoundedRectangle (panel, t.radiusLarge + 1.0f);
        g.reduceClipRegion (clip);
        juce::ColourGradient top (colour.withAlpha (0.10f), panel.getCentreX(), panel.getY(),
                                  colour.withAlpha (0.0f), panel.getCentreX(), panel.getY() + 54.0f, false);
        g.setGradientFill (top);
        g.fillRect (panel.withHeight (54.0f));
    }

    auto head = panel.reduced ((float) pad).removeFromTop ((float) header).translated (0.0f, -4.0f);
    const auto dot = head.removeFromLeft (14.0f).withSizeKeepingCentre (10.0f, 10.0f);
    g.setColour (colour.withAlpha (0.25f));
    g.fillEllipse (dot.expanded (3.0f));
    g.setColour (colour);
    g.fillEllipse (dot);
    head.removeFromLeft (8.0f);

    const auto name = strip.getMatrix().getSourceName (strip.getMatrix().getSourceId (index));
    const auto font = fonts::semibold (14.0f);
    g.setColour (t.text);
    g.setFont (font);
    g.drawText (name, head, juce::Justification::centredLeft, false);

    const auto kind = strip.getMatrix().getSourceId (index).kind;
    head.removeFromLeft (fonts::stringWidth (font, name) + 10.0f);
    paint::caption (g, kind == mod::SourceKind::lfo ? "LFO" : kind == mod::SourceKind::envelope ? "Envelope follower" : "Macro",
                    head, t.textFaint, juce::Justification::centredLeft, 9.5f);

    g.setColour (t.strokeSoft);
    g.fillRect (panel.getX() + (float) pad, panel.getY() + (float) pad + (float) header - 6.0f, panel.getWidth() - 2.0f * (float) pad, 1.0f);
}

bool ModulationPopover::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey)
    {
        juce::MessageManager::callAsync ([st = juce::Component::SafePointer<ModulationStrip> (&strip)]
        {
            if (st != nullptr) st->closePopover();
        });
        return true;
    }
    return false;
}

//==============================================================================
ModulationStrip::ModulationStrip (mod::ModMatrix& m) : matrix (m)
{
    if (matrix.getSourceColour (0).isTransparent())
    {
        juce::Array<juce::Colour> palette;
        for (auto c : Theme::get().series)
            palette.add (c);
        matrix.setSourcePalette (palette);
    }

    addTile = std::make_unique<AddTile> (*this);
    addAndMakeVisible (*addTile);

    matrix.addChangeListener (this);
    syncColumns();
    clock = std::make_unique<FrameClock> (*this, [this] (double dt) { tick (dt); });
}

ModulationStrip::~ModulationStrip()
{
    *alive = false;
    clock.reset();
    matrix.removeChangeListener (this);
    popover.reset();
}

juce::Colour ModulationStrip::getSourceColour (int i) const
{
    const auto c = matrix.getSourceColour (i);
    return c.isTransparent() ? Theme::get().seriesColour (matrix.getSourceColourIndex (matrix.getSourceId (i))) : c;
}

SourceColumn* ModulationStrip::findColumn (int flat) const
{
    for (auto* c : columns)
        if (c->getIndex() == flat)
            return c;
    return nullptr;
}

std::optional<juce::Point<float>> ModulationStrip::getConnectorPosition (int flat) const
{
    if (auto* c = findColumn (flat))
        return c->getConnectorCentre() + c->getPosition().toFloat();
    return std::nullopt;
}

void ModulationStrip::selectSource (int flat)
{
    if (flat == selected) return;
    selected = flat;
    for (auto* c : columns) c->repaint();
}

void ModulationStrip::setDragging (int flat)
{
    if (flat == dragging) return;
    dragging = flat;
    for (auto* c : columns) c->repaint();
    if (onSourceDrag) onSourceDrag (flat);
}

void ModulationStrip::showSource (int flat)
{
    const auto id = matrix.getSourceId (flat);
    if (! matrix.isValidSource (id)) return;
    matrix.beginUndoTransaction ("Add Modulation Source");
    matrix.setSourceShown (id, true);
    syncColumns();
}

void ModulationStrip::removeSource (int flat)
{
    const auto id = matrix.getSourceId (flat);
    if (! matrix.isValidSource (id)) return;
    if (popover != nullptr && popover->getSourceIndex() == flat)
        closePopover();
    matrix.beginUndoTransaction ("Remove Modulation Source");
    matrix.setSourceShown (id, false);
    matrix.removeAllRoutingsForSource (id, false);
    syncColumns();
}

juce::Component* ModulationStrip::findPopoverParent()
{
    juce::Component* c = this;
    while (auto* p = c->getParentComponent())
    {
        if (dynamic_cast<juce::AudioProcessorEditor*> (p) != nullptr)
            break;
        c = p;
    }
    if (c == this)
        c = getParentComponent();
    return c;
}

void ModulationStrip::showPopover (int flat)
{
    closePopover();
    auto* col = findColumn (flat);
    auto* parent = findPopoverParent();
    if (col == nullptr || parent == nullptr)
        return;

    popover = std::make_unique<ModulationPopover> (*this, flat);
    parent->addAndMakeVisible (*popover);
    const auto tile = parent->getLocalArea (col, col->getTileArea());
    const auto connTop = parent->getLocalPoint (col, col->getConnectorCentre()).y - connectorRadius;
    popover->place (tile, connTop);
    if (popover->isShowing())
        popover->grabKeyboardFocus();

    for (auto* c : columns) c->repaint();
}

void ModulationStrip::closePopover()
{
    if (popover == nullptr) return;
    popover.reset();
    for (auto* c : columns) c->repaint();
}

juce::PopupMenu ModulationStrip::createAddMenu()
{
    juce::PopupMenu menu;
    const auto shown = getShownSources();
    std::weak_ptr<bool> weak = alive;

    auto section = [&] (mod::SourceKind kind, const char* title)
    {
        bool header = false;
        for (int i = 0; i < matrix.getNumSources(); ++i)
        {
            const auto id = matrix.getSourceId (i);
            if (id.kind != kind || std::find (shown.begin(), shown.end(), i) != shown.end())
                continue;
            if (! header)
            {
                menu.addSectionHeader (title);
                header = true;
            }
            juce::PopupMenu::Item item (matrix.getSourceName (id));
            item.colour = getSourceColour (i);
            item.action = [weak, this, i] { if (auto a = weak.lock(); a != nullptr && *a) showSource (i); };
            menu.addItem (std::move (item));
        }
    };

    section (mod::SourceKind::lfo, "LFO");
    section (mod::SourceKind::envelope, "Envelope follower");
    section (mod::SourceKind::macro, "Macro");

    if (menu.getNumItems() == 0)
        menu.addItem ("All sources are shown", false, false, nullptr);
    return menu;
}

juce::PopupMenu ModulationStrip::createSourceMenu (int flat)
{
    juce::PopupMenu menu;
    const auto id = matrix.getSourceId (flat);
    if (! matrix.isValidSource (id))
        return menu;

    std::weak_ptr<bool> weak = alive;
    auto guarded = [weak, this] (std::function<void (ModulationStrip&)> fn)
    {
        return [weak, this, fn] { if (auto a = weak.lock(); a != nullptr && *a) fn (*this); };
    };

    menu.addItem (juce::String ("Edit") + juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xa6")),
                  guarded ([flat] (ModulationStrip& s) { s.showPopover (flat); }));

    if (id.kind == mod::SourceKind::lfo)
    {
        juce::PopupMenu shapes;
        const auto names = mod::getLfoShapeNames();
        const int current = juce::roundToInt (rawParam (matrix, id, mod::ids::shape));
        auto* p = findParam (matrix, id, mod::ids::shape);
        for (int i = 0; i < names.size(); ++i)
        {
            juce::PopupMenu::Item item (names[i]);
            item.isTicked = i == current;
            item.action = [p, i] { setParamPlain (p, (float) i); };
            shapes.addItem (std::move (item));
        }
        menu.addSubMenu ("Shape", shapes, p != nullptr);
    }
    else if (id.kind == mod::SourceKind::macro)
    {
        auto* p = findParam (matrix, id, "");
        menu.addItem ("Reset value", p != nullptr, false, [p] { if (p != nullptr) setParamPlain (p, p->convertFrom0to1 (p->getDefaultValue())); });
    }

    menu.addSeparator();
    const auto n = (int) matrix.getRoutingsForSource (id).size();
    menu.addItem (n > 0 ? "Remove routings (" + juce::String (n) + ")" : juce::String ("Remove routings"), n > 0, false,
                  [m = &matrix, id] { m->removeAllRoutingsForSource (id); });
    menu.addItem ("Remove " + matrix.getSourceName (id), guarded ([flat] (ModulationStrip& s) { s.removeSource (flat); }));
    return menu;
}

void ModulationStrip::syncColumns()
{
    const auto shown = getShownSources();

    // remove columns that are no longer shown
    for (int i = columns.size(); --i >= 0;)
    {
        const int idx = columns[i]->getIndex();
        if (std::find (shown.begin(), shown.end(), idx) == shown.end())
        {
            if (popover != nullptr && popover->getSourceIndex() == idx) closePopover();
            if (selected == idx) selected = -1;
            columns.remove (i);
        }
    }

    // add new ones (faded in), then sort into display order
    for (int idx : shown)
    {
        if (findColumn (idx) == nullptr)
        {
            auto* c = columns.add (new SourceColumn (*this, idx));
            c->alpha.snap (firstLayout ? 1.0f : 0.0f);
            c->alpha.setTarget (1.0f);
            c->xPos.snap (-1.0e6f); // marks "not placed yet"
            c->setAlpha (c->alpha.current);
            addAndMakeVisible (c);
        }
    }

    std::vector<SourceColumn*> ordered;
    for (int idx : shown)
        ordered.push_back (findColumn (idx));
    for (int i = 0; i < (int) ordered.size(); ++i)
        columns.set (i, ordered[(size_t) i], false);

    addTile->setAvailable ((int) shown.size() < matrix.getNumSources());
    layoutColumns (! firstLayout);
    firstLayout = false;
}

void ModulationStrip::layoutColumns (bool animate)
{
    const int n = columns.size();
    const float w = (float) getWidth();
    float p = pitch, ap = addPitch;
    const float total = (n > 0 ? (float) (n - 1) * p + ap : 0.0f) + columnWidth;
    const float avail = w - 24.0f;
    if (total > avail && total > 0.0f)
    {
        const float k = juce::jmax (0.5f, (avail - columnWidth) / (total - columnWidth));
        p *= k;
        ap *= k;
    }

    const float rowW = n > 0 ? (float) (n - 1) * p + ap : 0.0f;
    const float first = w * 0.5f - rowW * 0.5f;

    for (int i = 0; i < n; ++i)
    {
        auto* c = columns[i];
        const float target = first + (float) i * p - columnWidth * 0.5f;
        c->xPos.setTarget (target);
        if (! animate || c->xPos.current < -1.0e5f)
            c->xPos.snap (target);
        c->setBounds (juce::roundToInt (c->xPos.current), 0, (int) columnWidth, getHeight());
    }

    const float addX = (n > 0 ? first + rowW : w * 0.5f) - AddTile::width * 0.5f;
    addTile->xPos.setTarget (addX);
    if (! animate)
        addTile->xPos.snap (addX);
    addTile->setBounds (juce::roundToInt (addTile->xPos.current), 0, (int) AddTile::width, getHeight());
}

void ModulationStrip::resized()
{
    layoutColumns (false);
}

void ModulationStrip::tick (double dt)
{
    for (auto* c : columns)
    {
        if (c->xPos.tick (dt, 16.0f))
            c->setTopLeftPosition (juce::roundToInt (c->xPos.current), 0);
        if (c->alpha.tick (dt, 10.0f) || ! juce::exactlyEqual (c->getAlpha(), c->alpha.current))
            c->setAlpha (c->alpha.current);
        c->tick (dt);
    }

    if (addTile->xPos.tick (dt, 16.0f))
        addTile->setTopLeftPosition (juce::roundToInt (addTile->xPos.current), 0);

    // Safety net: a drag can end without the column seeing mouseUp (e.g. dropped outside the window).
    if (dragging >= 0)
        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this); container == nullptr || ! container->isDragAndDropActive())
            setDragging (-1);
}

void ModulationStrip::changeListenerCallback (juce::ChangeBroadcaster*)
{
    syncColumns();
    for (auto* c : columns) c->repaint();
}

} // namespace st
