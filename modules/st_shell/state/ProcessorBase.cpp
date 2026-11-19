namespace st
{

ProcessorBase::ProcessorBase (const BusesProperties& buses, juce::AudioProcessorValueTreeState::ParameterLayout layout,
                              const juce::String& companyName)
    : AudioProcessor (buses),
      apvts (*this, &undoManager, "STATE", std::move (layout)),
      presets (apvts, companyName, JucePlugin_Name),
      ab (apvts)
{
    presets.onPresetLoaded = [this]
    {
        undoManager.clearUndoHistory();
        stateRestored();
    };
    instanceNumber = InstanceRegistry::get().add (this);
}

ProcessorBase::~ProcessorBase()
{
    unregisterInstance();
}

void ProcessorBase::unregisterInstance()
{
    InstanceRegistry::get().remove (this);
}

void ProcessorBase::updateTrackProperties (const TrackProperties& p)
{
    {
        const juce::ScopedLock sl (trackLock);
        track = p;
    }
    InstanceRegistry::get().changed();
}

juce::String ProcessorBase::getTrackName() const
{
    const juce::ScopedLock sl (trackLock);
    return track.name.value_or (juce::String());
}

juce::Colour ProcessorBase::getTrackColour() const
{
    const juce::ScopedLock sl (trackLock);
    return track.colourARGB.has_value() ? juce::Colour (*track.colourARGB) : juce::Colour();
}

juce::String ProcessorBase::getInstanceLabel() const
{
    const auto n = getTrackName();
    return n.isNotEmpty() ? n : getName() + " " + juce::String (instanceNumber);
}

void ProcessorBase::prepareShell (double sampleRate)
{
    inputMeter.prepare (sampleRate);
    outputMeter.prepare (sampleRate);
    preAnalyzer.prepare (sampleRate);
    postAnalyzer.prepare (sampleRate);
}

bool ProcessorBase::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return in == out;
}

void ProcessorBase::getStateInformation (juce::MemoryBlock& dest)
{
    juce::XmlElement root ("STPluginState");
    root.setAttribute ("version", ST_PRODUCT_VERSION);

    if (auto xml = apvts.copyState().createXml())
        root.addChildElement (xml.release());
    if (auto ui = uiState.createXml())
        root.addChildElement (ui.release());

    copyXmlToBinary (root, dest);
}

void ProcessorBase::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName ("STPluginState"))
        return;

    if (auto* s = xml->getChildByName (apvts.state.getType()))
    {
        auto tree = juce::ValueTree::fromXml (*s);
        if (tree.isValid())
            apvts.replaceState (tree);
    }

    if (auto* u = xml->getChildByName ("UI"))
    {
        auto tree = juce::ValueTree::fromXml (*u);
        if (tree.isValid())
            uiState.copyPropertiesAndChildrenFrom (tree, nullptr);
    }

    juce::MessageManager::callAsync ([safe = juce::WeakReference<ProcessorBase> (this)]
    {
        if (auto* p = safe.get())
        {
            p->presets.sessionRestored();
            p->undoManager.clearUndoHistory();
            p->stateRestored();
        }
    });
}

} // namespace st
