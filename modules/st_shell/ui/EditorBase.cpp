namespace st
{

//==============================================================================
TextPrompt::TextPrompt (const juce::String& t, const juce::String& initial, const juce::String& confirmText,
                        std::function<void (const juce::String&)> onConfirm)
    : title (t), ok (confirmText), cancel ("Cancel"), confirm (std::move (onConfirm))
{
    addAndMakeVisible (editor);
    addAndMakeVisible (ok);
    addAndMakeVisible (cancel);

    editor.setText (initial, false);
    editor.setFont (fonts::medium (14.0f));
    editor.setIndents (10, 0);
    editor.setJustification (juce::Justification::centredLeft);
    editor.setSelectAllWhenFocused (true);
    editor.onReturnKey = [this] { close (true); };
    editor.onEscapeKey = [this] { close (false); };

    ok.setToggleState (true, juce::dontSendNotification);
    ok.onClick = [this] { close (true); };
    cancel.onClick = [this] { close (false); };
}

void TextPrompt::show (juce::Component& parent, const juce::String& t, const juce::String& initial,
                       const juce::String& confirmText, std::function<void (const juce::String&)> onConfirm)
{
    auto* p = new TextPrompt (t, initial, confirmText, std::move (onConfirm));
    parent.addAndMakeVisible (p);
    p->setBounds (parent.getLocalBounds());
    p->editor.grabKeyboardFocus();
    p->editor.selectAll();
}

void TextPrompt::paint (juce::Graphics& g)
{
    const auto& t = Theme::get();
    g.fillAll (juce::Colours::black.withAlpha (0.55f));
    paint::softShadow (g, panel.toFloat(), t.radiusLarge, 24.0f, 0.8f);
    paint::panel (g, panel.toFloat(), t.radiusLarge, t.bg2);

    g.setColour (t.text);
    g.setFont (fonts::semibold (15.0f));
    g.drawText (title, panel.reduced (20, 0).withHeight (52), juce::Justification::centredLeft, false);
}

void TextPrompt::resized()
{
    panel = getLocalBounds().withSizeKeepingCentre (360, 164);
    auto r = panel.reduced (20);
    r.removeFromTop (34);
    editor.setBounds (r.removeFromTop (36));
    r.removeFromTop (18);
    auto buttons = r.removeFromTop (30);
    ok.setBounds (buttons.removeFromRight (96));
    buttons.removeFromRight (8);
    cancel.setBounds (buttons.removeFromRight (86));
}

void TextPrompt::mouseDown (const juce::MouseEvent& e)
{
    if (! panel.contains (e.getPosition()))
        close (false);
}

void TextPrompt::close (bool accept)
{
    const auto text = editor.getText();
    auto cb = confirm;
    setVisible (false);

    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<TextPrompt> (this)]
    {
        if (safe != nullptr) delete safe.getComponent();
    });

    if (accept && cb && text.trim().isNotEmpty())
        cb (text.trim());
}

//==============================================================================
HeaderBar::HeaderBar (ProcessorBase& p) : proc (p), product (JucePlugin_Name)
{
    for (auto* b : { &undoButton, &redoButton, &prevButton, &nextButton, &settingsButton })
    {
        b->setMinimal (true);
        addAndMakeVisible (b);
    }
    for (auto* b : { &abButton, &copyButton, &saveButton })
        addAndMakeVisible (b);

    undoButton.onClick = [this] { proc.undoManager.undo(); };
    redoButton.onClick = [this] { proc.undoManager.redo(); };
    prevButton.onClick = [this] { proc.presets.loadPrevious(); };
    nextButton.onClick = [this] { proc.presets.loadNext(); };
    saveButton.onClick = [this] { promptSave(); };
    abButton.onClick = [this] { proc.ab.toggle(); };
    copyButton.onClick = [this] { proc.ab.copyActiveToOther(); };
    settingsButton.onClick = [this] { showSettingsMenu(); };

    proc.presets.addChangeListener (this);
    proc.ab.addChangeListener (this);
    refreshAB();
    startTimerHz (6);
}

HeaderBar::~HeaderBar()
{
    proc.presets.removeChangeListener (this);
    proc.ab.removeChangeListener (this);
}

void HeaderBar::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshAB();
    timerCallback();
    repaint();
}

void HeaderBar::refreshAB()
{
    const bool a = proc.ab.getActive() == 0;
    abButton.setButtonText (a ? "A" : "B");
    abButton.setDimmedText (a ? "" : "A/", a ? "/B" : "");
    copyButton.setTooltip (a ? "Copy A to B" : "Copy B to A");
}

void HeaderBar::timerCallback()
{
    undoButton.setEnabled (proc.undoManager.canUndo());
    redoButton.setEnabled (proc.undoManager.canRedo());

    const bool m = proc.presets.isModified();
    if (m != modified) { modified = m; repaint (presetArea); }
}

void HeaderBar::resized()
{
    auto r = getLocalBounds().withTrimmedTop (6).withTrimmedBottom (6);
    brandArea = r.removeFromLeft (140);

    r.removeFromRight (14);
    settingsButton.setBounds (r.removeFromRight (28).withSizeKeepingCentre (28, 28));

    // Centre group: undo redo   A/B Copy   < preset >   Save
    auto c = getLocalBounds().withTrimmedTop (6).withTrimmedBottom (6).withSizeKeepingCentre (600, r.getHeight());
    undoButton.setBounds (c.removeFromLeft (30).withSizeKeepingCentre (26, 26));
    c.removeFromLeft (6);
    redoButton.setBounds (c.removeFromLeft (30).withSizeKeepingCentre (26, 26));
    c.removeFromLeft (24);
    abButton.setBounds (c.removeFromLeft (38));
    c.removeFromLeft (6);
    copyButton.setBounds (c.removeFromLeft (44));
    c.removeFromLeft (24);
    saveButton.setBounds (c.removeFromRight (44));
    c.removeFromRight (10);
    prevButton.setBounds (c.removeFromLeft (24).withSizeKeepingCentre (22, 22));
    nextButton.setBounds (c.removeFromRight (24).withSizeKeepingCentre (22, 22));
    presetArea = c;
}

void HeaderBar::paint (juce::Graphics& g)
{
    const auto& t = Theme::get();

    // Small curved logo tab tucked into the top-left corner.
    {
        const float w = 132.0f, h = 30.0f;
        juce::Path tab;
        tab.startNewSubPath (0.0f, 0.0f);
        tab.lineTo (w, 0.0f);
        tab.cubicTo (w - 16.0f, 0.0f, w - 14.0f, h, w - 34.0f, h);
        tab.lineTo (12.0f, h);
        tab.quadraticTo (0.0f, h, 0.0f, h - 12.0f);
        tab.closeSubPath();

        g.setColour (juce::Colours::black.withAlpha (0.3f));
        g.fillPath (tab, juce::AffineTransform::translation (0.0f, 1.0f));
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.09f), 0.0f, 0.0f,
                                                 juce::Colours::white.withAlpha (0.04f), 0.0f, h, false));
        g.fillPath (tab);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.strokePath (tab, juce::PathStrokeType (0.8f));

        auto b = juce::Rectangle<float> (10.0f, 0.0f, w - 30.0f, h);
        auto mark = b.removeFromLeft (16.0f).withSizeKeepingCentre (15.0f, 15.0f);
        g.setGradientFill (juce::ColourGradient (t.accent2, mark.getX(), mark.getY(), t.accent, mark.getRight(), mark.getBottom(), false));
        const float d = mark.getHeight() * 0.7f;
        g.drawEllipse (juce::Rectangle<float> (d, d).withPosition (mark.getX(), mark.getY()), 1.4f);
        g.drawEllipse (juce::Rectangle<float> (d, d).withPosition (mark.getRight() - d, mark.getBottom() - d), 1.4f);

        b.removeFromLeft (7.0f);
        g.setColour (t.text.withAlpha (0.85f));
        g.setFont (fonts::make (12.5f, fonts::Weight::bold, 0.05f));
        const juce::String st ("ST");
        const float sw = fonts::stringWidth (g.getCurrentFont(), st);
        g.drawText (st, b, juce::Justification::centredLeft, false);
        g.setColour (t.text.withAlpha (0.65f));
        g.setFont (fonts::make (13.5f, fonts::Weight::regular, 0.02f));
        g.drawText (product, b.withTrimmedLeft (sw + 5.0f), juce::Justification::centredLeft, false);
    }

    // Preset name (text only)
    {
        auto p = presetArea.toFloat();
        const bool over = isMouseOver() && presetArea.contains (getMouseXYRelative());
        auto name = proc.presets.getCurrentName();
        g.setColour (over ? t.text : t.text.withAlpha (0.9f));
        g.setFont (fonts::medium (14.0f));
        g.drawText (name, p, juce::Justification::centred, true);

        if (modified)
        {
            const float tw = fonts::stringWidth (g.getCurrentFont(), name);
            g.setColour (t.accent2);
            g.fillEllipse (p.getCentreX() + tw * 0.5f + 6.0f, p.getCentreY() - 2.5f, 5.0f, 5.0f);
        }
    }
}

void HeaderBar::mouseDown (const juce::MouseEvent& e)
{
    if (presetArea.contains (e.getPosition()))
        showPresetMenu();
}

void HeaderBar::showPresetMenu()
{
    auto& pm = proc.presets;
    const auto& all = pm.getPresets();
    juce::PopupMenu menu;

    // Group by factory/user and category, preserving order.
    std::map<juce::String, juce::PopupMenu> factoryCats, userCats;
    juce::StringArray factoryOrder, userOrder;

    for (int i = 0; i < (int) all.size(); ++i)
    {
        const auto& p = all[(size_t) i];
        if (i == 0) continue; // Init gets its own item
        auto& map = p.isFactory ? factoryCats : userCats;
        auto& order = p.isFactory ? factoryOrder : userOrder;
        if (! order.contains (p.category)) order.add (p.category);
        map[p.category].addItem (i + 1, p.name, true, i == pm.getCurrentIndex());
    }

    menu.addItem (1, "Init", true, pm.getCurrentIndex() == 0);
    if (! factoryOrder.isEmpty())
    {
        menu.addSectionHeader ("Factory");
        for (auto& c : factoryOrder) menu.addSubMenu (c, factoryCats[c]);
    }
    if (! userOrder.isEmpty())
    {
        menu.addSectionHeader ("User");
        for (auto& c : userOrder)
        {
            if (c == "User") { juce::PopupMenu::MenuItemIterator it (userCats[c]); while (it.next()) menu.addItem (it.getItem()); }
            else menu.addSubMenu (c, userCats[c]);
        }
    }

    menu.addSeparator();
    const int saveId = 100000, deleteId = 100001, revealId = 100002, rescanId = 100003;
    menu.addItem (saveId, "Save preset as...");
    const bool canDelete = juce::isPositiveAndBelow (pm.getCurrentIndex(), (int) all.size()) && ! all[(size_t) pm.getCurrentIndex()].isFactory;
    menu.addItem (deleteId, "Delete current preset", canDelete);
    menu.addItem (revealId, "Show presets folder");
    menu.addItem (rescanId, "Rescan presets");

    ::st::showMenu (menu, this, [this, safe = juce::Component::SafePointer<HeaderBar> (this)] (int result)
    {
        if (safe == nullptr || result == 0) return;
        auto& pm2 = proc.presets;
        if (result == saveId) promptSave();
        else if (result == deleteId) pm2.deleteUserPreset (pm2.getCurrentIndex());
        else if (result == revealId) { pm2.getUserDirectory().createDirectory(); pm2.getUserDirectory().revealToUser(); }
        else if (result == rescanId) pm2.rescanUserPresets();
        else pm2.loadPreset (result - 1);
        repaint();
    }, true);
}

void HeaderBar::promptSave()
{
    if (auto* parent = getParentComponent())
    {
        const auto current = proc.presets.getCurrentName();
        TextPrompt::show (*parent, "Save preset", current == "Init" ? juce::String() : current, "Save",
                          [this, safe = juce::Component::SafePointer<HeaderBar> (this)] (const juce::String& name)
                          {
                              if (safe != nullptr) proc.presets.saveUserPreset (name);
                          });
    }
}

void HeaderBar::showSettingsMenu()
{
    juce::PopupMenu menu;
    juce::PopupMenu sizes;
    const float current = proc.getUiScale();
    for (float s : { 0.75f, 0.85f, 1.0f, 1.15f, 1.25f, 1.5f, 1.75f, 2.0f })
        sizes.addItem (juce::String (juce::roundToInt (s * 100.0f)) + "%", true, std::abs (current - s) < 0.01f,
                       [this, s] { if (onScaleChosen) onScaleChosen (s); });
    menu.addSubMenu ("Interface size", sizes);

    if (extendSettingsMenu)
    {
        menu.addSeparator();
        extendSettingsMenu (menu);
    }

    menu.addSeparator();
    menu.addItem (juce::String (JucePlugin_Name) + "  v" + ST_PRODUCT_VERSION, false, false, [] {});

    ::st::showMenu (menu, &settingsButton);
}

//==============================================================================
EditorBase::EditorBase (ProcessorBase& p, int w, int h)
    : AudioProcessorEditor (p), processorBase (p), header (p), designW (w), designH (h), content (*this)
{
    setLookAndFeel (&lnf);
    content.setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    content.addAndMakeVisible (header);
    content.setBounds (0, 0, designW, designH);

    tooltips = std::make_unique<juce::TooltipWindow> (this, 700);
    tooltips->setLookAndFeel (&lnf);

    header.onScaleChosen = [this] (float s) { setScale (s); };

    // Film-grain texture for the backdrop (subtle, generated once).
    grain = juce::Image (juce::Image::ARGB, 128, 128, true);
    juce::Random rng (0x5717);
    for (int y = 0; y < 128; ++y)
        for (int x = 0; x < 128; ++x)
        {
            const auto v = (juce::uint8) rng.nextInt (256);
            grain.setPixelAt (x, y, juce::Colour (v, v, v, (juce::uint8) 7));
        }

    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio ((double) designW / (double) designH);
        c->setSizeLimits (juce::roundToInt (designW * 0.6), juce::roundToInt (designH * 0.6), designW * 2, designH * 2);
    }

    scale = juce::jlimit (0.6f, 2.0f, p.getUiScale());
    setSize (juce::roundToInt ((float) designW * scale), juce::roundToInt ((float) designH * scale));
    setWantsKeyboardFocus (true);

    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<EditorBase> (this)]
    {
        if (safe != nullptr && ! safe->layoutReady) safe->relayout();
    });
}

EditorBase::~EditorBase()
{
    tooltips->setLookAndFeel (nullptr);
    content.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void EditorBase::setScale (float s)
{
    s = juce::jlimit (0.6f, 2.0f, s);
    setSize (juce::roundToInt ((float) designW * s), juce::roundToInt ((float) designH * s));
}

void EditorBase::resized()
{
    scale = (float) getWidth() / (float) designW;
    content.setTransform (juce::AffineTransform::scale (scale));
    processorBase.setUiScale (scale);
}

void EditorBase::paint (juce::Graphics& g)
{
    g.fillAll (Theme::get().bg0);
}

void EditorBase::relayout()
{
    layoutReady = true;
    content.resized();
}

void EditorBase::Content::resized()
{
    if (! owner.layoutReady)
        return;   // subclass not constructed yet - layout() is pure virtual here

    owner.header.setBounds (getLocalBounds().removeFromTop (headerHeight));
    owner.layout (getLocalBounds().withTrimmedTop (headerHeight));
}

void EditorBase::paintBackground (juce::Graphics& g, juce::Rectangle<int> area)
{
    // The backdrop never changes, but it sits behind everything that animates: render it once
    // (at the current physical resolution) and blit it afterwards.
    const float scale = juce::jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    const auto full = getContent().getLocalBounds();

    if (backdrop.isNull() || std::abs (scale - backdropScale) > 0.01f
        || backdrop.getWidth() != juce::roundToInt ((float) full.getWidth() * scale))
    {
        backdropScale = scale;
        backdrop = juce::Image (juce::Image::RGB, juce::roundToInt ((float) full.getWidth() * scale),
                                juce::roundToInt ((float) full.getHeight() * scale), false);
        juce::Graphics bg (backdrop);
        bg.addTransform (juce::AffineTransform::scale (scale));

        const auto& t = Theme::get();
        auto r = full.toFloat();
        juce::ColourGradient grad (t.displayTop, r.getCentreX(), r.getY(), t.bg0, r.getCentreX(), r.getBottom(), false);
        grad.addColour (0.45, t.displayTop.interpolatedWith (t.bg0, 0.55f));
        grad.addColour (0.75, t.bg1);
        bg.setGradientFill (grad);
        bg.fillRect (r);

        juce::ColourGradient vignette (juce::Colours::transparentBlack, r.getCentreX(), r.getY() + r.getHeight() * 0.35f,
                                       juce::Colours::black.withAlpha (0.35f), r.getX(), r.getY() + r.getHeight() * 0.35f, true);
        bg.setGradientFill (vignette);
        bg.fillRect (r);

        bg.setTiledImageFill (grain, 0, 0, 1.0f);
        bg.fillRect (r);
    }

    g.drawImage (backdrop, area.toFloat(), juce::RectanglePlacement::stretchToFit);
}

bool EditorBase::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    if (mods.isCommandDown() && key.getKeyCode() == 'Z')
    {
        if (mods.isShiftDown()) processorBase.undoManager.redo();
        else processorBase.undoManager.undo();
        return true;
    }
    if (mods.isCommandDown() && key.getKeyCode() == 'Y')
    {
        processorBase.undoManager.redo();
        return true;
    }
    return false;
}

} // namespace st
