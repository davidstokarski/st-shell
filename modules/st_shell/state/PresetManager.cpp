namespace st
{

static const juce::Identifier presetNameId ("stPresetName");
static const juce::Identifier presetIndexId ("stPresetIndex");

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& s, const juce::String& company, const juce::String& productName)
    : apvts (s), product (productName)
{
    const auto root = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       #if JUCE_MAC
                        .getChildFile ("Audio/Presets")
                       #endif
                        ;
    userDir = root.getChildFile (company).getChildFile (productName);
    // presets saved before the rename to Stokarski Studios (also the host's own presets, e.g. Logic's .aupreset)
    if (company == kCompanyName) adoptLegacyFolder (userDir, root.getChildFile (kLegacyCompanyName).getChildFile (productName));

    defaultState = apvts.copyState();
    defaultState.setProperty (presetNameId, "Init", nullptr);

    Preset init;
    init.name = "Init";
    init.category = "Factory";
    init.isFactory = true;
    init.state = defaultState.createCopy();
    presets.push_back (init);
    numFactory = 1;

    rescanUserPresets();
    takeSnapshot();
}

void PresetManager::addFactoryPreset (const juce::String& name, const juce::String& category,
                                      std::initializer_list<std::pair<juce::String, float>> values,
                                      std::function<void (juce::ValueTree&)> customise)
{
    addFactoryPreset (name, category, std::vector<std::pair<juce::String, float>> (values), std::move (customise));
}

void PresetManager::addFactoryPreset (const juce::String& name, const juce::String& category,
                                      const std::vector<std::pair<juce::String, float>>& values,
                                      std::function<void (juce::ValueTree&)> customise)
{
    auto state = defaultState.createCopy();

    for (auto& [id, value] : values)
    {
        auto child = state.getChildWithProperty ("id", id);
        jassert (child.isValid()); // unknown parameter id in factory preset
        if (child.isValid())
            child.setProperty ("value", value, nullptr);
    }

    if (customise)
        customise (state);

    state.setProperty (presetNameId, name, nullptr);

    Preset p;
    p.name = name;
    p.category = category;
    p.isFactory = true;
    p.state = state;
    presets.insert (presets.begin() + numFactory, p);
    ++numFactory;
}

void PresetManager::rescanUserPresets()
{
    presets.erase (presets.begin() + numFactory, presets.end());

    if (userDir.isDirectory())
    {
        auto files = userDir.findChildFiles (juce::File::findFiles, true, juce::String ("*") + fileExtension);
        std::vector<Preset> user;

        for (auto& f : files)
        {
            Preset p;
            p.name = f.getFileNameWithoutExtension();
            p.category = f.getParentDirectory() == userDir ? juce::String ("User") : f.getParentDirectory().getFileName();
            p.file = f;
            user.push_back (p);
        }

        std::sort (user.begin(), user.end(), [] (const Preset& a, const Preset& b)
        {
            if (a.category != b.category) return a.category.compareNatural (b.category) < 0;
            return a.name.compareNatural (b.name) < 0;
        });

        presets.insert (presets.end(), user.begin(), user.end());
    }

    sendChangeMessage();
}

juce::ValueTree PresetManager::readFile (const juce::File& f, juce::String* nameOut)
{
    auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr || ! xml->hasTagName ("STPreset"))
        return {};

    if (nameOut != nullptr)
        *nameOut = xml->getStringAttribute ("name", f.getFileNameWithoutExtension());

    if (auto* stateXml = xml->getFirstChildElement())
        return juce::ValueTree::fromXml (*stateXml);

    return {};
}

void PresetManager::applyState (const juce::ValueTree& state, const juce::String& name, int index)
{
    if (! state.isValid() || ! state.hasType (apvts.state.getType()))
        return;

    // Start from defaults so presets saved by older versions get sane values for new parameters.
    auto merged = defaultState.createCopy();
    for (auto child : state)
    {
        if (child.hasType ("PARAM"))
        {
            auto target = merged.getChildWithProperty ("id", child["id"]);
            if (target.isValid()) target.setProperty ("value", child["value"], nullptr);
        }
        else
        {
            auto existing = merged.getChildWithName (child.getType());
            if (existing.isValid()) merged.removeChild (existing, nullptr);
            merged.appendChild (child.createCopy(), nullptr);
        }
    }

    merged.setProperty (presetNameId, name, nullptr);
    merged.setProperty (presetIndexId, index, nullptr);
    apvts.replaceState (merged);
    currentIndex = index;
    takeSnapshot();

    if (onPresetLoaded) onPresetLoaded();
    sendChangeMessage();
}

bool PresetManager::loadPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return false;

    auto& p = presets[(size_t) index];
    auto state = p.isFactory ? p.state : readFile (p.file);
    if (! state.isValid())
        return false;

    applyState (state, p.name, index);
    return true;
}

bool PresetManager::loadPresetFile (const juce::File& f)
{
    juce::String name;
    auto state = readFile (f, &name);
    if (! state.isValid()) return false;

    int index = -1;
    for (size_t i = 0; i < presets.size(); ++i)
        if (presets[i].file == f) index = (int) i;

    applyState (state, name, index);
    return true;
}

void PresetManager::loadDefault() { loadPreset (0); }

void PresetManager::loadNext()
{
    if (presets.empty()) return;
    loadPreset ((currentIndex + 1 + (int) presets.size()) % (int) presets.size());
}

void PresetManager::loadPrevious()
{
    if (presets.empty()) return;
    loadPreset ((currentIndex - 1 + (int) presets.size()) % (int) presets.size());
}

bool PresetManager::saveUserPreset (const juce::String& rawName, const juce::String& category)
{
    const auto name = juce::File::createLegalFileName (rawName.trim());
    if (name.isEmpty()) return false;

    auto dir = category.isNotEmpty() ? userDir.getChildFile (juce::File::createLegalFileName (category)) : userDir;
    if (! dir.createDirectory()) return false;

    auto state = apvts.copyState();
    state.setProperty (presetNameId, name, nullptr);
    state.removeProperty (presetIndexId, nullptr);

    juce::XmlElement root ("STPreset");
    root.setAttribute ("name", name);
    root.setAttribute ("product", product);
    root.setAttribute ("version", ST_PRODUCT_VERSION);
    if (auto xml = state.createXml())
        root.addChildElement (xml.release());

    auto file = dir.getChildFile (name + fileExtension);
    if (! root.writeTo (file))
        return false;

    rescanUserPresets();

    for (size_t i = 0; i < presets.size(); ++i)
        if (presets[i].file == file)
            currentIndex = (int) i;

    apvts.state.setProperty (presetNameId, name, nullptr);
    apvts.state.setProperty (presetIndexId, currentIndex, nullptr);
    takeSnapshot();
    sendChangeMessage();
    return true;
}

bool PresetManager::deleteUserPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets.size()) || presets[(size_t) index].isFactory)
        return false;

    const bool ok = presets[(size_t) index].file.moveToTrash();
    rescanUserPresets();
    if (currentIndex == index) currentIndex = -1;
    return ok;
}

juce::String PresetManager::getCurrentName() const
{
    return apvts.state.getProperty (presetNameId, "Init").toString();
}

void PresetManager::takeSnapshot()
{
    snapshot.clear();
    for (auto* p : apvts.processor.getParameters())
        snapshot.push_back (p->getValue());
}

bool PresetManager::isModified() const
{
    const auto& params = apvts.processor.getParameters();
    if (params.size() != (int) snapshot.size()) return true;

    for (int i = 0; i < params.size(); ++i)
        if (std::abs (params[i]->getValue() - snapshot[(size_t) i]) > 1.0e-5f)
            return true;

    return false;
}

void PresetManager::sessionRestored()
{
    currentIndex = (int) apvts.state.getProperty (presetIndexId, -1);
    const auto name = getCurrentName();

    // Indices of user presets can shift between sessions; resolve by name when needed.
    if (! juce::isPositiveAndBelow (currentIndex, (int) presets.size()) || presets[(size_t) currentIndex].name != name)
    {
        currentIndex = -1;
        for (size_t i = 0; i < presets.size(); ++i)
            if (presets[i].name == name) { currentIndex = (int) i; break; }
    }

    takeSnapshot();
    sendChangeMessage();
}

//==============================================================================
void ABCompare::select (int slot)
{
    slot = juce::jlimit (0, 1, slot);
    if (slot == active) return;

    slots[(size_t) active] = apvts.copyState();

    if (slots[(size_t) slot].isValid())
        apvts.replaceState (slots[(size_t) slot].createCopy());
    else
        slots[(size_t) slot] = apvts.copyState();

    active = slot;
    sendChangeMessage();
}

void ABCompare::copyActiveToOther()
{
    slots[(size_t) (1 - active)] = apvts.copyState();
    sendChangeMessage();
}

} // namespace st
