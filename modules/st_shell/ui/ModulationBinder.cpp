namespace st
{

ModulationBinder::ModulationBinder (mod::ModMatrix& m, juce::Component& r, ModulationPanel* p)
    : matrix (m), root (&r)
{
    attachPanel (p);
    refresh();
    startTimerHz (2);
}

ModulationBinder::~ModulationBinder()
{
    stopTimer();
    *alive = false;

    if (panel != nullptr)
        panel->onSourceDrag = nullptr;

    for (auto& b : bindings)
        unbind (b);
}

void ModulationBinder::attachPanel (ModulationPanel* p)
{
    if (panel != nullptr)
        panel->onSourceDrag = nullptr;

    panel = p;

    if (p != nullptr)
        p->onSourceDrag = [this] (int i) { setDragSource (i); };
}

void ModulationBinder::setAutoRefresh (bool b)
{
    autoRefresh = b;
}

void ModulationBinder::refresh()
{
    bindings.erase (std::remove_if (bindings.begin(), bindings.end(), [] (const Binding& b) { return b.knob == nullptr; }),
                    bindings.end());

    if (root == nullptr)
        return;

    std::vector<juce::Component*> stack { root.getComponent() };
    while (! stack.empty())
    {
        auto* c = stack.back();
        stack.pop_back();

        if (dynamic_cast<ModulationPanel*> (c) != nullptr)
            continue;

        if (auto* knob = dynamic_cast<Knob*> (c))
        {
            const auto id = knob->getParameter().getParameterID();
            const bool known = std::any_of (bindings.begin(), bindings.end(), [knob] (const Binding& b) { return b.knob == knob; });
            if (! known && matrix.isTarget (id))
                bind (*knob, id);
        }

        for (auto* child : c->getChildren())
            stack.push_back (child);
    }
}

void ModulationBinder::bind (Knob& knob, const juce::String& id)
{
    Binding b;
    b.knob = &knob;
    b.paramId = id;
    b.previousMenu = knob.extendContextMenu;

    knob.setModulationSource ([m = &matrix, k = &knob, id] { return m->getDisplay (id, k->getNormalised()); });
    knob.onModulationDropped = [this, id] (int src) { createRouting (src, id); };
    knob.extendContextMenu = [this, prev = b.previousMenu, id] (juce::PopupMenu& menu)
    {
        if (prev) prev (menu);
        addMenuItems (menu, id);
    };

    if (dragSource >= 0)
        knob.setModulationDropHint (true, panel != nullptr ? panel->getSourceColour (dragSource) : juce::Colour());

    bindings.push_back (std::move (b));
}

void ModulationBinder::unbind (Binding& b)
{
    if (auto* k = b.knob.getComponent())
    {
        k->setModulationSource ({});
        k->onModulationDropped = nullptr;
        k->extendContextMenu = b.previousMenu;
        k->setModulationDropHint (false);
    }
}

void ModulationBinder::createRouting (int sourceIndex, const juce::String& paramId)
{
    const auto src = matrix.getSourceId (sourceIndex);
    if (! matrix.isValidSource (src))
        return;

    const bool existed = matrix.getRouting (src, paramId).has_value();
    if (! existed && matrix.addRouting (src, paramId, defaultDepth) && onRoutingCreated)
        onRoutingCreated (src, paramId);

    if (panel != nullptr)
        panel->selectSource (sourceIndex);

    setDragSource (-1);
}

void ModulationBinder::setDragSource (int i)
{
    dragSource = i;
    const auto c = (i >= 0 && panel != nullptr) ? panel->getSourceColour (i) : juce::Colour();

    for (auto& b : bindings)
        if (auto* k = b.knob.getComponent())
            k->setModulationDropHint (i >= 0, c);
}

void ModulationBinder::addMenuItems (juce::PopupMenu& menu, const juce::String& paramId)
{
    const auto routings = matrix.getRoutingsForTarget (paramId);
    auto* m = &matrix;
    std::weak_ptr<bool> weakAlive = alive;

    menu.addSeparator();
    menu.addSectionHeader ("Modulation");

    auto colourFor = [this] (mod::SourceId s)
    {
        return Theme::get().seriesColour (matrix.getSourceColourIndex (s));
    };

    for (auto& r : routings)
    {
        const auto label = "Remove " + matrix.getSourceName (r.source) + "  ("
                         + params::formatPercent (r.depth * 100.0f, true) + (r.enabled ? ")" : ", off)");
        juce::PopupMenu::Item item (label);
        item.colour = colourFor (r.source);
        item.action = [m, s = r.source, paramId] { m->removeRouting (s, paramId); };
        menu.addItem (std::move (item));
    }

    if (routings.size() > 1)
        menu.addItem ("Remove all modulation", [m, paramId] { m->removeAllRoutingsForTarget (paramId); });

    juce::PopupMenu add;
    const bool full = matrix.getNumRoutings() >= matrix.getConfig().maxRoutings;
    for (int i = 0; i < matrix.getNumSources(); ++i)
    {
        const auto s = matrix.getSourceId (i);
        const bool routed = std::any_of (routings.begin(), routings.end(), [s] (const mod::Routing& r) { return r.source == s; });

        juce::PopupMenu::Item item (matrix.getSourceName (s));
        item.colour = colourFor (s);
        item.isTicked = routed;
        item.isEnabled = ! routed && ! full;
        item.action = [weakAlive, this, i, paramId]
        {
            if (auto a = weakAlive.lock(); a != nullptr && *a)
                createRouting (i, paramId);
        };
        add.addItem (std::move (item));
    }

    menu.addSubMenu (full ? "Modulate with (all slots used)" : "Modulate with", add, ! full || ! routings.empty());
}

void ModulationBinder::timerCallback()
{
    if (autoRefresh)
        refresh();

    // Safety net: clear drop hints if a drag ended without the panel noticing.
    if (dragSource >= 0 && root != nullptr)
    {
        auto* container = dynamic_cast<juce::DragAndDropContainer*> (root.getComponent());
        if (container == nullptr)
            container = juce::DragAndDropContainer::findParentDragContainerFor (root.getComponent());
        if (container != nullptr && ! container->isDragAndDropActive())
            setDragSource (-1);
    }
}

} // namespace st
