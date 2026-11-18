namespace st
{

LevelMeter::LevelMeter (MeterSource& s, const juce::String& c) : source (s), caption (c)
{
    startTimerHz (40);
    lastTime = juce::Time::getMillisecondCounterHiRes() * 0.001;
    setInterceptsMouseClicks (true, false);
}

float LevelMeter::dbToY (float db, juce::Rectangle<float> bar) const
{
    // Slightly non-linear scale: more resolution near the top.
    const float n = juce::jlimit (0.0f, 1.0f, (db - lo) / (hi - lo));
    const float shaped = std::pow (n, 1.35f);
    return bar.getBottom() - shaped * bar.getHeight();
}

void LevelMeter::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const float dt = (float) juce::jlimit (0.0, 0.2, now - lastTime);
    lastTime = now;

    auto r = source.read();
    bool changed = false;

    for (size_t c = 0; c < 2; ++c)
    {
        const float pk = juce::Decibels::gainToDecibels (r.peak[c], -100.0f);
        const float rm = juce::Decibels::gainToDecibels (r.rms[c], -100.0f);

        const float newPeak = pk > peakDb[c] ? pk : juce::jmax (pk, peakDb[c] - 26.0f * dt);
        const float newRms = rm;

        if (pk >= holdDb[c]) { holdDb[c] = pk; holdTime[c] = now; }
        else if (now - holdTime[c] > 1.5) holdDb[c] = juce::jmax (-100.0f, holdDb[c] - 30.0f * dt);

        maxPeakDb = juce::jmax (maxPeakDb, pk);

        changed |= std::abs (newPeak - peakDb[c]) > 0.05f || std::abs (newRms - rmsDb[c]) > 0.05f;
        peakDb[c] = newPeak;
        rmsDb[c] = newRms;
    }

    if (changed) repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    const auto& t = Theme::get();
    auto r = getLocalBounds().toFloat();

    auto readout = r.removeFromTop (16.0f);
    if (caption.isNotEmpty())
        paint::caption (g, caption, r.removeFromBottom (14.0f), t.textFaint, juce::Justification::centred, 9.0f);

    // Peak readout
    g.setFont (fonts::semibold (10.5f));
    g.setColour (maxPeakDb > 0.0f ? t.danger : t.textDim);
    g.drawText (maxPeakDb <= -99.0f ? juce::String ("-inf") : juce::String (maxPeakDb, 1), readout, juce::Justification::centred, false);

    auto bars = r.reduced (0.0f, 2.0f);
    if (showScale) bars.removeFromRight (16.0f);

    const float gap = 2.0f;
    const float bw = (bars.getWidth() - gap) * 0.5f;

    for (int c = 0; c < 2; ++c)
    {
        auto bar = juce::Rectangle<float> (bars.getX() + (float) c * (bw + gap), bars.getY(), bw, bars.getHeight());

        g.setColour (t.bg1);
        g.fillRoundedRectangle (bar, 2.0f);

        const float yTop = dbToY (hi, bar);
        const float y0 = dbToY (0.0f, bar);
        const float yWarn = dbToY (-6.0f, bar);

        juce::ColourGradient grad (t.positive.withAlpha (0.85f), 0.0f, bar.getBottom(), t.danger, 0.0f, yTop, false);
        grad.addColour (juce::jlimit (0.0, 1.0, (double) ((bar.getBottom() - yWarn) / bar.getHeight())), t.positive);
        grad.addColour (juce::jlimit (0.0, 1.0, (double) ((bar.getBottom() - y0) / bar.getHeight()) - 0.001), t.warning);

        const float yr = dbToY (rmsDb[(size_t) c], bar);
        const float yp = dbToY (peakDb[(size_t) c], bar);

        g.setGradientFill (grad);
        g.saveState();
        g.reduceClipRegion (bar.withTop (yp).toNearestInt());
        g.setOpacity (0.35f);
        g.fillRoundedRectangle (bar, 2.0f);
        g.restoreState();

        g.saveState();
        g.reduceClipRegion (bar.withTop (yr).toNearestInt());
        g.setGradientFill (grad);
        g.fillRoundedRectangle (bar, 2.0f);
        g.restoreState();

        if (holdDb[(size_t) c] > lo)
        {
            const float yh = dbToY (holdDb[(size_t) c], bar);
            g.setColour (holdDb[(size_t) c] > 0.0f ? t.danger : t.text.withAlpha (0.8f));
            g.fillRect (bar.getX(), yh - 0.5f, bar.getWidth(), 1.5f);
        }

        // 0 dB tick
        g.setColour (t.bg0.withAlpha (0.8f));
        g.fillRect (bar.getX(), y0, bar.getWidth(), 1.0f);
    }

    if (showScale)
    {
        auto scale = getLocalBounds().toFloat().withTrimmedTop (16.0f).reduced (0.0f, 2.0f).removeFromRight (14.0f);
        if (caption.isNotEmpty()) scale.removeFromBottom (14.0f);
        g.setFont (fonts::medium (8.5f));
        g.setColour (t.textFaint);
        for (float db : { 0.0f, -6.0f, -12.0f, -24.0f, -48.0f })
        {
            const float y = dbToY (db, scale);
            g.drawText (juce::String ((int) std::abs (db)), juce::Rectangle<float> (scale.getX(), y - 5.0f, scale.getWidth(), 10.0f),
                        juce::Justification::centredRight, false);
        }
    }
}

//==============================================================================
GainChangeMeter::GainChangeMeter (std::function<float()> fn, float r) : getter (std::move (fn)), range (r), colour (Theme::get().accent)
{
    startTimerHz (30);
}

void GainChangeMeter::timerCallback()
{
    const float v = getter ? getter() : 0.0f;
    if (std::abs (v - value) > 0.05f) { value = v; repaint(); }
}

void GainChangeMeter::paint (juce::Graphics& g)
{
    const auto& t = Theme::get();
    auto r = getLocalBounds().toFloat();
    g.setColour (t.bg1);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);

    const float cx = r.getCentreX();
    const float w = r.getWidth() * 0.5f * juce::jlimit (-1.0f, 1.0f, value / range);
    auto fill = w >= 0 ? juce::Rectangle<float> (cx, r.getY(), w, r.getHeight()) : juce::Rectangle<float> (cx + w, r.getY(), -w, r.getHeight());
    g.setColour (w >= 0 ? t.positive.withAlpha (0.8f) : colour.withAlpha (0.85f));
    g.fillRoundedRectangle (fill.reduced (0.0f, 1.0f), (r.getHeight() - 2.0f) * 0.5f);

    g.setColour (t.textFaint);
    g.fillRect (cx - 0.5f, r.getY(), 1.0f, r.getHeight());
}

} // namespace st
