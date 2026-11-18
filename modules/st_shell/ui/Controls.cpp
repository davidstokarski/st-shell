namespace st
{

//==============================================================================
ToggleChip::ToggleChip (juce::RangedAudioParameter& p, juce::UndoManager* um, const juce::String& t, juce::Colour on)
    : ParameterControl (p, um), text (t), onColour (on.isTransparent() ? Theme::get().accent : on)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void ToggleChip::paint (juce::Graphics& g)
{
    const auto& t = Theme::get();
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool on = isOn();
    const bool over = isMouseOver();
    const float rad = juce::jmin (r.getHeight() * 0.5f, t.radius);

    if (on)
    {
        g.setColour (onColour.withAlpha (0.16f));
        g.fillRoundedRectangle (r, rad);
        g.setColour (onColour.withAlpha (0.75f));
        g.drawRoundedRectangle (r, rad, 1.0f);
    }
    else
    {
        g.setColour (over ? t.bg4 : t.bg3);
        g.fillRoundedRectangle (r, rad);
        g.setColour (t.stroke);
        g.drawRoundedRectangle (r, rad, 1.0f);
    }

    const auto fg = on ? onColour.brighter (0.2f) : (over ? t.text : t.textDim);

    if (icon.has_value() && text.isEmpty())
    {
        icons::draw (g, *icon, r.reduced (r.getHeight() * 0.18f), fg, 1.7f);
    }
    else
    {
        auto content = r.reduced (6.0f, 0.0f);
        if (icon.has_value())
        {
            icons::draw (g, *icon, content.removeFromLeft (r.getHeight() * 0.8f).reduced (2.0f), fg, 1.6f);
            content.removeFromLeft (2.0f);
        }
        g.setColour (fg);
        g.setFont (fonts::make (juce::jlimit (9.5f, 12.5f, r.getHeight() * 0.48f), fonts::Weight::semibold, 0.04f));
        g.drawText (text, content, icon.has_value() ? juce::Justification::centredLeft : juce::Justification::centred, false);
    }
}

void ToggleChip::mouseUp (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { showContextMenu ({}); return; }
    if (getLocalBounds().contains (e.getPosition()))
        setNormalisedAsGesture (normValue >= 0.5f ? 0.0f : 1.0f);
}

//==============================================================================
Segmented::Segmented (juce::RangedAudioParameter& p, juce::UndoManager* um, juce::StringArray overrideLabels)
    : ParameterControl (p, um), accent (Theme::get().accent)
{
    if (overrideLabels.size() > 0) labels = overrideLabels;
    else if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (&p)) labels = c->choices;
    else labels = { "Off", "On" };

    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

int Segmented::getIndex() const
{
    return juce::jlimit (0, getNumChoices() - 1, juce::roundToInt (normValue * (float) (getNumChoices() - 1)));
}

int Segmented::indexAt (juce::Point<float> p) const
{
    const float w = (float) getWidth() / (float) juce::jmax (1, getNumChoices());
    return juce::jlimit (0, getNumChoices() - 1, (int) (p.x / w));
}

void Segmented::paint (juce::Graphics& g)
{
    const auto& t = Theme::get();
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const float rad = juce::jmin (r.getHeight() * 0.5f, t.radius);

    g.setColour (t.bg1);
    g.fillRoundedRectangle (r, rad);
    g.setColour (t.stroke);
    g.drawRoundedRectangle (r, rad, 1.0f);

    const int n = getNumChoices();
    const float w = r.getWidth() / (float) n;
    const int sel = getIndex();

    for (int i = 0; i < n; ++i)
    {
        auto seg = juce::Rectangle<float> (r.getX() + w * (float) i, r.getY(), w, r.getHeight());

        if (i == sel)
        {
            auto pill = seg.reduced (2.0f);
            g.setColour (t.bg4);
            g.fillRoundedRectangle (pill, rad - 2.0f);
            g.setColour (accent.withAlpha (0.55f));
            g.drawRoundedRectangle (pill, rad - 2.0f, 1.0f);
        }

        g.setColour (i == sel ? t.text : (i == hoverIndex ? t.text.withAlpha (0.8f) : t.textDim));
        g.setFont (fonts::make (juce::jlimit (9.5f, 12.0f, r.getHeight() * 0.46f), fonts::Weight::semibold, 0.03f));
        g.drawText (labels[i], seg, juce::Justification::centred, false);
    }
}

void Segmented::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { showContextMenu ({}); return; }
    const int n = getNumChoices();
    if (n > 1)
        setNormalisedAsGesture ((float) indexAt (e.position) / (float) (n - 1));
}

void Segmented::mouseMove (const juce::MouseEvent& e)
{
    const int i = indexAt (e.position);
    if (i != hoverIndex) { hoverIndex = i; repaint(); }
}

//==============================================================================
Dropdown::Dropdown (juce::RangedAudioParameter& p, juce::UndoManager* um)
    : ParameterControl (p, um), accent (Theme::get().accent)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

int Dropdown::getNumChoices() const
{
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (&param)) return c->choices.size();
    return juce::jmax (2, param.getNumSteps());
}

int Dropdown::getIndex() const
{
    return juce::jlimit (0, getNumChoices() - 1, juce::roundToInt (normValue * (float) (getNumChoices() - 1)));
}

juce::Rectangle<float> Dropdown::leftArrow() const
{
    auto r = getLocalBounds().toFloat();
    return r.removeFromLeft (r.getHeight() * 0.9f);
}

juce::Rectangle<float> Dropdown::rightArrow() const
{
    auto r = getLocalBounds().toFloat();
    return r.removeFromRight (r.getHeight() * 0.9f);
}

void Dropdown::paint (juce::Graphics& g)
{
    const auto& t = Theme::get();
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool over = isMouseOver (true);
    const float rad = juce::jmin (t.radius, r.getHeight() * 0.5f);

    g.setColour (over ? t.bg4 : t.bg3);
    g.fillRoundedRectangle (r, rad);
    g.setColour (t.stroke);
    g.drawRoundedRectangle (r, rad, 1.0f);

    auto content = r.reduced (large ? 12.0f : 8.0f, 0.0f);

    if (arrows)
    {
        icons::draw (g, icons::Icon::chevronLeft, leftArrow().reduced (r.getHeight() * 0.28f), t.textDim, 1.8f);
        icons::draw (g, icons::Icon::chevronRight, rightArrow().reduced (r.getHeight() * 0.28f), t.textDim, 1.8f);
        content = r.reduced (r.getHeight() * 0.9f, 0.0f);
    }
    else
    {
        icons::draw (g, icons::Icon::chevronDown, content.removeFromRight (r.getHeight() * 0.5f).withSizeKeepingCentre (r.getHeight() * 0.42f, r.getHeight() * 0.42f),
                     over ? t.text : t.textDim, 1.8f);
    }

    if (caption.isNotEmpty())
    {
        auto capArea = content.removeFromLeft (fonts::stringWidth (fonts::caption (10.0f), caption.toUpperCase()) + 10.0f);
        paint::caption (g, caption, capArea, t.textFaint, juce::Justification::centredLeft, 10.0f);
    }

    const auto text = param.getCurrentValueAsText();
    g.setColour (t.text);
    g.setFont (large ? fonts::semibold (juce::jmin (17.0f, r.getHeight() * 0.5f)) : fonts::medium (juce::jmin (13.0f, r.getHeight() * 0.5f)));
    g.drawText (text, content, arrows ? juce::Justification::centred : juce::Justification::centredLeft, true);
}

void Dropdown::step (int delta)
{
    const int n = getNumChoices();
    const int idx = (getIndex() + delta + n) % n;
    setNormalisedAsGesture ((float) idx / (float) (n - 1));
}

void Dropdown::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { showContextMenu ({}); return; }

    if (arrows)
    {
        if (leftArrow().contains (e.position))  { step (-1); return; }
        if (rightArrow().contains (e.position)) { step (+1); return; }
    }

    juce::PopupMenu menu;
    const int current = getIndex();

    if (buildMenu)
        menu = buildMenu (current);
    else if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (&param))
        for (int i = 0; i < c->choices.size(); ++i)
            menu.addItem (i + 1, c->choices[i], true, i == current);

    const int n = getNumChoices();
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth()),
                        [safe = juce::Component::SafePointer<Dropdown> (this), n] (int result)
                        {
                            if (safe != nullptr && result > 0)
                                safe->setNormalisedAsGesture ((float) (result - 1) / (float) juce::jmax (1, n - 1));
                        });
}

void Dropdown::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const float d = std::abs (w.deltaY) > std::abs (w.deltaX) ? w.deltaY : -w.deltaX;
    if (std::abs (d) > 0.01f) step (d > 0 ? -1 : 1);
}

//==============================================================================
NumberBox::NumberBox (juce::RangedAudioParameter& p, juce::UndoManager* um, const juce::String& c)
    : ParameterControl (p, um), caption (c), accent (Theme::get().accent)
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void NumberBox::paint (juce::Graphics& g)
{
    const auto& t = Theme::get();
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool over = isMouseOver() || dragging;

    g.setColour (over ? t.bg4 : t.bg3);
    g.fillRoundedRectangle (r, t.radiusSmall + 1.0f);
    g.setColour (dragging ? accent.withAlpha (0.6f) : t.stroke);
    g.drawRoundedRectangle (r, t.radiusSmall + 1.0f, 1.0f);

    auto content = r.reduced (8.0f, 0.0f);
    if (caption.isNotEmpty())
    {
        paint::caption (g, caption, content, t.textFaint, juce::Justification::centredLeft, 9.5f);
    }
    g.setColour (t.text);
    g.setFont (fonts::semibold (juce::jmin (12.5f, r.getHeight() * 0.52f)));
    g.drawText (param.getCurrentValueAsText(), content, caption.isNotEmpty() ? juce::Justification::centredRight : juce::Justification::centred, false);
}

void NumberBox::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { showContextMenu ({}); return; }
    if (e.mods.isAltDown()) { resetToDefault(); return; }
    dragging = true;
    lastY = e.position.y;
    beginEdit();
    e.source.enableUnboundedMouseMovement (true, false);
}

void NumberBox::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging) return;
    const float dy = lastY - e.position.y;
    lastY = e.position.y;
    normValue = juce::jlimit (0.0f, 1.0f, normValue + dy / (e.mods.isShiftDown() ? 1500.0f : 220.0f));
    setNormalisedDuringEdit (normValue);
    repaint();
}

void NumberBox::mouseUp (const juce::MouseEvent& e)
{
    if (! dragging) return;
    dragging = false;
    endEdit();
    e.source.enableUnboundedMouseMovement (false, true);
    repaint();
}

void NumberBox::mouseDoubleClick (const juce::MouseEvent&) { resetToDefault(); }

//==============================================================================
IconButton::IconButton (icons::Icon i, const juce::String& tip) : juce::Button (tip), icon (i), onColour (Theme::get().accent)
{
    setTooltip (tip);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void IconButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    const auto& t = Theme::get();
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool on = getToggleState();

    if (drawBackground || over || on)
    {
        g.setColour (on ? onColour.withAlpha (0.16f) : (down ? t.bg4.brighter (0.05f) : over ? t.bg4 : t.bg3));
        g.fillRoundedRectangle (r, t.radius - 2.0f);
        if (drawBackground || on)
        {
            g.setColour (on ? onColour.withAlpha (0.6f) : t.stroke);
            g.drawRoundedRectangle (r, t.radius - 2.0f, 1.0f);
        }
    }

    const auto fg = ! isEnabled() ? t.textFaint : on ? onColour : (over ? t.text : t.textDim);

    if (text.isEmpty())
    {
        icons::draw (g, icon, r.reduced (juce::jmax (3.0f, r.getHeight() * 0.2f)), fg, 1.7f);
    }
    else
    {
        auto c = r.reduced (8.0f, 0.0f);
        icons::draw (g, icon, c.removeFromLeft (r.getHeight() * 0.62f).reduced (1.0f), fg, 1.7f);
        g.setColour (fg);
        g.setFont (fonts::semibold (12.0f));
        g.drawText (text, c.withTrimmedLeft (5.0f), juce::Justification::centredLeft, false);
    }
}

//==============================================================================
void PillButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    const auto& t = Theme::get();
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool on = getToggleState();
    const auto oc = onColour.isTransparent() ? t.accent : onColour;
    const float rad = juce::jmin (t.radius - 1.0f, r.getHeight() * 0.5f);

    g.setColour (on ? oc.withAlpha (0.18f) : (down ? t.bg4.brighter (0.05f) : over ? t.bg4 : t.bg3));
    g.fillRoundedRectangle (r, rad);
    g.setColour (on ? oc.withAlpha (0.7f) : t.stroke);
    g.drawRoundedRectangle (r, rad, 1.0f);

    g.setColour (on ? oc.brighter (0.25f) : (over ? t.text : t.textDim));
    g.setFont (fonts::make (juce::jlimit (9.5f, 12.5f, r.getHeight() * 0.48f), fonts::Weight::semibold, 0.04f));
    g.drawText (getButtonText(), r, juce::Justification::centred, false);
}

} // namespace st
