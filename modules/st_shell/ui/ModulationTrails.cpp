namespace st
{
namespace trails_detail
{
    constexpr float downsample = 1.0f / 3.0f;   // glow buffers are rendered at a third of design resolution
    constexpr int blurRadius = 4, blurPasses = 3; // ~3 box passes approximate a gaussian
    constexpr double updatePeriod = 1.0 / 30.0;

    /** In-place box blur of one line of premultiplied ARGB pixels (outside the line counts as transparent). */
    static void blurLine (juce::uint8* p, int n, int step, int r, std::vector<juce::uint8>& src)
    {
        src.resize ((size_t) n * 4);
        for (int i = 0; i < n; ++i)
            std::memcpy (&src[(size_t) i * 4], p + (size_t) i * (size_t) step, 4);

        int sum[4] = { 0, 0, 0, 0 };
        const int win = 2 * r + 1;
        for (int k = 0; k <= r && k < n; ++k)
            for (int c = 0; c < 4; ++c)
                sum[c] += src[(size_t) k * 4 + (size_t) c];

        for (int i = 0; i < n; ++i)
        {
            auto* out = p + (size_t) i * (size_t) step;
            for (int c = 0; c < 4; ++c)
                out[c] = (juce::uint8) (sum[c] / win);

            const int add = i + r + 1, rem = i - r;
            if (add < n) for (int c = 0; c < 4; ++c) sum[c] += src[(size_t) add * 4 + (size_t) c];
            if (rem >= 0) for (int c = 0; c < 4; ++c) sum[c] -= src[(size_t) rem * 4 + (size_t) c];
        }
    }

    static void boxBlur (juce::Image& img, int radius, int passes)
    {
        juce::Image::BitmapData d (img, juce::Image::BitmapData::readWrite);
        if (d.pixelFormat != juce::Image::ARGB)
            return;

        std::vector<juce::uint8> scratch;
        for (int pass = 0; pass < passes; ++pass)
        {
            for (int y = 0; y < d.height; ++y)
                blurLine (d.getLinePointer (y), d.width, d.pixelStride, radius, scratch);
            for (int x = 0; x < d.width; ++x)
                blurLine (d.getPixelPointer (x, 0), d.height, d.lineStride, radius, scratch);
        }
    }

    struct Hasher
    {
        std::uint64_t h = 1469598103934665603ull;
        void add (std::int64_t v) noexcept
        {
            for (int i = 0; i < 8; ++i)
            {
                h ^= (std::uint64_t) ((v >> (i * 8)) & 0xff);
                h *= 1099511628211ull;
            }
        }
        void add (juce::Point<float> p) noexcept { add ((std::int64_t) std::lround (p.x * 2.0f)); add ((std::int64_t) std::lround (p.y * 2.0f)); }
    };
}

using namespace trails_detail;

bool ModulationTrails::isVisibleWithin (const juce::Component& c) const
{
    for (auto* p = &c; p != nullptr && p != root.getComponent(); p = p->getParentComponent())
        if (! p->isVisible())
            return false;
    return true;
}

//==============================================================================
ModulationTrails::ModulationTrails (mod::ModMatrix& m, ModulationStrip& s, juce::Component& r)
    : matrix (m), strip (&s), root (&r)
{
    setInterceptsMouseClicks (false, false);
    setPaintingIsUnclipped (false);
    r.addAndMakeVisible (this);
    setBounds (r.getLocalBounds());
    r.addComponentListener (this);
    matrix.addChangeListener (this);
    clock = std::make_unique<FrameClock> (*this, [this] (double dt) { tick (dt); });
}

ModulationTrails::~ModulationTrails()
{
    clock.reset();
    matrix.removeChangeListener (this);
    if (auto* r = root.getComponent())
        r->removeComponentListener (this);
}

void ModulationTrails::setStrength (float s)
{
    strength = juce::jlimit (0.0f, 2.0f, s);
    for (auto& g : glows) g->requestedAlpha = -1.0f;
    repaint();
}

void ModulationTrails::setLiveResponse (float r)
{
    liveResponse = juce::jlimit (0.0f, 1.0f, r);
}

void ModulationTrails::refresh()
{
    knobs.clear();
    needsScan = false;
    sinceScan = 0.0;

    if (root == nullptr)
        return;

    std::vector<juce::Component*> stack { root.getComponent() };
    while (! stack.empty())
    {
        auto* c = stack.back();
        stack.pop_back();

        if (c == this || c == strip.getComponent() || dynamic_cast<ModulationPopover*> (c) != nullptr
            || dynamic_cast<ModulationPanel*> (c) != nullptr)
            continue;

        if (auto* k = dynamic_cast<Knob*> (c))
        {
            const auto id = k->getParameter().getParameterID();
            if (matrix.isTarget (id))
                knobs.push_back ({ k, id });
        }

        for (auto* child : c->getChildren())
            stack.push_back (child);
    }
    sinceUpdate = updatePeriod; // update geometry on the next frame
}

ModulationTrails::Glow& ModulationTrails::glowFor (int source)
{
    for (auto& g : glows)
        if (g->source == source)
            return *g;

    auto g = std::make_unique<Glow>();
    g->source = source;
    g->presence.snap (0.0f);
    g->level.snap (0.6f);
    glows.push_back (std::move (g));
    return *glows.back();
}

juce::Path ModulationTrails::makeTrailPath (const Link& l)
{
    const float dy = juce::jmax (48.0f, l.from.y - l.to.y);
    juce::Path p;
    p.startNewSubPath (l.from);
    p.cubicTo (l.from.x, l.from.y - dy * 0.5f, l.to.x, l.to.y + dy * 0.6f, l.to.x, l.to.y);
    return p;
}

void ModulationTrails::render (Glow& g)
{
    juce::Rectangle<float> area;
    auto include = [&area] (juce::Rectangle<float> r) { area = area.isEmpty() ? r : area.getUnion (r); };

    auto poolFor = [] (const Link& l)
    {
        const float w = juce::jmax (64.0f, l.dial.getWidth() * 1.55f);
        return juce::Rectangle<float> (w, 24.0f).withCentre ({ l.dial.getCentreX(), l.to.y + 17.0f });
    };

    for (auto& l : g.links)
    {
        include (makeTrailPath (l).getBounds());
        include (poolFor (l));
    }

    if (area.isEmpty())
    {
        g.image = {};
        return;
    }

    constexpr float margin = 40.0f;
    area = area.expanded (margin).getSmallestIntegerContainer().toFloat();
    const int w = (int) std::ceil (area.getWidth() * downsample) + 1;
    const int h = (int) std::ceil (area.getHeight() * downsample) + 1;

    juce::Image img (juce::Image::ARGB, w, h, true, juce::SoftwareImageType());
    {
        juce::Graphics gr (img);
        gr.addTransform (juce::AffineTransform::translation (-area.getX(), -area.getY()).scaled (downsample));
        const auto c = g.colour;

        for (auto& l : g.links)
        {
            const auto path = makeTrailPath (l);
            gr.setColour (c.withAlpha (0.14f));
            gr.strokePath (path, juce::PathStrokeType (30.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            gr.setColour (c.withAlpha (0.30f));
            gr.strokePath (path, juce::PathStrokeType (10.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // light pooled under the target control
            gr.setColour (c.withAlpha (0.46f));
            gr.fillEllipse (poolFor (l));
            gr.setColour (c.withAlpha (0.26f));
            gr.fillEllipse (juce::Rectangle<float> (22.0f, 22.0f).withCentre (l.to));
        }

        // a little light rising from the connector
        gr.setColour (c.withAlpha (0.3f));
        gr.fillEllipse (juce::Rectangle<float> (26.0f, 20.0f).withCentre (g.links.front().from.translated (0.0f, -4.0f)));
    }
    boxBlur (img, blurRadius, blurPasses);

    g.image = img;
    g.area = { area.getX(), area.getY(), (float) w / downsample, (float) h / downsample };
}

float ModulationTrails::alphaFor (const Glow& g) const
{
    float emphasis = 1.0f;
    if (strip != nullptr)
    {
        const bool focused = strip->getSelectedSource() == g.source
                          || (strip->getPopover() != nullptr && strip->getPopover()->getSourceIndex() == g.source);
        if (strip->getDraggingSource() == g.source) emphasis = 1.35f;
        else if (focused)                           emphasis = 1.15f;
    }
    return juce::jlimit (0.0f, 1.0f, strength * g.presence.current * g.level.current * emphasis);
}

void ModulationTrails::tick (double dt)
{
    sinceUpdate += dt;
    sinceScan += dt;
    updateDrag();

    if (sinceUpdate >= updatePeriod)
    {
        update (juce::jmin (sinceUpdate, 0.1));
        sinceUpdate = 0.0;
    }
}

void ModulationTrails::update (double dt)
{
    if (needsScan || sinceScan > 0.5)
        refresh();

    if (strip == nullptr)
        return;

    // 1. Current geometry, grouped by source.
    const auto routings = matrix.getRoutings();
    for (auto& g : glows) { g->links.clear(); g->live = false; }

    for (auto& r : routings)
    {
        if (! r.enabled)
            continue;
        const int src = matrix.getSourceIndex (r.source);
        if (src < 0)
            continue;
        const auto conn = strip->getConnectorPosition (src);
        if (! conn.has_value())
            continue;

        const auto from = getLocalPoint (strip.getComponent(), *conn - juce::Point<float> (0.0f, ModulationStrip::connectorRadius + 1.0f));
        for (auto& k : knobs)
        {
            auto* knob = k.knob.getComponent();
            if (knob == nullptr || k.paramId != r.targetId || ! isVisibleWithin (*knob))
                continue;

            auto& g = glowFor (src);
            g.live = true;
            g.links.push_back ({ from, getLocalPoint (knob, knob->getModulationAnchor()),
                                 getLocalArea (knob, knob->getDialBounds()), true });
        }
    }

    // 2. Re-render changed glows, animate levels, request minimal repaints.
    for (auto it = glows.begin(); it != glows.end();)
    {
        auto& g = **it;
        const auto oldArea = g.area;

        if (g.live)
        {
            g.colour = strip->getSourceColour (g.source);
            Hasher hs;
            hs.add ((std::int64_t) g.colour.getARGB());
            for (auto& l : g.links) { hs.add (l.from); hs.add (l.to); hs.add (l.dial.getCentre()); hs.add ((std::int64_t) l.dial.getWidth()); }

            if (hs.h != g.signature || ! g.image.isValid())
            {
                g.signature = hs.h;
                render (g);
                g.requestedAlpha = -1.0f;
                if (! oldArea.isEmpty()) repaint (oldArea.getSmallestIntegerContainer());
            }
        }

        g.presence.setTarget (g.live ? 1.0f : 0.0f);
        g.presence.tick (dt, g.live ? 5.0f : 7.0f);

        const float v = matrix.getSourceValueNormalised (g.source);
        g.level.setTarget (1.0f - liveResponse * 0.5f * (1.0f - juce::jlimit (0.0f, 1.0f, v)));
        g.level.tick (dt, 10.0f);

        if (! g.live && g.presence.current < 0.003f)
        {
            if (! g.area.isEmpty()) repaint (g.area.getSmallestIntegerContainer());
            it = glows.erase (it);
            continue;
        }

        const float a = alphaFor (g);
        if (std::abs (a - g.requestedAlpha) > 0.012f && ! g.area.isEmpty())
        {
            g.requestedAlpha = a;
            repaint (g.area.getSmallestIntegerContainer());
        }
        ++it;
    }
}

void ModulationTrails::updateDrag()
{
    const int src = strip != nullptr ? strip->getDraggingSource() : -1;
    const auto conn = src >= 0 ? strip->getConnectorPosition (src) : std::nullopt;

    if (src >= 0 && conn.has_value())
    {
        const auto from = getLocalPoint (strip.getComponent(), *conn - juce::Point<float> (0.0f, ModulationStrip::connectorRadius));
        const auto to = getMouseXYRelative().toFloat();
        const float dy = juce::jmax (36.0f, from.y - to.y);

        juce::Path p;
        p.startNewSubPath (from);
        p.cubicTo (from.x, from.y - dy * 0.5f, to.x, to.y + dy * 0.5f, to.x, to.y);

        const auto area = p.getBounds().expanded (24.0f);
        repaint ((dragArea.isEmpty() ? area : dragArea.getUnion (area)).getSmallestIntegerContainer());
        dragArea = area;
        dragPath = p;
        dragSource = src;
    }
    else if (dragSource >= 0)
    {
        repaint (dragArea.getSmallestIntegerContainer());
        dragSource = -1;
        dragPath.clear();
        dragArea = {};
    }
}

void ModulationTrails::paint (juce::Graphics& g)
{
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);

    for (auto& glow : glows)
    {
        const float a = alphaFor (*glow);
        if (a < 0.004f || ! glow->image.isValid())
            continue;

        g.setOpacity (a);
        g.drawImageTransformed (glow->image,
                                juce::AffineTransform::scale (glow->area.getWidth() / (float) glow->image.getWidth(),
                                                              glow->area.getHeight() / (float) glow->image.getHeight())
                                    .translated (glow->area.getX(), glow->area.getY()));
    }
    g.setOpacity (1.0f);

    // faint filament along each trail so connections read clearly without being garish
    for (auto& glow : glows)
    {
        const float a = alphaFor (*glow);
        if (a < 0.004f) continue;
        g.setColour (glow->colour.withAlpha (0.11f * a));
        for (auto& l : glow->links)
            g.strokePath (makeTrailPath (l), juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    if (dragSource >= 0 && strip != nullptr)
    {
        const auto c = strip->getSourceColour (dragSource);
        paint::lightTrail (g, dragPath, c, 7.0f);
        g.setColour (c.withAlpha (0.85f));
        g.strokePath (dragPath, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

void ModulationTrails::changeListenerCallback (juce::ChangeBroadcaster*)
{
    sinceUpdate = updatePeriod;
}

void ModulationTrails::componentMovedOrResized (juce::Component& c, bool, bool wasResized)
{
    if (&c == root.getComponent() && wasResized)
    {
        setBounds (c.getLocalBounds());
        sinceUpdate = updatePeriod;
    }
}

void ModulationTrails::componentChildrenChanged (juce::Component&)
{
    needsScan = true;
}

void ModulationTrails::componentBeingDeleted (juce::Component& c)
{
    c.removeComponentListener (this);
    knobs.clear();
}

} // namespace st
