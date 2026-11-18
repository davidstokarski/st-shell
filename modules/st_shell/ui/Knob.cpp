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

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (180));
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

juce::Rectangle<float> Knob::getDialBounds() const
{
    auto r = getLocalBounds().toFloat();
    const float textH = size == Size::mini ? 0.0f : (size == Size::small ? 15.0f : 18.0f);
    r.removeFromBottom (textH);
    const float d = juce::jmin (r.getWidth(), r.getHeight());
    return r.withSizeKeepingCentre (d, d);
}

void Knob::paint (juce::Graphics& g)
{
    const auto& t = Theme::get();
    const bool enabled = isEnabled();
    auto dial = getDialBounds();
    const auto centre = dial.getCentre();
    const float R = dial.getWidth() * 0.5f;

    constexpr float startA = juce::MathConstants<float>::pi * 1.25f;
    constexpr float endA   = juce::MathConstants<float>::pi * 2.75f;
    const float range = endA - startA;
    const float valueA = startA + range * normValue;
    const float defNorm = param.getDefaultValue();
    const float originA = bipolar ? startA + range * 0.5f : (defNorm > 0.001f && defNorm < 0.999f ? startA : startA);

    const float arcW   = juce::jmax (2.0f, R * (size == Size::large ? 0.075f : 0.09f));
    const float arcR   = R - arcW * 0.5f - (modSource ? arcW * 1.4f : 0.5f);
    const float bodyR  = arcR - arcW * 1.6f;
    const auto active  = enabled ? colour : t.textFaint;

    // Track
    {
        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startA, endA, true);
        g.setColour (t.bg3.brighter (0.08f));
        g.strokePath (track, juce::PathStrokeType (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Value arc
    if (std::abs (valueA - originA) > 0.001f)
    {
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, juce::jmin (originA, valueA), juce::jmax (originA, valueA), true);
        if (enabled) paint::glowStroke (g, arc, active, arcW, arcW * 2.2f);
        else { g.setColour (active); g.strokePath (arc, juce::PathStrokeType (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); }
    }

    // Modulation ring
    if (modSource && modState.active)
    {
        const float mr = arcR + arcW * 1.45f;
        const float a0 = startA + range * juce::jlimit (0.0f, 1.0f, modState.lo);
        const float a1 = startA + range * juce::jlimit (0.0f, 1.0f, modState.hi);
        const auto mc = modState.colour.isTransparent() ? t.modulation : modState.colour;

        juce::Path ring;
        ring.addCentredArc (centre.x, centre.y, mr, mr, 0.0f, juce::jmin (a0, a1), juce::jmax (a0, a1) + 0.0001f, true);
        g.setColour (mc.withAlpha (0.55f));
        g.strokePath (ring, juce::PathStrokeType (arcW * 0.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float ca = startA + range * juce::jlimit (0.0f, 1.0f, modState.current);
        auto p = centre.getPointOnCircumference (mr, ca);
        g.setColour (mc);
        g.fillEllipse (juce::Rectangle<float> (arcW * 1.3f, arcW * 1.3f).withCentre (p));
    }

    // Body
    {
        auto body = juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (centre);
        paint::softShadow (g, body, bodyR, bodyR * 0.35f, 0.6f);

        const auto top = t.bg4.brighter (hovering || dragging ? 0.18f : 0.1f);
        const auto bottom = t.bg2.darker (0.1f);
        juce::ColourGradient grad (top, centre.x - bodyR * 0.5f, centre.y - bodyR, bottom, centre.x + bodyR * 0.4f, centre.y + bodyR, false);
        g.setGradientFill (grad);
        g.fillEllipse (body);

        // rim: light on top, dark at bottom
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.16f), centre.x, body.getY(),
                                                 juce::Colours::black.withAlpha (0.35f), centre.x, body.getBottom(), false));
        g.drawEllipse (body.reduced (0.5f), 1.0f);

        // inner face
        auto face = body.reduced (bodyR * 0.16f);
        g.setGradientFill (juce::ColourGradient (t.bg3.brighter (0.05f), centre.x, face.getY(),
                                                 t.bg3.darker (0.15f), centre.x, face.getBottom(), false));
        g.fillEllipse (face);
    }

    // Pointer
    {
        const auto p0 = centre.getPointOnCircumference (bodyR * 0.30f, valueA);
        const auto p1 = centre.getPointOnCircumference (bodyR * 0.78f, valueA);
        g.setColour (enabled ? t.text : t.textFaint);
        g.drawLine ({ p0, p1 }, juce::jmax (1.6f, R * 0.06f));
    }

    // Caption / value
    if (size != Size::mini)
    {
        auto textArea = getLocalBounds().toFloat().removeFromBottom (size == Size::small ? 15.0f : 18.0f);
        const bool showValue = dragging || hovering || showValueAlways || textEntry != nullptr;
        const float h = size == Size::small ? 10.0f : 11.0f;

        if (showValue)
        {
            g.setColour (t.text);
            g.setFont (fonts::semibold (h + 1.0f));
            g.drawText (getValueText(), textArea, juce::Justification::centred, false);
        }
        else
        {
            paint::caption (g, label, textArea, enabled ? t.textDim : t.textFaint, juce::Justification::centred, h);
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
    if (d != 0.0f)
    {
        normValue = juce::jlimit (0.0f, 1.0f, normValue + d);
        setNormalisedDuringEdit (normValue);
        repaint();
    }
}

void Knob::mouseUp (const juce::MouseEvent& e)
{
    if (! dragging) return;
    dragging = false;
    endEdit();
    e.source.enableUnboundedMouseMovement (false, true);
    repaint();
}

void Knob::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu())
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
