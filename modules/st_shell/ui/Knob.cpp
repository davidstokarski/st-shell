namespace st
{

//==============================================================================
ParameterControl::ParameterControl (juce::RangedAudioParameter& p, juce::UndoManager* um)
    : param (p), undo (um),
      attachment (p, [this] (float plain)
      {
          normValue = param.convertTo0to1 (plain);
          valueChanged();
      }, um)
{
    attachment.sendInitialUpdate();
    setRepaintsOnMouseActivity (false);
}

void ParameterControl::beginEdit()
{
    if (! gestureActive)
    {
        attachment.beginGesture();
        gestureActive = true;
    }
}

void ParameterControl::setNormalisedDuringEdit (float n)
{
    n = juce::jlimit (0.0f, 1.0f, n);
    attachment.setValueAsPartOfGesture (param.convertFrom0to1 (n));
}

void ParameterControl::endEdit()
{
    if (gestureActive)
    {
        attachment.endGesture();
        gestureActive = false;
    }
}

void ParameterControl::setNormalisedAsGesture (float n)
{
    attachment.setValueAsCompleteGesture (param.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, n)));
}

void ParameterControl::resetToDefault()
{
    setNormalisedAsGesture (param.getDefaultValue());
}

void ParameterControl::showContextMenu (std::function<void (juce::PopupMenu&)> addOwnItems)
{
    juce::PopupMenu menu;
    menu.addSectionHeader (param.getName (64));

    if (addOwnItems) addOwnItems (menu);
    if (extendContextMenu) extendContextMenu (menu);

    menu.addItem ("Reset to default", [safe = juce::Component::SafePointer<ParameterControl> (this)]
    {
        if (safe != nullptr) safe->resetToDefault();
    });

    if (auto* editor = findParentComponentOfClass<juce::AudioProcessorEditor>())
    {
        if (auto* ctx = editor->getHostContext())
        {
            if (auto hostMenu = ctx->getContextMenuForParameter (&param))
            {
                auto hm = hostMenu->getEquivalentPopupMenu();
                if (hm.getNumItems() > 0)
                {
                    menu.addSeparator();
                    menu.addSubMenu ("Host", hm);
                }
            }
        }
    }

    ::st::showMenu (menu, this, {}, true);
}

//==============================================================================
Knob::Knob (juce::RangedAudioParameter& p, juce::UndoManager* um, const juce::String& l)
    : ParameterControl (p, um), label (l.isNotEmpty() ? l : p.getName (32)), colour (Theme::get().accent)
{
    setWantsKeyboardFocus (false);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

Knob::~Knob()
{
    stopTimer();
    endEdit();
}

void Knob::setModulationSource (std::function<ModulationDisplay()> fn)
{
    modSource = std::move (fn);
    if (modSource) startTimerHz (30);
    else { stopTimer(); modState = {}; repaint(); }
}

void Knob::setModulationDropHint (bool show, juce::Colour c)
{
    if (show == modDropHint && (c == modDropColour || ! show)) return;
    modDropHint = show;
    if (show) modDropColour = c;
    repaint();
}

juce::String Knob::makeModulationDragDescription (int sourceIndex, juce::Colour c)
{
    return "stmod:" + juce::String (sourceIndex) + ":" + c.toString();
}

bool Knob::parseModulationDrag (const juce::var& desc, int& sourceIndex, juce::Colour& c)
{
    if (! desc.isString()) return false;
    const auto s = desc.toString();
    if (! s.startsWith ("stmod:")) return false;

    juce::StringArray parts;
    parts.addTokens (s.fromFirstOccurrenceOf ("stmod:", false, false), ":", {});
    if (parts.isEmpty() || ! parts[0].containsOnly ("0123456789") || parts[0].isEmpty()) return false;

    sourceIndex = parts[0].getIntValue();
    c = parts.size() > 1 ? juce::Colour::fromString (parts[1]) : juce::Colour();
    return true;
}

bool Knob::isInterestedInDragSource (const SourceDetails& d)
{
    int index = 0;
    juce::Colour c;
    return onModulationDropped != nullptr && isEnabled() && parseModulationDrag (d.description, index, c);
}

void Knob::itemDragEnter (const SourceDetails& d)
{
    int index = 0;
    juce::Colour c;
    if (parseModulationDrag (d.description, index, c))
        modDropColour = c;
    modDropHover = true;
    repaint();
}

void Knob::itemDragExit (const SourceDetails&)
{
    modDropHover = false;
    repaint();
}

void Knob::itemDropped (const SourceDetails& d)
{
    modDropHover = false;
    repaint();

    int index = 0;
    juce::Colour c;
    if (onModulationDropped && parseModulationDrag (d.description, index, c))
        onModulationDropped (index);
}

void Knob::timerCallback()
{
    if (! modSource) return;
    auto m = modSource();
    if (m.active != modState.active || std::abs (m.current - modState.current) > 0.001f
        || std::abs (m.lo - modState.lo) > 0.001f || std::abs (m.hi - modState.hi) > 0.001f)
    {
        modState = m;
        repaint();
    }
}

juce::String Knob::getValueText() const
{
    const float plain = param.convertFrom0to1 (normValue);
    if (valueToText) return valueToText (plain);
    return param.getText (normValue, 32) + (param.getLabel().isNotEmpty() && ! param.getText (normValue, 32).endsWith (param.getLabel())
                                              ? " " + param.getLabel() : juce::String());
}

float Knob::getCaptionHeight() const noexcept
{
    return size == Size::mini ? 0.0f : (size == Size::small ? 16.0f : 20.0f);
}

juce::Rectangle<float> Knob::getDialBounds() const
{
    auto r = getLocalBounds().toFloat();
    r.removeFromBottom (getCaptionHeight());
    const float d = juce::jmin (r.getWidth(), r.getHeight());
    return r.withSizeKeepingCentre (d, d);
}

juce::Point<float> Knob::getModulationAnchor() const
{
    auto textArea = getLocalBounds().toFloat().removeFromBottom (juce::jmax (12.0f, getCaptionHeight()));
    const float w = fonts::stringWidth (fonts::caption (size == Size::small ? 10.0f : 11.5f), label.toUpperCase());
    return { textArea.getCentreX() + w * 0.5f + 8.0f, textArea.getCentreY() };
}

Knob::Geometry Knob::geometry() const
{
    auto dial = getDialBounds();
    Geometry gm;
    gm.centre = dial.getCentre();
    gm.R = dial.getWidth() * 0.5f;
    gm.modW = juce::jmax (2.0f, gm.R * 0.07f);
    gm.outerR = gm.R - gm.modW - 1.5f;
    gm.rimW = look == Look::ringed ? gm.outerR * 0.17f : 0.0f;
    gm.discR = gm.outerR - gm.rimW - (look == Look::ringed ? 1.0f : 0.0f);
    return gm;
}

Knob::Zone Knob::zoneAt (juce::Point<float> p) const
{
    const auto gm = geometry();
    const float d = p.getDistanceFrom (gm.centre);
    if (modSource && modState.active && onModulationDepthDrag && d >= gm.outerR - 0.5f && d <= gm.R + 4.0f)
        return Zone::modDepth;
    if (look == Look::ringed && rimParam != nullptr && d >= gm.discR - 1.0f && d < gm.outerR + (modState.active ? -0.5f : 4.0f))
        return Zone::rim;
    return Zone::value;
}

void Knob::setRimParameter (juce::RangedAudioParameter* p, const juce::String& label, bool showLR)
{
    rimAttachment.reset();
    rimParam = p;
    rimLabel = label;
    rimLR = showLR;
    if (p != nullptr)
    {
        rimAttachment = std::make_unique<juce::ParameterAttachment> (*p, [this] (float plain)
        {
            rimNorm = rimParam->convertTo0to1 (plain);
            repaint();
        }, undo);
        rimAttachment->sendInitialUpdate();
    }
    repaint();
}

void Knob::mouseMove (const juce::MouseEvent& e)
{
    const auto z = zoneAt (e.position);
    if (z != hoverZone)
    {
        hoverZone = z;
        setMouseCursor (z == Zone::value ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::UpDownLeftRightResizeCursor);
        repaint();
    }
}

void Knob::paint (juce::Graphics& g)
{
    const auto& t = Theme::get();
    const bool enabled = isEnabled();
    const auto gm = geometry();
    const auto centre = gm.centre;
    const float R = gm.R;

    constexpr float startA = -2.45f, endA = 2.45f;   // radians from 12 o'clock
    const float valueA = startA + (endA - startA) * normValue;

    const float modW = gm.modW;
    const float outerR = gm.outerR;
    const bool ringed = look == Look::ringed;
    const float rimW = gm.rimW;
    const float discR = gm.discR;
    const bool rimActive = rimParam != nullptr && ((hovering && hoverZone == Zone::rim) || (dragging && dragZone == Zone::rim));
    const bool modActive = (hovering && hoverZone == Zone::modDepth) || (dragging && dragZone == Zone::modDepth);

    // Modulation-drop hint
    if (modDropHint || modDropHover)
    {
        const auto c = modDropColour.isTransparent() ? t.modulation : modDropColour;
        g.setColour (c.withAlpha (modDropHover ? 0.35f : 0.12f));
        g.fillEllipse (juce::Rectangle<float> ((R + 2.0f) * 2.0f, (R + 2.0f) * 2.0f).withCentre (centre));
    }

    // Red rim (big knobs): a thick ring with a light index at 12 o'clock and the value position marked white.
    if (ringed)
    {
        const float rr = outerR - rimW * 0.5f;
        juce::Path rim;
        rim.addCentredArc (centre.x, centre.y, rr, rr, 0.0f, 0.0f, juce::MathConstants<float>::twoPi, true);

        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (juce::Rectangle<float> (outerR * 2.0f + 2.0f, outerR * 2.0f + 2.0f).withCentre (centre.translated (0.0f, 1.5f)));

        const auto rimCol = (enabled ? colour : t.textFaint).brighter (rimActive ? 0.3f : 0.0f);
        g.setGradientFill (juce::ColourGradient (rimCol.brighter (0.25f), centre.x, centre.y - outerR,
                                                 rimCol.darker (0.45f), centre.x, centre.y + outerR, false));
        g.strokePath (rim, juce::PathStrokeType (rimW));

        // inner + outer edges of the rim
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawEllipse (juce::Rectangle<float> (outerR * 2.0f, outerR * 2.0f).withCentre (centre), 0.8f);

        // scale ticks: start, centre, end
        for (float a : { startA, 0.0f, endA })
        {
            const auto p0 = centre.getPointOnCircumference (outerR - rimW + 1.5f, a);
            const auto p1 = centre.getPointOnCircumference (outerR - 1.0f, a);
            g.setColour (juce::Colours::white.withAlpha (a == 0.0f ? 0.95f : 0.6f));
            g.drawLine ({ p0, p1 }, a == 0.0f ? 2.0f : 1.2f);
        }

        // white "lit" segment from the start of the travel to the current value
        juce::Path lit;
        lit.addCentredArc (centre.x, centre.y, rr, rr, 0.0f, startA, valueA, true);
        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.strokePath (lit, juce::PathStrokeType (rimW * 0.55f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

        // Rim control: L / R markers spread around the value
        if (rimParam != nullptr)
        {
            const float s = (rimNorm - 0.5f) * 2.0f;                    // -1..1
            const float spread = s * 0.55f;                              // radians
            const float aL = juce::jlimit (startA, endA, valueA - spread);
            const float aR = juce::jlimit (startA, endA, valueA + spread);

            if (std::abs (s) > 0.01f)
            {
                juce::Path span;
                span.addCentredArc (centre.x, centre.y, rr, rr, 0.0f, juce::jmin (aL, aR), juce::jmax (aL, aR), true);
                g.setColour (juce::Colours::white.withAlpha (0.55f));
                g.strokePath (span, juce::PathStrokeType (rimW * 0.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }

            for (int k = 0; k < 2; ++k)
            {
                const float a = k == 0 ? aL : aR;
                const auto p0 = centre.getPointOnCircumference (outerR - rimW + 1.0f, a);
                const auto p1 = centre.getPointOnCircumference (outerR - 0.5f, a);
                g.setColour (juce::Colours::white);
                g.drawLine ({ p0, p1 }, 2.2f);

                if (rimLR && (rimActive || std::abs (s) > 0.01f))
                {
                    const auto lp = centre.getPointOnCircumference (outerR + 7.0f, a);
                    g.setFont (fonts::semibold (8.5f));
                    g.setColour (t.text.withAlpha (0.9f));
                    g.drawText (k == 0 ? "L" : "R", juce::Rectangle<float> (10.0f, 10.0f).withCentre (lp), juce::Justification::centred, false);
                }
            }
        }

        if (rimParam != nullptr && rimLR)
        {
            // Static L / R hint at the bottom corners, like a stereo knob
            g.setFont (fonts::semibold (10.0f));
            g.setColour (t.text.withAlpha (rimActive ? 0.9f : 0.55f));
            g.drawText ("L", juce::Rectangle<float> (12.0f, 12.0f).withCentre (centre.getPointOnCircumference (R + 2.0f, startA - 0.25f)), juce::Justification::centred, false);
            g.drawText ("R", juce::Rectangle<float> (12.0f, 12.0f).withCentre (centre.getPointOnCircumference (R + 2.0f, endA + 0.25f)), juce::Justification::centred, false);
        }
    }

    paint::knobDisc (g, centre, discR, valueA, hovering || dragging);

    // Modulation range arc (outside the knob) + live position
    if (modSource && modState.active)
    {
        const auto mc = modState.colour.isTransparent() ? t.modulation : modState.colour;
        const float mr = R - modW * 0.5f - 0.5f;
        const float a0 = startA + (endA - startA) * juce::jlimit (0.0f, 1.0f, juce::jmin (modState.lo, modState.hi));
        const float a1 = startA + (endA - startA) * juce::jlimit (0.0f, 1.0f, juce::jmax (modState.lo, modState.hi));

        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, mr, mr, 0.0f, a0, a1 + 0.0001f, true);
        // faint full-travel track so the arc reads as a grabbable ring
        juce::Path track;
        track.addCentredArc (centre.x, centre.y, mr, mr, 0.0f, startA, endA, true);
        g.setColour (mc.withAlpha (modActive ? 0.18f : 0.07f));
        g.strokePath (track, juce::PathStrokeType (modW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (mc.withAlpha (modActive ? 0.45f : 0.25f));
        g.strokePath (arc, juce::PathStrokeType (modW + (modActive ? 4.0f : 2.0f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (mc);
        g.strokePath (arc, juce::PathStrokeType (modW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float ca = startA + (endA - startA) * juce::jlimit (0.0f, 1.0f, modState.current);
        g.setColour (juce::Colours::white);
        g.fillEllipse (juce::Rectangle<float> (modW * 1.3f, modW * 1.3f).withCentre (centre.getPointOnCircumference (mr, ca)));
    }

    // Caption / value
    if (size != Size::mini)
    {
        auto textArea = getLocalBounds().toFloat().removeFromBottom (getCaptionHeight());
        const bool showValue = dragging || hovering || showValueAlways || textEntry != nullptr;
        const float h = size == Size::small ? 10.0f : 11.5f;

        if (modDropHover)
        {
            paint::caption (g, "+ Modulate", textArea, modDropColour.isTransparent() ? t.modulation : modDropColour,
                            juce::Justification::centred, h);
        }
        else if (rimActive && rimParam != nullptr)
        {
            g.setColour (t.text);
            g.setFont (fonts::semibold (h + 1.0f));
            g.drawText (rimLabel + " " + rimParam->getCurrentValueAsText(), textArea.expanded (20.0f, 0.0f), juce::Justification::centred, false);
        }
        else if (modActive)
        {
            const auto mc = modState.colour.isTransparent() ? t.modulation : modState.colour;
            g.setColour (mc);
            g.setFont (fonts::semibold (h + 1.0f));
            g.drawText ("Depth " + juce::String (juce::roundToInt ((modState.hi - modState.lo) * 100.0f)) + "%", textArea.expanded (20.0f, 0.0f),
                        juce::Justification::centred, false);
        }
        else if (showValue)
        {
            g.setColour (t.text);
            g.setFont (fonts::semibold (h + 1.0f));
            g.drawText (getValueText(), textArea, juce::Justification::centred, false);
        }
        else
        {
            paint::caption (g, label, textArea, enabled ? t.text.withAlpha (0.92f) : t.textFaint, juce::Justification::centred, h);

            if (modSource && modState.active)
            {
                const auto mc = modState.colour.isTransparent() ? t.modulation : modState.colour;
                const auto a = getModulationAnchor();
                g.setColour (mc);
                g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (a));
            }
        }
    }
}

void Knob::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        showContextMenu ([this] (juce::PopupMenu& m)
        {
            m.addItem ("Enter value...", [safe = juce::Component::SafePointer<Knob> (this)] { if (safe) safe->showTextEntry(); });
        });
        return;
    }

    if (e.mods.isAltDown())
    {
        resetToDefault();
        return;
    }

    if (e.mods.isCommandDown())
    {
        showTextEntry();
        return;
    }

    dragging = true;
    lastDragPos = e.position;
    dragZone = zoneAt (e.position);

    if (dragZone == Zone::rim && rimAttachment != nullptr)
        rimAttachment->beginGesture();
    else if (dragZone == Zone::modDepth)
    {
        if (onModulationDepthBegin) onModulationDepthBegin();
    }
    else
        beginEdit();

    e.source.enableUnboundedMouseMovement (true, false);
    repaint();
}

void Knob::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging) return;

    const auto delta = e.position - lastDragPos;
    lastDragPos = e.position;

    const float pixelsForFull = e.mods.isShiftDown() ? 1600.0f : 240.0f;
    const float d = (-delta.y + delta.x * 0.35f) / pixelsForFull;
    if (d == 0.0f) return;

    if (dragZone == Zone::rim && rimAttachment != nullptr)
    {
        rimNorm = juce::jlimit (0.0f, 1.0f, rimNorm + d);
        rimAttachment->setValueAsPartOfGesture (rimParam->convertFrom0to1 (rimNorm));
    }
    else if (dragZone == Zone::modDepth)
    {
        if (onModulationDepthDrag) onModulationDepthDrag (d);
    }
    else
    {
        normValue = juce::jlimit (0.0f, 1.0f, normValue + d);
        setNormalisedDuringEdit (normValue);
    }
    repaint();
}

void Knob::mouseUp (const juce::MouseEvent& e)
{
    if (! dragging) return;
    dragging = false;

    if (dragZone == Zone::rim && rimAttachment != nullptr)
        rimAttachment->endGesture();
    else if (dragZone == Zone::modDepth)
    {
        if (onModulationDepthEnd) onModulationDepthEnd();
    }
    else
        endEdit();

    dragZone = Zone::value;
    e.source.enableUnboundedMouseMovement (false, true);
    repaint();
}

void Knob::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;

    if (zoneAt (e.position) == Zone::rim && rimAttachment != nullptr)
        rimAttachment->setValueAsCompleteGesture (rimParam->convertFrom0to1 (rimParam->getDefaultValue()));
    else
        resetToDefault();
}

void Knob::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const float step = (e.mods.isShiftDown() ? 0.002f : 0.02f) * (w.isReversed ? -1.0f : 1.0f);
    const float dy = std::abs (w.deltaY) > std::abs (w.deltaX) ? w.deltaY : -w.deltaX;
    if (dy == 0.0f) return;

    const int steps = param.getNumSteps();
    float n = normValue;
    if (steps > 1 && steps < 64)
        n += (dy > 0 ? 1.0f : -1.0f) / (float) (steps - 1);
    else
        n += step * (dy > 0 ? 1.0f : -1.0f) * juce::jmin (4.0f, 1.0f + std::abs (dy) * 4.0f);

    setNormalisedAsGesture (n);
}

void Knob::showTextEntry()
{
    textEntry = std::make_unique<juce::TextEditor>();
    auto& ed = *textEntry;
    addAndMakeVisible (ed);

    auto area = getLocalBounds().toFloat().removeFromBottom (size == Size::small ? 16.0f : 20.0f).toNearestInt();
    ed.setBounds (area.withSizeKeepingCentre (juce::jmax (56, getWidth() - 4), area.getHeight()));
    ed.setJustification (juce::Justification::centred);
    ed.setFont (fonts::semibold (12.0f));
    ed.setText (param.getText (normValue, 32), false);
    ed.selectAll();
    ed.setSelectAllWhenFocused (true);

    auto finish = [this] (bool apply)
    {
        if (textEntry == nullptr) return;
        if (apply)
        {
            const auto text = textEntry->getText();
            setNormalisedAsGesture (param.getValueForText (text));
        }
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<Knob> (this)]
        {
            if (safe != nullptr) { safe->textEntry.reset(); safe->repaint(); }
        });
    };

    ed.onReturnKey = [finish] { finish (true); };
    ed.onEscapeKey = [finish] { finish (false); };
    ed.onFocusLost = [finish] { finish (true); };
    ed.grabKeyboardFocus();
    repaint();
}

} // namespace st
