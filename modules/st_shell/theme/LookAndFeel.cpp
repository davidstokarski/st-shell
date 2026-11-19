namespace st
{

//==============================================================================
namespace icons
{
    juce::Path path (Icon icon)
    {
        juce::Path p;

        switch (icon)
        {
            case Icon::chevronLeft:  p.startNewSubPath (14.5f, 6.0f);  p.lineTo (8.5f, 12.0f);  p.lineTo (14.5f, 18.0f); break;
            case Icon::chevronRight: p.startNewSubPath (9.5f, 6.0f);   p.lineTo (15.5f, 12.0f); p.lineTo (9.5f, 18.0f);  break;
            case Icon::chevronDown:  p.startNewSubPath (6.0f, 9.5f);   p.lineTo (12.0f, 15.5f); p.lineTo (18.0f, 9.5f);  break;
            case Icon::chevronUp:    p.startNewSubPath (6.0f, 14.5f);  p.lineTo (12.0f, 8.5f);  p.lineTo (18.0f, 14.5f); break;

            case Icon::undo:
                p.startNewSubPath (9.0f, 5.5f); p.lineTo (4.5f, 10.0f); p.lineTo (9.0f, 14.5f);
                p.startNewSubPath (4.5f, 10.0f); p.lineTo (14.0f, 10.0f);
                p.cubicTo (17.6f, 10.0f, 19.5f, 12.4f, 19.5f, 14.8f); p.cubicTo (19.5f, 17.2f, 17.6f, 19.0f, 14.0f, 19.0f); p.lineTo (11.0f, 19.0f);
                break;
            case Icon::redo:
                p.startNewSubPath (15.0f, 5.5f); p.lineTo (19.5f, 10.0f); p.lineTo (15.0f, 14.5f);
                p.startNewSubPath (19.5f, 10.0f); p.lineTo (10.0f, 10.0f);
                p.cubicTo (6.4f, 10.0f, 4.5f, 12.4f, 4.5f, 14.8f); p.cubicTo (4.5f, 17.2f, 6.4f, 19.0f, 10.0f, 19.0f); p.lineTo (13.0f, 19.0f);
                break;

            case Icon::gear:
            {
                const int teeth = 8;
                for (int i = 0; i < teeth * 2; ++i)
                {
                    const float a0 = juce::MathConstants<float>::twoPi * (float) i / (float) (teeth * 2);
                    const float a1 = juce::MathConstants<float>::twoPi * (float) (i + 1) / (float) (teeth * 2);
                    const float r = (i % 2 == 0) ? 8.6f : 6.6f;
                    const juce::Point<float> c (12.0f, 12.0f);
                    auto pt0 = c.getPointOnCircumference (r, a0 + 0.06f);
                    auto pt1 = c.getPointOnCircumference (r, a1 - 0.06f);
                    if (i == 0) p.startNewSubPath (pt0); else p.lineTo (pt0);
                    p.lineTo (pt1);
                }
                p.closeSubPath();
                p.addEllipse (9.0f, 9.0f, 6.0f, 6.0f);
                break;
            }

            case Icon::power:
                p.addCentredArc (12.0f, 13.0f, 7.0f, 7.0f, 0.0f, 0.75f, juce::MathConstants<float>::twoPi - 0.75f, true);
                p.startNewSubPath (12.0f, 4.0f); p.lineTo (12.0f, 12.0f);
                break;

            case Icon::copy:
                p.addRoundedRectangle (8.5f, 8.5f, 11.0f, 11.0f, 2.0f);
                p.startNewSubPath (5.0f, 15.0f); p.lineTo (5.0f, 6.5f);
                p.quadraticTo (5.0f, 5.0f, 6.5f, 5.0f); p.lineTo (15.0f, 5.0f);
                break;

            case Icon::plus:  p.startNewSubPath (12.0f, 5.5f); p.lineTo (12.0f, 18.5f); p.startNewSubPath (5.5f, 12.0f); p.lineTo (18.5f, 12.0f); break;
            case Icon::close: p.startNewSubPath (6.5f, 6.5f); p.lineTo (17.5f, 17.5f); p.startNewSubPath (17.5f, 6.5f); p.lineTo (6.5f, 17.5f); break;

            case Icon::save:
                p.startNewSubPath (5.0f, 5.0f); p.lineTo (16.0f, 5.0f); p.lineTo (19.0f, 8.0f); p.lineTo (19.0f, 19.0f); p.lineTo (5.0f, 19.0f); p.closeSubPath();
                p.addRectangle (8.0f, 13.0f, 8.0f, 6.0f);
                p.startNewSubPath (8.0f, 5.0f); p.lineTo (8.0f, 9.0f); p.lineTo (14.0f, 9.0f); p.lineTo (14.0f, 5.0f);
                break;

            case Icon::folder:
                p.startNewSubPath (4.0f, 7.0f); p.lineTo (9.5f, 7.0f); p.lineTo (11.5f, 9.0f); p.lineTo (20.0f, 9.0f);
                p.lineTo (20.0f, 18.0f); p.lineTo (4.0f, 18.0f); p.closeSubPath();
                break;

            case Icon::menu:
                for (float y : { 7.0f, 12.0f, 17.0f }) { p.startNewSubPath (5.0f, y); p.lineTo (19.0f, y); }
                break;

            case Icon::dice:
                p.addRoundedRectangle (5.0f, 5.0f, 14.0f, 14.0f, 3.0f);
                for (auto pt : { juce::Point<float> (9.0f, 9.0f), { 15.0f, 15.0f }, { 12.0f, 12.0f } })
                    p.addEllipse (pt.x - 0.9f, pt.y - 0.9f, 1.8f, 1.8f);
                break;

            case Icon::link:
                p.addRoundedRectangle (3.5f, 9.0f, 10.0f, 6.0f, 3.0f);
                p.addRoundedRectangle (10.5f, 9.0f, 10.0f, 6.0f, 3.0f);
                break;

            case Icon::solo:  p.addEllipse (6.0f, 6.0f, 12.0f, 12.0f); p.addEllipse (10.5f, 10.5f, 3.0f, 3.0f); break;
            case Icon::mute:
                p.startNewSubPath (5.0f, 10.0f); p.lineTo (8.0f, 10.0f); p.lineTo (12.0f, 6.0f); p.lineTo (12.0f, 18.0f); p.lineTo (8.0f, 14.0f); p.lineTo (5.0f, 14.0f); p.closeSubPath();
                p.startNewSubPath (15.0f, 9.5f); p.lineTo (20.0f, 14.5f); p.startNewSubPath (20.0f, 9.5f); p.lineTo (15.0f, 14.5f);
                break;

            case Icon::info:
                p.addEllipse (4.5f, 4.5f, 15.0f, 15.0f);
                p.startNewSubPath (12.0f, 11.0f); p.lineTo (12.0f, 16.0f);
                p.addEllipse (11.3f, 7.6f, 1.4f, 1.4f);
                break;

            case Icon::resize:
                p.startNewSubPath (19.0f, 9.0f); p.lineTo (9.0f, 19.0f);
                p.startNewSubPath (19.0f, 14.0f); p.lineTo (14.0f, 19.0f);
                break;

            case Icon::swap:
                p.startNewSubPath (5.0f, 9.0f); p.lineTo (18.0f, 9.0f); p.startNewSubPath (15.0f, 6.0f); p.lineTo (18.0f, 9.0f); p.lineTo (15.0f, 12.0f);
                p.startNewSubPath (19.0f, 15.0f); p.lineTo (6.0f, 15.0f); p.startNewSubPath (9.0f, 12.0f); p.lineTo (6.0f, 15.0f); p.lineTo (9.0f, 18.0f);
                break;

            case Icon::star:
                p.addStar ({ 12.0f, 12.5f }, 5, 3.6f, 8.0f, 0.0f);
                break;

            case Icon::trash:
                p.startNewSubPath (5.0f, 7.0f); p.lineTo (19.0f, 7.0f);
                p.startNewSubPath (10.0f, 7.0f); p.lineTo (10.0f, 5.0f); p.lineTo (14.0f, 5.0f); p.lineTo (14.0f, 7.0f);
                p.startNewSubPath (7.0f, 7.0f); p.lineTo (8.0f, 19.0f); p.lineTo (16.0f, 19.0f); p.lineTo (17.0f, 7.0f);
                break;

            case Icon::pencil:
                p.startNewSubPath (5.0f, 19.0f); p.lineTo (6.0f, 15.0f); p.lineTo (15.5f, 5.5f); p.lineTo (18.5f, 8.5f);
                p.lineTo (9.0f, 18.0f); p.closeSubPath();
                p.startNewSubPath (13.5f, 7.5f); p.lineTo (16.5f, 10.5f);
                break;

            case Icon::scissors:
                p.addEllipse (5.0f, 14.5f, 5.0f, 5.0f);
                p.addEllipse (14.0f, 14.5f, 5.0f, 5.0f);
                p.startNewSubPath (9.0f, 15.0f); p.lineTo (16.5f, 4.5f);
                p.startNewSubPath (15.0f, 15.0f); p.lineTo (7.5f, 4.5f);
                break;

            case Icon::stereoLink:
                p.addEllipse (4.0f, 7.0f, 10.0f, 10.0f);
                p.addEllipse (10.0f, 7.0f, 10.0f, 10.0f);
                break;

            case Icon::bars:
                p.startNewSubPath (5.0f, 13.0f);  p.lineTo (5.0f, 17.0f);
                p.startNewSubPath (9.0f, 9.0f);   p.lineTo (9.0f, 17.0f);
                p.startNewSubPath (12.0f, 6.0f);  p.lineTo (12.0f, 17.0f);
                p.startNewSubPath (15.0f, 9.0f);  p.lineTo (15.0f, 17.0f);
                p.startNewSubPath (19.0f, 12.0f); p.lineTo (19.0f, 17.0f);
                break;

            case Icon::chevronsRight:
                p.startNewSubPath (6.0f, 7.0f);  p.lineTo (11.0f, 12.0f); p.lineTo (6.0f, 17.0f);
                p.startNewSubPath (12.0f, 7.0f); p.lineTo (17.0f, 12.0f); p.lineTo (12.0f, 17.0f);
                break;

            case Icon::chevronsLeft:
                p.startNewSubPath (18.0f, 7.0f); p.lineTo (13.0f, 12.0f); p.lineTo (18.0f, 17.0f);
                p.startNewSubPath (12.0f, 7.0f); p.lineTo (7.0f, 12.0f);  p.lineTo (12.0f, 17.0f);
                break;

            case Icon::gears:   // a large and a small gear (settings that interact): rings with short teeth
            {
                auto gear = [&p] (float cx, float cy, float r, int teeth)
                {
                    p.addEllipse (cx - r, cy - r, 2.0f * r, 2.0f * r);
                    for (int i = 0; i < teeth; ++i)
                    {
                        const float a = juce::MathConstants<float>::twoPi * (float) i / (float) teeth;
                        p.startNewSubPath (cx + std::cos (a) * r, cy + std::sin (a) * r);
                        p.lineTo (cx + std::cos (a) * (r + 2.2f), cy + std::sin (a) * (r + 2.2f));
                    }
                };
                gear (9.0f, 9.5f, 4.2f, 8);
                gear (17.5f, 16.5f, 2.4f, 6);
                break;
            }

            case Icon::sidechain:   // arrow up from a base line (external trigger)
                p.startNewSubPath (6.0f, 19.0f); p.lineTo (18.0f, 19.0f);
                p.startNewSubPath (12.0f, 16.0f); p.lineTo (12.0f, 5.0f);
                p.startNewSubPath (8.0f, 9.0f); p.lineTo (12.0f, 5.0f); p.lineTo (16.0f, 9.0f);
                break;

            case Icon::multiview:   // stacked track rows (the multiview)
                p.addRoundedRectangle (4.0f, 5.0f, 16.0f, 4.0f, 1.0f);
                p.addRoundedRectangle (4.0f, 10.5f, 16.0f, 4.0f, 1.0f);
                p.addRoundedRectangle (4.0f, 16.0f, 10.0f, 4.0f, 1.0f);
                break;

            case Icon::tilt:   // a rising slope over a base line (spectral / analyser tilt)
                p.startNewSubPath (4.0f, 19.0f); p.lineTo (20.0f, 19.0f);
                p.startNewSubPath (4.0f, 15.5f); p.lineTo (20.0f, 5.0f);
                p.startNewSubPath (16.0f, 5.0f); p.lineTo (20.0f, 5.0f); p.lineTo (20.0f, 9.0f);
                break;

            case Icon::headphones:
                p.startNewSubPath (5.0f, 15.0f); p.lineTo (5.0f, 12.0f);
                p.cubicTo (5.0f, 3.0f, 19.0f, 3.0f, 19.0f, 12.0f); p.lineTo (19.0f, 15.0f);
                p.addRoundedRectangle (4.0f, 13.5f, 4.0f, 6.0f, 1.5f);
                p.addRoundedRectangle (16.0f, 13.5f, 4.0f, 6.0f, 1.5f);
                break;
        }

        return p;
    }

    void draw (juce::Graphics& g, Icon icon, juce::Rectangle<float> area, juce::Colour colour, float strokeWidth)
    {
        auto p = path (icon);
        const float side = juce::jmin (area.getWidth(), area.getHeight());
        const float scale = side / 24.0f;
        p.applyTransform (juce::AffineTransform::scale (scale)
                            .translated (area.getCentreX() - 12.0f * scale, area.getCentreY() - 12.0f * scale));
        g.setColour (colour);
        g.strokePath (p, juce::PathStrokeType (strokeWidth * juce::jmax (0.75f, scale),
                                               juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

//==============================================================================
namespace paint
{
    void panel (juce::Graphics& g, juce::Rectangle<float> r, float radius, juce::Colour base, bool raised)
    {
        const auto& t = Theme::get();

        juce::ColourGradient grad (base.brighter (raised ? 0.05f : 0.0f), r.getX(), r.getY(),
                                   base.darker (raised ? 0.10f : 0.0f), r.getX(), r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, radius);

        if (raised)
        {
            // top sheen
            juce::Path sheen;
            sheen.addRoundedRectangle (r.reduced (0.5f), radius);
            g.saveState();
            g.reduceClipRegion (r.withHeight (juce::jmin (r.getHeight(), radius * 3.0f)).toNearestInt());
            g.setGradientFill (juce::ColourGradient (t.highlight, r.getX(), r.getY(), juce::Colours::transparentWhite,
                                                     r.getX(), r.getY() + radius * 3.0f, false));
            g.strokePath (sheen, juce::PathStrokeType (1.0f));
            g.restoreState();
        }

        g.setColour (t.stroke.withAlpha (0.9f));
        g.drawRoundedRectangle (r.reduced (0.5f), radius, t.hairline);
    }

    void glass (juce::Graphics& g, juce::Rectangle<float> r, float radius, juce::Colour tint, float opacity)
    {
        const auto& t = Theme::get();
        softShadow (g, r, radius, 18.0f, 0.55f);

        const auto base = t.bg2.interpolatedWith (tint.isTransparent() ? t.bg2 : tint, 0.06f);
        g.setGradientFill (juce::ColourGradient (base.brighter (0.06f).withAlpha (opacity), r.getX(), r.getY(),
                                                 base.darker (0.15f).withAlpha (juce::jmin (1.0f, opacity + 0.06f)), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, radius);

        // Edge light: brighter along the top, fading out downwards.
        juce::Path edge;
        edge.addRoundedRectangle (r.reduced (0.5f), radius);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.13f), r.getX(), r.getY(),
                                                 juce::Colours::white.withAlpha (0.025f), r.getX(), r.getY() + juce::jmin (r.getHeight(), 60.0f), false));
        g.strokePath (edge, juce::PathStrokeType (1.0f));

        if (! tint.isTransparent())
        {
            // faint coloured glow along the top edge
            g.setGradientFill (juce::ColourGradient (tint.withAlpha (0.10f), r.getCentreX(), r.getY(),
                                                     tint.withAlpha (0.0f), r.getCentreX(), r.getY() + 46.0f, false));
            g.fillRoundedRectangle (r.withHeight (juce::jmin (r.getHeight(), 46.0f)), radius);
        }
    }

    void well (juce::Graphics& g, juce::Rectangle<float> r, float radius)
    {
        const auto& t = Theme::get();
        g.setGradientFill (juce::ColourGradient (t.bg0.darker (0.2f), r.getX(), r.getY(),
                                                 t.bg1, r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, radius);

        // inner top shadow
        g.saveState();
        juce::Path clip;
        clip.addRoundedRectangle (r, radius);
        g.reduceClipRegion (clip);
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.45f), r.getX(), r.getY(),
                                                 juce::Colours::transparentBlack, r.getX(), r.getY() + 10.0f, false));
        g.fillRect (r.withHeight (10.0f));
        g.restoreState();

        g.setColour (t.strokeSoft);
        g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.03f));
        g.drawLine (r.getX() + radius, r.getBottom() - 0.5f, r.getRight() - radius, r.getBottom() - 0.5f, 1.0f);
    }

    void softShadow (juce::Graphics& g, juce::Rectangle<float> r, float radius, float spread, float alpha)
    {
        const int steps = 6;
        for (int i = steps; i >= 1; --i)
        {
            const float f = (float) i / (float) steps;
            g.setColour (juce::Colours::black.withAlpha (alpha * (1.0f - f) * (1.0f - f) * 0.5f));
            g.fillRoundedRectangle (r.expanded (spread * f).translated (0.0f, spread * 0.35f * f), radius + spread * f);
        }
    }

    void glowStroke (juce::Graphics& g, const juce::Path& p, juce::Colour c, float width, float glow)
    {
        const int layers = 4;
        for (int i = layers; i >= 1; --i)
        {
            const float f = (float) i / (float) layers;
            g.setColour (c.withMultipliedAlpha (0.07f * (1.0f - f * 0.5f)));
            g.strokePath (p, juce::PathStrokeType (width + glow * f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void greyButton (juce::Graphics& g, juce::Rectangle<float> r, float radius, bool hover, bool down, bool on)
    {
        const auto& t = Theme::get();
        r = r.withTrimmedBottom (2.0f).reduced (0.5f, 0.0f);   // room for the shadow inside our bounds
        radius = juce::jmin (radius, r.getHeight() * 0.5f);
        auto top = t.buttonTop, bottom = t.buttonBottom;
        if (hover) { top = top.brighter (0.12f); bottom = bottom.brighter (0.10f); }
        if (down)  { std::swap (top, bottom); }

        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillRoundedRectangle (r.translated (0.0f, 1.5f), radius);
        g.setGradientFill (juce::ColourGradient (top, 0.0f, r.getY(), bottom, 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, radius);
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        g.drawLine (r.getX() + radius, r.getY() + 0.75f, r.getRight() - radius, r.getY() + 0.75f, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawRoundedRectangle (r.reduced (0.25f), radius, 0.75f);

        if (on)
        {
            g.setColour (t.accent.withAlpha (0.35f));
            g.fillRoundedRectangle (r, radius);
        }
    }

    void pillHandle (juce::Graphics& g, juce::Rectangle<float> r, bool hover, bool active)
    {
        const auto& t = Theme::get();
        const float rad = juce::jmin (r.getHeight(), r.getWidth()) * 0.32f;

        // shadow
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (r.translated (0.0f, 1.5f).expanded (0.5f), rad + 0.5f);

        auto top = t.knobLight.darker (0.12f), bottom = t.knobDark;
        if (hover || active) { top = top.brighter (0.15f); bottom = bottom.brighter (0.12f); }
        g.setGradientFill (juce::ColourGradient (top, 0.0f, r.getY(), bottom, 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, rad);

        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawLine (r.getX() + rad, r.getY() + 0.8f, r.getRight() - rad, r.getY() + 0.8f, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawRoundedRectangle (r.reduced (0.25f), rad, 0.75f);

        // centre dash
        const bool vertical = r.getHeight() > r.getWidth();
        auto c = r.getCentre();
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        if (vertical) g.fillRect (c.x - 0.75f, c.y - r.getHeight() * 0.28f, 1.5f, r.getHeight() * 0.56f);
        else          g.fillRect (c.x - r.getWidth() * 0.28f, c.y - 0.5f, r.getWidth() * 0.56f, 1.25f);
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        if (vertical) g.fillRect (c.x + 0.75f, c.y - r.getHeight() * 0.28f, 0.75f, r.getHeight() * 0.56f);
        else          g.fillRect (c.x - r.getWidth() * 0.28f, c.y + 0.75f, r.getWidth() * 0.56f, 0.75f);
    }

    void knobDisc (juce::Graphics& g, juce::Point<float> c, float R, float angle, bool hover)
    {
        const auto& t = Theme::get();
        auto body = juce::Rectangle<float> (R * 2.0f, R * 2.0f).withCentre (c);

        // drop shadow
        for (int i = 3; i >= 1; --i)
        {
            const float f = (float) i / 3.0f;
            g.setColour (juce::Colours::black.withAlpha (0.18f * (1.0f - f) + 0.05f));
            g.fillEllipse (body.expanded (R * 0.10f * f).translated (0.0f, R * 0.12f * f));
        }

        auto light = t.knobLight, dark = t.knobDark;
        if (hover) { light = light.brighter (0.08f); dark = dark.brighter (0.08f); }
        g.setGradientFill (juce::ColourGradient (light, c.x - R * 0.5f, c.y - R, dark, c.x + R * 0.4f, c.y + R, false));
        g.fillEllipse (body);

        // subtle concave face
        g.setGradientFill (juce::ColourGradient (dark.withAlpha (0.25f), c.x, c.y - R, light.withAlpha (0.18f), c.x, c.y + R, false));
        g.fillEllipse (body.reduced (R * 0.14f));

        // rim
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawEllipse (body.reduced (0.4f), 0.9f);
        g.setColour (juce::Colours::white.withAlpha (0.28f));
        juce::Path rimLight;
        rimLight.addCentredArc (c.x, c.y, R - 1.0f, R - 1.0f, 0.0f, -1.2f, 1.2f, true);
        g.strokePath (rimLight, juce::PathStrokeType (0.9f));

        // white crescent pointer: a bright circle near the rim, clipped to the body
        g.saveState();
        juce::Path clip;
        clip.addEllipse (body.reduced (0.8f));
        g.reduceClipRegion (clip);
        const auto dot = c.getPointOnCircumference (R * 0.86f, angle);
        g.setColour (juce::Colours::white.withAlpha (0.96f));
        g.fillEllipse (juce::Rectangle<float> (R * 0.78f, R * 0.78f).withCentre (dot));
        g.restoreState();

        // fine dark tick through the crescent
        const auto p0 = c.getPointOnCircumference (R * 0.62f, angle);
        const auto p1 = c.getPointOnCircumference (R * 0.94f, angle);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawLine ({ p0, p1 }, juce::jmax (1.0f, R * 0.055f));
    }

    void connectorDot (juce::Graphics& g, juce::Point<float> c, float r, juce::Colour col, bool hover)
    {
        g.setColour (col.withAlpha (hover ? 0.35f : 0.18f));
        g.fillEllipse (juce::Rectangle<float> (r * 2.8f, r * 2.8f).withCentre (c));
        g.setColour (juce::Colour (0xff141015));
        g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
        g.setColour (col);
        g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c).reduced (0.6f), juce::jmax (1.2f, r * 0.28f));
        g.fillEllipse (juce::Rectangle<float> (r * 0.85f, r * 0.85f).withCentre (c));
    }

    void lightTrail (juce::Graphics& g, const juce::Path& p, juce::Colour col, float width)
    {
        const int layers = 7;
        for (int i = layers; i >= 1; --i)
        {
            const float f = (float) i / (float) layers;
            g.setColour (col.withAlpha (0.045f * (1.2f - f)));
            g.strokePath (p, juce::PathStrokeType (width * (0.4f + 2.6f * f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    void fadedLine (juce::Graphics& g, juce::Point<float> a, juce::Point<float> b, juce::Colour c, float thickness, float fade)
    {
        fade = juce::jlimit (0.001f, 0.5f, fade);
        juce::ColourGradient grad (c.withAlpha (0.0f), a, c.withAlpha (0.0f), b, false);
        grad.addColour (fade, c);
        grad.addColour (1.0 - fade, c);
        g.setGradientFill (grad);
        juce::Path p;
        p.startNewSubPath (a);
        p.lineTo (b);
        g.strokePath (p, juce::PathStrokeType (thickness, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
    }

    void tray (juce::Graphics& g, juce::Rectangle<float> r, float cornerRadius)
    {
        juce::Path bar;
        bar.addRoundedRectangle (r.getX() + 6.0f, r.getY(), r.getWidth() - 12.0f, r.getHeight() + cornerRadius + 2.0f,
                                 cornerRadius, cornerRadius, true, true, false, false);
        g.setColour (juce::Colours::black.withAlpha (0.42f));
        g.fillPath (bar);
        fadedLine (g, { r.getX() + 24.0f, r.getY() + 0.5f }, { r.getRight() - 24.0f, r.getY() + 0.5f },
                   juce::Colours::white.withAlpha (0.09f), 1.0f, 0.15f);
    }

    void caption (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, juce::Colour c,
                  juce::Justification j, float height)
    {
        g.setColour (c);
        g.setFont (fonts::caption (height));
        g.drawText (s.toUpperCase(), r, j, false);
    }
}

//==============================================================================
LookAndFeel::LookAndFeel()
{
    const auto& t = Theme::get();

    setColour (juce::ResizableWindow::backgroundColourId, t.bg0);
    setColour (juce::DocumentWindow::backgroundColourId, t.bg0);

    setColour (juce::PopupMenu::backgroundColourId, t.bg2);
    setColour (juce::PopupMenu::textColourId, t.text);
    setColour (juce::PopupMenu::headerTextColourId, t.textFaint);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, t.bg4);
    setColour (juce::PopupMenu::highlightedTextColourId, t.text);

    setColour (juce::ComboBox::backgroundColourId, t.bg3);
    setColour (juce::ComboBox::textColourId, t.text);
    setColour (juce::ComboBox::outlineColourId, t.stroke);
    setColour (juce::ComboBox::arrowColourId, t.textDim);

    setColour (juce::TextButton::buttonColourId, t.bg3);
    setColour (juce::TextButton::buttonOnColourId, t.accent);
    setColour (juce::TextButton::textColourOffId, t.textDim);
    setColour (juce::TextButton::textColourOnId, t.bg0);

    setColour (juce::ToggleButton::textColourId, t.textDim);
    setColour (juce::ToggleButton::tickColourId, t.accent);

    setColour (juce::Label::textColourId, t.text);
    setColour (juce::Label::textWhenEditingColourId, t.text);
    setColour (juce::Label::backgroundWhenEditingColourId, t.bg3);
    setColour (juce::Label::outlineWhenEditingColourId, t.accent);

    setColour (juce::TextEditor::backgroundColourId, t.bg3);
    setColour (juce::TextEditor::textColourId, t.text);
    setColour (juce::TextEditor::highlightColourId, t.accent.withAlpha (0.3f));
    setColour (juce::TextEditor::highlightedTextColourId, t.text);
    setColour (juce::TextEditor::outlineColourId, t.stroke);
    setColour (juce::TextEditor::focusedOutlineColourId, t.accent.withAlpha (0.7f));
    setColour (juce::CaretComponent::caretColourId, t.accent);

    setColour (juce::ScrollBar::thumbColourId, t.bg4);
    setColour (juce::TooltipWindow::backgroundColourId, t.bg3);
    setColour (juce::TooltipWindow::textColourId, t.text);
    setColour (juce::TooltipWindow::outlineColourId, t.stroke);

    setColour (juce::AlertWindow::backgroundColourId, t.bg2);
    setColour (juce::AlertWindow::textColourId, t.text);
    setColour (juce::AlertWindow::outlineColourId, t.stroke);

    setColour (juce::ListBox::backgroundColourId, t.bg1);
    setColour (juce::ListBox::outlineColourId, t.stroke);

    auto scheme = getCurrentColourScheme();
    scheme.setUIColour (ColourScheme::windowBackground, t.bg0);
    scheme.setUIColour (ColourScheme::widgetBackground, t.bg3);
    scheme.setUIColour (ColourScheme::menuBackground, t.bg2);
    scheme.setUIColour (ColourScheme::outline, t.stroke);
    scheme.setUIColour (ColourScheme::defaultText, t.text);
    scheme.setUIColour (ColourScheme::defaultFill, t.accent);
    scheme.setUIColour (ColourScheme::highlightedText, t.text);
    scheme.setUIColour (ColourScheme::highlightedFill, t.bg4);
    scheme.setUIColour (ColourScheme::menuText, t.text);
    setColourScheme (scheme);
}

juce::Typeface::Ptr LookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    if (f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName()
        || f.getTypefaceName() == "<Sans-Serif>" || f.getTypefaceName().isEmpty())
        return fonts::typeface (f.isBold() ? fonts::Weight::semibold : fonts::Weight::regular);

    return LookAndFeel_V4::getTypefaceForFont (f);
}

//==============================================================================
void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    const auto& t = Theme::get();
    auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
    g.fillAll (t.bg2);
    g.setColour (t.stroke);
    g.drawRect (r, 1.0f);
}

void LookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator,
                                     bool isActive, bool isHighlighted, bool isTicked, bool hasSubMenu,
                                     const juce::String& text, const juce::String& shortcutKeyText,
                                     const juce::Drawable*, const juce::Colour* textColour)
{
    const auto& t = Theme::get();

    if (isSeparator)
    {
        auto r = area.toFloat().reduced (8.0f, 0.0f);
        g.setColour (t.stroke);
        g.fillRect (r.withSizeKeepingCentre (r.getWidth(), 1.0f));
        return;
    }

    auto r = area.toFloat().reduced (4.0f, 1.0f);

    if (isHighlighted && isActive)
    {
        g.setColour (t.bg4);
        g.fillRoundedRectangle (r, 5.0f);
    }

    auto content = r.reduced (10.0f, 0.0f);
    auto tickArea = content.removeFromLeft (16.0f);

    if (isTicked)
    {
        g.setColour (t.accent);
        g.fillEllipse (tickArea.withSizeKeepingCentre (6.0f, 6.0f));
    }

    juce::Colour col = textColour != nullptr ? *textColour : t.text;
    if (! isActive) col = t.textFaint;
    else if (isTicked) col = t.text;
    else if (! isHighlighted) col = col.withMultipliedAlpha (0.88f);

    g.setColour (col);
    g.setFont (fonts::medium (13.0f));

    if (hasSubMenu)
    {
        icons::draw (g, icons::Icon::chevronRight, content.removeFromRight (14.0f).withSizeKeepingCentre (12.0f, 12.0f), t.textDim, 1.8f);
        g.setColour (col);
    }

    if (shortcutKeyText.isNotEmpty())
    {
        g.setFont (fonts::regular (12.0f));
        g.setColour (t.textFaint);
        g.drawText (shortcutKeyText, content, juce::Justification::centredRight, true);
        g.setColour (col);
        g.setFont (fonts::medium (13.0f));
    }

    g.drawFittedText (text, content.toNearestInt(), juce::Justification::centredLeft, 1);
}

void LookAndFeel::drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& name)
{
    const auto& t = Theme::get();
    paint::caption (g, name, area.toFloat().reduced (14.0f, 0.0f).withTrimmedTop (4.0f), t.textFaint,
                    juce::Justification::bottomLeft, 10.0f);
}

juce::Font LookAndFeel::getPopupMenuFont() { return fonts::medium (13.0f); }

void LookAndFeel::getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator, int standardMenuItemHeight,
                                             int& idealWidth, int& idealHeight)
{
    if (isSeparator)
    {
        idealWidth = 50;
        idealHeight = 9;
        return;
    }

    idealHeight = standardMenuItemHeight > 0 ? standardMenuItemHeight : 26;
    idealWidth = (int) fonts::stringWidth (getPopupMenuFont(), text) + 60;
}

//==============================================================================
void LookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto& t = Theme::get();
    auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
    const bool hover = box.isMouseOver (true);

    g.setColour (hover ? t.bg4 : t.bg3);
    g.fillRoundedRectangle (r, t.radius - 2.0f);
    g.setColour (box.hasKeyboardFocus (true) ? t.accent.withAlpha (0.6f) : t.stroke);
    g.drawRoundedRectangle (r, t.radius - 2.0f, 1.0f);

    auto arrow = r.removeFromRight ((float) height).reduced ((float) height * 0.3f);
    icons::draw (g, icons::Icon::chevronDown, arrow, hover ? t.text : t.textDim, 1.8f);
}

juce::Font LookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    return fonts::medium (juce::jlimit (11.0f, 15.0f, (float) box.getHeight() * 0.48f));
}

void LookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - box.getHeight() - 6, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

//==============================================================================
void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto& t = Theme::get();
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    const auto onColour = b.findColour (juce::TextButton::buttonOnColourId);

    juce::Colour fill = on ? onColour : (down ? t.bg4.brighter (0.05f) : over ? t.bg4 : t.bg3);
    g.setColour (fill);
    g.fillRoundedRectangle (r, t.radius - 2.0f);

    if (! on)
    {
        g.setColour (t.stroke);
        g.drawRoundedRectangle (r, t.radius - 2.0f, 1.0f);
    }
}

juce::Font LookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return fonts::semibold (juce::jlimit (10.0f, 14.0f, (float) buttonHeight * 0.45f));
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const auto& t = Theme::get();
    auto r = b.getLocalBounds().toFloat();
    auto sw = r.removeFromLeft (30.0f).withSizeKeepingCentre (26.0f, 14.0f);
    const bool on = b.getToggleState();

    g.setColour (on ? t.accent.withAlpha (0.9f) : (highlighted ? t.bg4 : t.bg3));
    g.fillRoundedRectangle (sw, 7.0f);
    g.setColour (t.stroke);
    if (! on) g.drawRoundedRectangle (sw, 7.0f, 1.0f);

    auto knob = sw.withWidth (10.0f).withSizeKeepingCentre (10.0f, 10.0f).withX (on ? sw.getRight() - 12.0f : sw.getX() + 2.0f);
    g.setColour (on ? t.bg0 : t.textDim);
    g.fillEllipse (knob);

    g.setColour (on ? t.text : t.textDim);
    g.setFont (fonts::medium (12.5f));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (4.0f), juce::Justification::centredLeft, true);
}

//==============================================================================
void LookAndFeel::fillTextEditorBackground (juce::Graphics& g, int width, int height, juce::TextEditor& ed)
{
    g.setColour (ed.findColour (juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle (juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height), 5.0f);
}

void LookAndFeel::drawTextEditorOutline (juce::Graphics& g, int width, int height, juce::TextEditor& ed)
{
    if (! ed.isEnabled()) return;
    const bool focus = ed.hasKeyboardFocus (true) && ! ed.isReadOnly();
    g.setColour (ed.findColour (focus ? juce::TextEditor::focusedOutlineColourId : juce::TextEditor::outlineColourId));
    g.drawRoundedRectangle (juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f), 5.0f, 1.0f);
}

juce::Font LookAndFeel::getLabelFont (juce::Label& l)
{
    auto f = l.getFont();
    return fonts::make (f.getHeight(), f.isBold() ? fonts::Weight::semibold : fonts::Weight::regular);
}

//==============================================================================
juce::Rectangle<int> LookAndFeel::getTooltipBounds (const juce::String& tip, juce::Point<int> pos, juce::Rectangle<int> parent)
{
    juce::AttributedString s;
    s.append (tip, fonts::medium (12.0f), Theme::get().text);
    juce::TextLayout tl;
    tl.createLayout (s, 280.0f);
    const int w = (int) tl.getWidth() + 20, h = (int) tl.getHeight() + 12;

    return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 18,
                                 pos.y > parent.getCentreY() ? pos.y - (h + 8) : pos.y + 8, w, h)
             .constrainedWithin (parent);
}

void LookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    const auto& t = Theme::get();
    auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
    g.setColour (t.bg3);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (t.stroke);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

    juce::AttributedString s;
    s.setJustification (juce::Justification::centredLeft);
    s.append (text, fonts::medium (12.0f), t.text);
    juce::TextLayout tl;
    tl.createLayout (s, (float) width - 20.0f);
    tl.draw (g, r.reduced (10.0f, 6.0f));
}

//==============================================================================
void LookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int width, int height, bool vertical,
                                 int thumbStart, int thumbSize, bool over, bool down)
{
    const auto& t = Theme::get();
    juce::Rectangle<float> thumb = vertical ? juce::Rectangle<float> ((float) x, (float) thumbStart, (float) width, (float) thumbSize)
                                            : juce::Rectangle<float> ((float) thumbStart, (float) y, (float) thumbSize, (float) height);
    thumb = vertical ? thumb.reduced (3.0f, 2.0f) : thumb.reduced (2.0f, 3.0f);
    g.setColour (down ? t.textFaint : over ? t.bg4.brighter (0.2f) : t.bg4);
    g.fillRoundedRectangle (thumb, juce::jmin (thumb.getWidth(), thumb.getHeight()) * 0.5f);
}

void LookAndFeel::drawCornerResizer (juce::Graphics& g, int w, int h, bool over, bool dragging)
{
    const auto& t = Theme::get();
    icons::draw (g, icons::Icon::resize, juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h),
                 (over || dragging) ? t.textDim : t.textFaint, 1.4f);
}

//==============================================================================
void LookAndFeel::drawAlertBox (juce::Graphics& g, juce::AlertWindow& w, const juce::Rectangle<int>& textArea, juce::TextLayout& tl)
{
    const auto& t = Theme::get();
    auto r = w.getLocalBounds().toFloat();
    g.setColour (t.bg2);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (t.stroke);
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
    tl.draw (g, textArea.toFloat());
}

juce::Font LookAndFeel::getAlertWindowTitleFont()   { return fonts::semibold (16.0f); }
juce::Font LookAndFeel::getAlertWindowMessageFont() { return fonts::regular (13.0f); }
juce::Font LookAndFeel::getAlertWindowFont()        { return fonts::medium (13.0f); }

} // namespace st
