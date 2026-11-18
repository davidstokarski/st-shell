namespace st
{

static constexpr int popoverPadX = 18, popoverPadY = 14;

Popover::Popover (const juce::String& t, const juce::String& sub) : title (t), subtitle (sub)
{
    closeButton.setMinimal (true);
    closeButton.onClick = [this] { requestClose(); };
    addAndMakeVisible (closeButton);
    setWantsKeyboardFocus (true);
}

juce::Rectangle<int> Popover::getContentBounds() const noexcept
{
    auto r = getPanelBounds().reduced (popoverPadX, popoverPadY);
    r.removeFromTop (headerHeight + 10);
    return r;
}

juce::Rectangle<int> Popover::getHeaderBounds() const noexcept
{
    auto r = getPanelBounds().reduced (popoverPadX, popoverPadY).removeFromTop (headerHeight);
    r.removeFromRight (22 + 10);
    const float tw = fonts::stringWidth (fonts::caption (11.5f), title.toUpperCase())
                   + (subtitle.isNotEmpty() ? fonts::stringWidth (fonts::medium (11.5f), subtitle) + 14.0f : 0.0f);
    r.removeFromLeft ((int) tw + 16);
    return r;
}

void Popover::resized()
{
    auto header = getPanelBounds().reduced (popoverPadX, popoverPadY).removeFromTop (headerHeight);
    closeButton.setBounds (header.removeFromRight (22).withSizeKeepingCentre (22, 22).translated (4, 0));
    layoutContent (getContentBounds());
}

void Popover::paint (juce::Graphics& g)
{
    const auto& t = Theme::get();
    auto panel = getPanelBounds().toFloat();

    paint::softShadow (g, panel, t.radiusLarge, 14.0f, 0.6f);
    g.setColour (t.bg2.withAlpha (0.97f));
    g.fillRoundedRectangle (panel, t.radiusLarge);

    // Hairline: bright along the top, almost invisible lower down.
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.11f), 0.0f, panel.getY(),
                                             juce::Colours::white.withAlpha (0.03f), 0.0f, panel.getY() + 60.0f, false));
    g.drawRoundedRectangle (panel.reduced (0.5f), t.radiusLarge, 1.0f);

    auto titleArea = panel.reduced ((float) popoverPadX, (float) popoverPadY).removeFromTop ((float) headerHeight);
    paint::caption (g, title, titleArea, t.text.withAlpha (0.92f), juce::Justification::centredLeft, 11.5f);
    if (subtitle.isNotEmpty())
    {
        const float tw = fonts::stringWidth (fonts::caption (11.5f), title.toUpperCase());
        g.setFont (fonts::medium (11.5f));
        g.setColour (t.textFaint);
        g.drawText (subtitle, titleArea.withTrimmedLeft (tw + 14.0f), juce::Justification::centredLeft, false);
    }

    paintContent (g);
}

bool Popover::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey) { requestClose(); return true; }
    return false;
}

} // namespace st
