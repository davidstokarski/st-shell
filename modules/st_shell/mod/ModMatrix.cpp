namespace st::mod
{

namespace
{
    const juce::Identifier modulationType ("MODULATION");
    const juce::Identifier routingType    ("ROUTING");
    const juce::Identifier sourceProp     ("source");
    const juce::Identifier targetProp     ("target");
    const juce::Identifier depthProp      ("depth");
    const juce::Identifier enabledProp    ("enabled");

    struct Division { const char* name; double quarterNotes; bool bars; };

    constexpr Division divisions[] =
    {
        { "8 Bars", 8.0, true }, { "4 Bars", 4.0, true }, { "2 Bars", 2.0, true }, { "1 Bar", 1.0, true },
        { "1/2",   2.0, false },         { "1/2.",  3.0, false },         { "1/2T",  4.0 / 3.0, false },
        { "1/4",   1.0, false },         { "1/4.",  1.5, false },         { "1/4T",  2.0 / 3.0, false },
        { "1/8",   0.5, false },         { "1/8.",  0.75, false },        { "1/8T",  1.0 / 3.0, false },
        { "1/16",  0.25, false },        { "1/16.", 0.375, false },       { "1/16T", 1.0 / 6.0, false },
        { "1/32",  0.125, false },       { "1/32.", 0.1875, false },      { "1/32T", 1.0 / 12.0, false },
        { "1/64",  0.0625, false },
    };
    constexpr int numDivisions = (int) std::size (divisions);

    // Indices into Theme::series (cyan/blue/purple/pink for LFOs, greens for envelopes, warm for macros).
    constexpr int lfoColours[]   = { 5, 4, 3, 2 };
    constexpr int envColours[]   = { 6, 7 };
    constexpr int macroColours[] = { 1, 0, 7, 3 };

    inline float readRef (const params::Ref& r, float fallback) noexcept
    {
        return r.ptr != nullptr ? r.get() : fallback;
    }

    juce::AudioParameterFloatAttributes lfoRateAttributes()
    {
        return juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int)
            {
                const int decimals = v < 0.1f ? 3 : (v < 10.0f ? 2 : 1);
                return juce::String (v, decimals) + " Hz";
            })
            .withValueFromStringFunction ([] (const juce::String& s) { return s.retainCharacters ("-.0123456789").getFloatValue(); })
            .withLabel ("Hz");
    }

    juce::AudioParameterFloatAttributes degreeAttributes()
    {
        return juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + juce::String (juce::CharPointer_UTF8 ("\xc2\xb0")); })
            .withValueFromStringFunction ([] (const juce::String& s) { return s.retainCharacters ("-.0123456789").getFloatValue(); })
            .withLabel (juce::String (juce::CharPointer_UTF8 ("\xc2\xb0")));
    }
}

//==============================================================================
juce::String SourceId::toString() const
{
    switch (kind)
    {
        case SourceKind::lfo:      return "lfo" + juce::String (index + 1);
        case SourceKind::envelope: return "env" + juce::String (index + 1);
        case SourceKind::macro:    return "macro" + juce::String (index + 1);
    }
    return {};
}

std::optional<SourceId> SourceId::fromString (const juce::String& s)
{
    const auto t = s.trim().toLowerCase();
    auto parse = [&t] (const char* prefix, SourceKind k) -> std::optional<SourceId>
    {
        if (! t.startsWith (prefix)) return std::nullopt;
        const auto num = t.substring ((int) std::strlen (prefix));
        if (num.isEmpty() || ! num.containsOnly ("0123456789")) return std::nullopt;
        const int n = num.getIntValue();
        if (n < 1) return std::nullopt;
        return SourceId { k, n - 1 };
    };

    if (auto r = parse ("lfo", SourceKind::lfo))        return r;
    if (auto r = parse ("env", SourceKind::envelope))   return r;
    if (auto r = parse ("macro", SourceKind::macro))    return r;
    return std::nullopt;
}

juce::StringArray getLfoShapeNames()
{
    return { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Random Step", "Random Smooth" };
}

juce::StringArray getLfoDivisionNames()
{
    juce::StringArray s;
    for (auto& d : divisions)
        s.add (d.name);
    return s;
}

int getDefaultLfoDivision() noexcept { return 7; }

double getLfoDivisionQuarterNotes (int index, double quarterNotesPerBar) noexcept
{
    const auto& d = divisions[(size_t) juce::jlimit (0, numDivisions - 1, index)];
    return d.bars ? d.quarterNotes * juce::jmax (0.25, quarterNotesPerBar) : d.quarterNotes;
}

//==============================================================================
juce::String ModMatrix::getParamId (const Config& c, SourceId s, const juce::String& suffix)
{
    switch (s.kind)
    {
        case SourceKind::lfo:      return c.idPrefix + "lfo" + juce::String (s.index + 1) + "_" + suffix;
        case SourceKind::envelope: return c.idPrefix + "env" + juce::String (s.index + 1) + "_" + suffix;
        case SourceKind::macro:    return c.idPrefix + "macro" + juce::String (s.index + 1);
    }
    return {};
}

juce::String ModMatrix::getParamId (SourceId s, const juce::String& suffix) const
{
    return getParamId (config, s, suffix);
}

void ModMatrix::addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout, const Config& c)
{
    using namespace st::params;

    for (int i = 0; i < c.numLfos; ++i)
    {
        const auto src = SourceId::lfo (i);
        const auto n = "LFO " + juce::String (i + 1);
        auto id = [&] (const char* suffix) { return getParamId (c, src, suffix); };

        layout.add (std::make_unique<juce::AudioProcessorParameterGroup> (
            c.idPrefix + "lfo" + juce::String (i + 1), n, "|",
            makeFloat (id (ids::rate), n + " Rate", logarithmic (0.01f, 40.0f), 1.0f, lfoRateAttributes()),
            makeBool (id (ids::sync), n + " Sync", false),
            makeChoice (id (ids::division), n + " Division", getLfoDivisionNames(), getDefaultLfoDivision()),
            makeChoice (id (ids::shape), n + " Shape", getLfoShapeNames(), 0),
            makeFloat (id (ids::phase), n + " Phase", linear (0.0f, 360.0f), 0.0f, degreeAttributes()),
            makeBool (id (ids::unipolar), n + " Unipolar", false)));
    }

    const auto inputs = c.envelopeInputs.isEmpty() ? juce::StringArray { "Input" } : c.envelopeInputs;

    for (int i = 0; i < c.numEnvelopes; ++i)
    {
        const auto src = SourceId::envelope (i);
        const auto n = "ENV " + juce::String (i + 1);
        auto id = [&] (const char* suffix) { return getParamId (c, src, suffix); };

        layout.add (std::make_unique<juce::AudioProcessorParameterGroup> (
            c.idPrefix + "env" + juce::String (i + 1), n, "|",
            makeFloat (id (ids::attack), n + " Attack", skewed (0.1f, 500.0f, 15.0f), 10.0f, msAttributes()),
            makeFloat (id (ids::release), n + " Release", skewed (1.0f, 3000.0f, 200.0f), 150.0f, msAttributes()),
            makeFloat (id (ids::gain), n + " Gain", linear (-24.0f, 24.0f), 0.0f, dbAttributes (1)),
            makeChoice (id (ids::input), n + " Input", inputs, 0)));
    }

    if (c.numMacros > 0)
    {
        auto group = std::make_unique<juce::AudioProcessorParameterGroup> (c.idPrefix + "macros", "Macros", "|");
        for (int i = 0; i < c.numMacros; ++i)
            group->addChild (makeFloat (getParamId (c, SourceId::macro (i)), "Macro " + juce::String (i + 1),
                                        linear (0.0f, 100.0f), 0.0f, percentAttributes()));
        layout.add (std::move (group));
    }
}

//==============================================================================
ModMatrix::ModMatrix (juce::AudioProcessorValueTreeState& s, Config c)
    : apvts (s), config (std::move (c)),
      numSources (juce::jmax (0, config.numLfos) + juce::jmax (0, config.numEnvelopes) + juce::jmax (0, config.numMacros))
{
    auto bind = [this] (params::Ref& r, const juce::String& id)
    {
        if (apvts.getParameter (id) != nullptr)
            r.bind (apvts, id);
        else
            jassertfalse; // ModMatrix::addParameters() wasn't called with the same Config
    };

    lfos = std::make_unique<Lfo[]> ((size_t) juce::jmax (1, config.numLfos));
    for (int i = 0; i < config.numLfos; ++i)
    {
        auto& l = lfos[(size_t) i];
        const auto src = SourceId::lfo (i);
        bind (l.rate,     getParamId (src, ids::rate));
        bind (l.sync,     getParamId (src, ids::sync));
        bind (l.division, getParamId (src, ids::division));
        bind (l.shape,    getParamId (src, ids::shape));
        bind (l.phase,    getParamId (src, ids::phase));
        bind (l.unipolar, getParamId (src, ids::unipolar));
    }

    envs = std::make_unique<Env[]> ((size_t) juce::jmax (1, config.numEnvelopes));
    for (int i = 0; i < config.numEnvelopes; ++i)
    {
        auto& e = envs[(size_t) i];
        const auto src = SourceId::envelope (i);
        bind (e.attack,  getParamId (src, ids::attack));
        bind (e.release, getParamId (src, ids::release));
        bind (e.gain,    getParamId (src, ids::gain));
        bind (e.input,   getParamId (src, ids::input));
    }

    macros.resize ((size_t) juce::jmax (0, config.numMacros));
    for (int i = 0; i < config.numMacros; ++i)
        bind (macros[(size_t) i], getParamId (SourceId::macro (i)));

    sourceValues.assign ((size_t) numSources, 0.0f);
    uiSourceValues = std::make_unique<std::atomic<float>[]> ((size_t) juce::jmax (1, numSources));
    for (int i = 0; i < numSources; ++i)
        uiSourceValues[(size_t) i].store (0.0f);

    const auto maxR = (size_t) juce::jmax (1, config.maxRoutings);
    pendingRoutes.resize (maxR);
    activeRoutes.resize (maxR);
    routings.reserve (maxR);

    apvts.state.addListener (this);
    rebuildFromState();
}

ModMatrix::~ModMatrix()
{
    apvts.state.removeListener (this);
}

//==============================================================================
int ModMatrix::addTarget (const juce::String& paramId)
{
    jassert (! prepared); // register targets in the processor constructor

    if (auto existing = getTargetIndex (paramId); existing >= 0)
        return existing;

    auto* p = apvts.getParameter (paramId);
    if (p == nullptr)
    {
        jassertfalse; // unknown parameter ID
        return -1;
    }

    auto slot = std::make_unique<TargetSlot>();
    slot->paramId = paramId;
    slot->param = p;
    targets.push_back (std::move (slot));

    const int index = (int) targets.size() - 1;
    targetLookup[paramId] = index;
    scratchOffsets.resize (targets.size(), 0.0f);
    scratchModulated.resize (targets.size(), 0);

    rebuildFromState(); // routings saved before this target was registered become live
    return index;
}

int ModMatrix::getTargetIndex (const juce::String& paramId) const noexcept
{
    auto it = targetLookup.find (paramId);
    return it != targetLookup.end() ? it->second : -1;
}

juce::String ModMatrix::getTargetParamId (int i) const
{
    return juce::isPositiveAndBelow (i, (int) targets.size()) ? targets[(size_t) i]->paramId : juce::String();
}

juce::RangedAudioParameter* ModMatrix::getTargetParameter (int i) const noexcept
{
    return juce::isPositiveAndBelow (i, (int) targets.size()) ? targets[(size_t) i]->param : nullptr;
}

//==============================================================================
SourceId ModMatrix::getSourceId (int flat) const noexcept
{
    if (flat < config.numLfos) return SourceId::lfo (juce::jmax (0, flat));
    flat -= config.numLfos;
    if (flat < config.numEnvelopes) return SourceId::envelope (flat);
    flat -= config.numEnvelopes;
    return SourceId::macro (flat);
}

int ModMatrix::getSourceIndex (SourceId s) const noexcept
{
    switch (s.kind)
    {
        case SourceKind::lfo:      return juce::isPositiveAndBelow (s.index, config.numLfos) ? s.index : -1;
        case SourceKind::envelope: return juce::isPositiveAndBelow (s.index, config.numEnvelopes) ? config.numLfos + s.index : -1;
        case SourceKind::macro:    return juce::isPositiveAndBelow (s.index, config.numMacros) ? config.numLfos + config.numEnvelopes + s.index : -1;
    }
    return -1;
}

juce::String ModMatrix::getSourceName (SourceId s) const
{
    switch (s.kind)
    {
        case SourceKind::lfo:      return "LFO " + juce::String (s.index + 1);
        case SourceKind::envelope: return "ENV " + juce::String (s.index + 1);
        case SourceKind::macro:    return "MACRO " + juce::String (s.index + 1);
    }
    return {};
}

int ModMatrix::getSourceColourIndex (SourceId s) const noexcept
{
    switch (s.kind)
    {
        case SourceKind::lfo:      return lfoColours[(size_t) (s.index % 4)];
        case SourceKind::envelope: return envColours[(size_t) (s.index % 2)];
        case SourceKind::macro:    return macroColours[(size_t) (s.index % 4)];
    }
    return 0;
}

SourceInfo ModMatrix::getSourceInfo (int flat) const
{
    SourceInfo info;
    info.id = getSourceId (flat);
    info.index = flat;
    info.name = getSourceName (info.id);
    info.shortName = info.id.kind == SourceKind::macro ? "MAC " + juce::String (info.id.index + 1) : info.name;
    info.colourIndex = getSourceColourIndex (info.id);
    return info;
}

int ModMatrix::getEnvelopeInputChoice (int envIndex) const noexcept
{
    if (! juce::isPositiveAndBelow (envIndex, config.numEnvelopes)) return 0;
    return juce::roundToInt (readRef (envs[(size_t) envIndex].input, 0.0f));
}

//==============================================================================
juce::ValueTree ModMatrix::getModTree() const
{
    return apvts.state.getChildWithName (modulationType);
}

juce::ValueTree ModMatrix::getOrCreateModTree()
{
    return apvts.state.getOrCreateChildWithName (modulationType, getUndoManager());
}

juce::ValueTree ModMatrix::findRoutingTree (SourceId s, const juce::String& paramId) const
{
    const auto key = s.toString();
    for (auto child : getModTree())
        if (child.hasType (routingType) && child[sourceProp].toString() == key && child[targetProp].toString() == paramId)
            return child;
    return {};
}

void ModMatrix::beginUndoTransaction (const juce::String& name)
{
    if (auto* um = getUndoManager())
        um->beginNewTransaction (name);
}

bool ModMatrix::addRouting (SourceId s, const juce::String& paramId, float depth)
{
    if (! isValidSource (s) || ! isTarget (paramId))
        return false;

    depth = juce::jlimit (-1.0f, 1.0f, depth);
    auto* um = getUndoManager();

    if (auto existing = findRoutingTree (s, paramId); existing.isValid())
    {
        beginUndoTransaction ("Modulation Depth");
        existing.setProperty (depthProp, depth, um);
        return true;
    }

    if (getNumRoutings() >= config.maxRoutings)
        return false;

    beginUndoTransaction ("Add Modulation");
    juce::ValueTree r (routingType);
    r.setProperty (sourceProp, s.toString(), nullptr);
    r.setProperty (targetProp, paramId, nullptr);
    r.setProperty (depthProp, depth, nullptr);
    r.setProperty (enabledProp, true, nullptr);
    getOrCreateModTree().appendChild (r, um);
    return true;
}

bool ModMatrix::removeRouting (SourceId s, const juce::String& paramId)
{
    auto r = findRoutingTree (s, paramId);
    if (! r.isValid())
        return false;

    beginUndoTransaction ("Remove Modulation");
    getModTree().removeChild (r, getUndoManager());
    return true;
}

void ModMatrix::removeAllRoutingsForTarget (const juce::String& paramId)
{
    auto mod = getModTree();
    beginUndoTransaction ("Remove Modulation");
    for (int i = mod.getNumChildren(); --i >= 0;)
        if (mod.getChild (i)[targetProp].toString() == paramId)
            mod.removeChild (i, getUndoManager());
}

void ModMatrix::removeAllRoutingsForSource (SourceId s)
{
    auto mod = getModTree();
    const auto key = s.toString();
    beginUndoTransaction ("Remove Modulation");
    for (int i = mod.getNumChildren(); --i >= 0;)
        if (mod.getChild (i)[sourceProp].toString() == key)
            mod.removeChild (i, getUndoManager());
}

void ModMatrix::clearRoutings()
{
    auto mod = getModTree();
    if (mod.getNumChildren() == 0) return;
    beginUndoTransaction ("Clear Modulation");
    mod.removeAllChildren (getUndoManager());
}

bool ModMatrix::setRoutingDepth (SourceId s, const juce::String& paramId, float depth)
{
    auto r = findRoutingTree (s, paramId);
    if (! r.isValid())
        return false;
    r.setProperty (depthProp, juce::jlimit (-1.0f, 1.0f, depth), getUndoManager());
    return true;
}

bool ModMatrix::setRoutingEnabled (SourceId s, const juce::String& paramId, bool enabled)
{
    auto r = findRoutingTree (s, paramId);
    if (! r.isValid())
        return false;
    beginUndoTransaction (enabled ? "Enable Modulation" : "Disable Modulation");
    r.setProperty (enabledProp, enabled, getUndoManager());
    return true;
}

std::vector<Routing> ModMatrix::getRoutings() const
{
    const juce::ScopedLock sl (routingLock);
    return routings;
}

std::vector<Routing> ModMatrix::getRoutingsForTarget (const juce::String& paramId) const
{
    const juce::ScopedLock sl (routingLock);
    std::vector<Routing> out;
    for (auto& r : routings)
        if (r.targetId == paramId)
            out.push_back (r);
    return out;
}

std::vector<Routing> ModMatrix::getRoutingsForSource (SourceId s) const
{
    const juce::ScopedLock sl (routingLock);
    std::vector<Routing> out;
    for (auto& r : routings)
        if (r.source == s)
            out.push_back (r);
    return out;
}

std::optional<Routing> ModMatrix::getRouting (SourceId s, const juce::String& paramId) const
{
    const juce::ScopedLock sl (routingLock);
    for (auto& r : routings)
        if (r.source == s && r.targetId == paramId)
            return r;
    return std::nullopt;
}

int ModMatrix::getNumRoutings() const
{
    const juce::ScopedLock sl (routingLock);
    return (int) routings.size();
}

//==============================================================================
void ModMatrix::rebuildFromState()
{
    std::vector<Routing> fresh;
    fresh.reserve ((size_t) juce::jmax (1, config.maxRoutings));

    for (auto child : getModTree())
    {
        if (! child.hasType (routingType) || (int) fresh.size() >= config.maxRoutings)
            continue;

        auto src = SourceId::fromString (child[sourceProp].toString());
        const auto target = child[targetProp].toString();
        if (! src.has_value() || target.isEmpty())
            continue;

        const bool duplicate = std::any_of (fresh.begin(), fresh.end(),
                                            [&] (const Routing& r) { return r.source == *src && r.targetId == target; });
        if (duplicate)
            continue;

        Routing r;
        r.source = *src;
        r.targetId = target;
        r.depth = juce::jlimit (-1.0f, 1.0f, (float) (double) child.getProperty (depthProp, 0.0));
        r.enabled = (bool) child.getProperty (enabledProp, true);
        fresh.push_back (std::move (r));
    }

    {
        const juce::ScopedLock sl (routingLock);
        routings.swap (fresh);

        const juce::SpinLock::ScopedLockType spin (pendingLock);
        numPendingRoutes = 0;
        for (auto& r : routings)
        {
            const int src = getSourceIndex (r.source);
            const int tgt = getTargetIndex (r.targetId);
            if (r.enabled && src >= 0 && tgt >= 0 && ! juce::exactlyEqual (r.depth, 0.0f) && numPendingRoutes < (int) pendingRoutes.size())
                pendingRoutes[(size_t) numPendingRoutes++] = { src, tgt, r.depth };
        }
        pendingDirty.store (true, std::memory_order_release);
    }

    sendChangeMessage();
}

void ModMatrix::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier&)
{
    if (tree.hasType (routingType) && tree.getParent().hasType (modulationType))
        rebuildFromState();
}

void ModMatrix::valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree& child)
{
    if (parent.hasType (modulationType) || child.hasType (modulationType))
        rebuildFromState();
}

void ModMatrix::valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree& child, int)
{
    if (parent.hasType (modulationType) || child.hasType (modulationType))
        rebuildFromState();
}

void ModMatrix::valueTreeRedirected (juce::ValueTree&)
{
    rebuildFromState();
}

//==============================================================================
void ModMatrix::prepare (double sr, int)
{
    sampleRate = sr > 0.0 ? sr : 44100.0;

    for (int i = 0; i < config.numEnvelopes; ++i)
    {
        auto& e = envs[(size_t) i];
        e.follower.prepare (sampleRate, dsp::EnvelopeFollower::kMaxChannels);
        e.lastAttack = e.lastRelease = -1.0f;
    }

    scratchOffsets.assign (targets.size(), 0.0f);
    scratchModulated.assign (targets.size(), 0);

    reset();
    prepared = true;
}

void ModMatrix::reset() noexcept
{
    for (int i = 0; i < config.numLfos; ++i)
    {
        auto& l = lfos[(size_t) i];
        l.phaseAcc = 0.0;
        l.lastP = 1.0f;     // forces a fresh random value on the first update
        l.rndFrom = 0.0f;
        l.rndTo = 0.0f;
    }

    for (int i = 0; i < config.numEnvelopes; ++i)
    {
        envs[(size_t) i].follower.reset();
        envs[(size_t) i].level = 0.0f;
    }

    samplesSinceTransport = 0;
}

void ModMatrix::setTransport (const juce::AudioPlayHead::PositionInfo& info) noexcept
{
    transportPlaying = info.getIsPlaying();

    if (auto bpm = info.getBpm(); bpm.hasValue() && *bpm > 1.0)
        transportBpm = *bpm;

    if (auto ppq = info.getPpqPosition(); ppq.hasValue())
    {
        transportPpq = *ppq;
        transportHasPpq = true;
    }
    else
    {
        transportHasPpq = false;
    }

    if (auto ts = info.getTimeSignature(); ts.hasValue() && ts->numerator > 0 && ts->denominator > 0)
        quarterNotesPerBar = 4.0 * (double) ts->numerator / (double) ts->denominator;

    samplesSinceTransport = 0;
}

void ModMatrix::feedEnvelopeInput (int envIndex, const float* const* channels, int numChannels, int numSamples) noexcept
{
    if (! prepared || channels == nullptr || numSamples <= 0 || ! juce::isPositiveAndBelow (envIndex, config.numEnvelopes))
        return;

    auto& e = envs[(size_t) envIndex];

    const float att = readRef (e.attack, 10.0f), rel = readRef (e.release, 150.0f);
    if (! juce::exactlyEqual (att, e.lastAttack))  { e.follower.setAttackMs (att);  e.lastAttack = att; }
    if (! juce::exactlyEqual (rel, e.lastRelease)) { e.follower.setReleaseMs (rel); e.lastRelease = rel; }

    const int nch = juce::jmin (numChannels, dsp::EnvelopeFollower::kMaxChannels);
    float level = 0.0f;
    for (int ch = 0; ch < nch; ++ch)
        if (channels[ch] != nullptr)
            level = juce::jmax (level, e.follower.processBlock (ch, channels[ch], numSamples));

    e.level = level;
}

float ModMatrix::nextRandom() noexcept
{
    // xorshift32 -> [-1, 1)
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;
    return (float) (rngState >> 8) * (2.0f / 16777216.0f) - 1.0f;
}

void ModMatrix::updateLfo (int i, int numSamples) noexcept
{
    auto& l = lfos[(size_t) i];

    const auto shape = (LfoShape) juce::jlimit (0, numLfoShapes - 1, juce::roundToInt (readRef (l.shape, 0.0f)));
    const double offset = (double) readRef (l.phase, 0.0f) / 360.0;
    const bool synced = readRef (l.sync, 0.0f) >= 0.5f;

    double increment = 0.0;
    if (synced)
    {
        const double qn = getLfoDivisionQuarterNotes (juce::roundToInt (readRef (l.division, (float) getDefaultLfoDivision())), quarterNotesPerBar);

        if (transportPlaying && transportHasPpq)
        {
            const double ppq = transportPpq + (double) samplesSinceTransport / sampleRate * transportBpm / 60.0;
            const double cycles = ppq / qn;
            l.phaseAcc = cycles - std::floor (cycles);
        }
        else
        {
            increment = transportBpm / 60.0 / qn * (double) numSamples / sampleRate;
        }
    }
    else
    {
        increment = (double) juce::jlimit (0.001f, 1000.0f, readRef (l.rate, 1.0f)) * (double) numSamples / sampleRate;
    }

    double pd = l.phaseAcc + offset;
    pd -= std::floor (pd);
    const float p = juce::jlimit (0.0f, 0.99999994f, (float) pd);

    if (p < l.lastP)  // new cycle
    {
        l.rndFrom = l.rndTo;
        l.rndTo = nextRandom();
    }
    l.lastP = p;

    float v = evaluateLfoShape (shape, p, l.rndFrom, l.rndTo);
    if (readRef (l.unipolar, 0.0f) >= 0.5f)
        v = 0.5f * (v + 1.0f);

    sourceValues[(size_t) i] = v;
    l.uiPhase.store (p, std::memory_order_relaxed);

    if (increment > 0.0)
    {
        l.phaseAcc += increment;
        l.phaseAcc -= std::floor (l.phaseAcc);
    }
}

void ModMatrix::applyPendingRoutes() noexcept
{
    if (! pendingDirty.load (std::memory_order_acquire))
        return;

    if (pendingLock.tryEnter())
    {
        numActiveRoutes = juce::jmin (numPendingRoutes, (int) activeRoutes.size());
        std::copy (pendingRoutes.begin(), pendingRoutes.begin() + numActiveRoutes, activeRoutes.begin());
        pendingDirty.store (false, std::memory_order_relaxed);
        pendingLock.exit();
    }
}

void ModMatrix::advance (int numSamples) noexcept
{
    if (! prepared || numSamples <= 0)
        return;

    applyPendingRoutes();

    for (int i = 0; i < config.numLfos; ++i)
        updateLfo (i, numSamples);

    constexpr float envRangeDb = 48.0f;
    for (int i = 0; i < config.numEnvelopes; ++i)
    {
        auto& e = envs[(size_t) i];
        const float db = dsp::gainToDb (e.level) + readRef (e.gain, 0.0f);
        sourceValues[(size_t) (config.numLfos + i)] = juce::jlimit (0.0f, 1.0f, (db + envRangeDb) / envRangeDb);
    }

    for (int i = 0; i < config.numMacros; ++i)
        sourceValues[(size_t) (config.numLfos + config.numEnvelopes + i)]
            = juce::jlimit (0.0f, 1.0f, readRef (macros[(size_t) i], 0.0f) * 0.01f);

    for (int i = 0; i < numSources; ++i)
        uiSourceValues[(size_t) i].store (sourceValues[(size_t) i], std::memory_order_relaxed);

    // Per-target offsets
    const int nt = juce::jmin ((int) targets.size(), (int) scratchOffsets.size());
    std::fill (scratchOffsets.begin(), scratchOffsets.end(), 0.0f);
    std::fill (scratchModulated.begin(), scratchModulated.end(), (std::uint8_t) 0);

    for (int r = 0; r < numActiveRoutes; ++r)
    {
        const auto& route = activeRoutes[(size_t) r];
        if (route.target < nt && route.source < numSources)
        {
            scratchOffsets[(size_t) route.target] += route.depth * sourceValues[(size_t) route.source];
            scratchModulated[(size_t) route.target] = 1;
        }
    }

    for (int t = 0; t < nt; ++t)
    {
        targets[(size_t) t]->offset.store (scratchOffsets[(size_t) t], std::memory_order_relaxed);
        targets[(size_t) t]->modulated.store (scratchModulated[(size_t) t] != 0, std::memory_order_relaxed);
    }

    samplesSinceTransport += numSamples;
}

float ModMatrix::getOffset (int t) const noexcept
{
    return juce::isPositiveAndBelow (t, (int) targets.size()) ? targets[(size_t) t]->offset.load (std::memory_order_relaxed) : 0.0f;
}

float ModMatrix::getModulatedNormalised (int t, float base) const noexcept
{
    if (! juce::isPositiveAndBelow (t, (int) targets.size()))
        return base;
    return juce::jlimit (0.0f, 1.0f, base + targets[(size_t) t]->offset.load (std::memory_order_relaxed));
}

float ModMatrix::getModulatedPlain (int t) const noexcept
{
    auto* p = getTargetParameter (t);
    if (p == nullptr)
        return 0.0f;
    return p->convertFrom0to1 (getModulatedNormalised (t, p->getValue()));
}

bool ModMatrix::isTargetModulated (int t) const noexcept
{
    return juce::isPositiveAndBelow (t, (int) targets.size()) && targets[(size_t) t]->modulated.load (std::memory_order_relaxed);
}

//==============================================================================
float ModMatrix::getSourceValue (int flat) const noexcept
{
    return juce::isPositiveAndBelow (flat, numSources) ? uiSourceValues[(size_t) flat].load (std::memory_order_relaxed) : 0.0f;
}

bool ModMatrix::isSourceBipolar (int flat) const noexcept
{
    if (! juce::isPositiveAndBelow (flat, config.numLfos))
        return false;
    return readRef (lfos[(size_t) flat].unipolar, 0.0f) < 0.5f;
}

float ModMatrix::getSourceValueNormalised (int flat) const noexcept
{
    const float v = getSourceValue (flat);
    return isSourceBipolar (flat) ? 0.5f * (v + 1.0f) : v;
}

float ModMatrix::getLfoPhase (int lfo) const noexcept
{
    return juce::isPositiveAndBelow (lfo, config.numLfos) ? lfos[(size_t) lfo].uiPhase.load (std::memory_order_relaxed) : 0.0f;
}

void ModMatrix::setSourcePalette (const juce::Array<juce::Colour>& p)
{
    const juce::ScopedLock sl (routingLock);
    palette = p;
}

juce::Colour ModMatrix::getSourceColour (int flat) const
{
    const juce::ScopedLock sl (routingLock);
    if (palette.isEmpty() || ! juce::isPositiveAndBelow (flat, numSources))
        return {};
    return palette[getSourceColourIndex (getSourceId (flat)) % palette.size()];
}

ModulationDisplay ModMatrix::getDisplay (const juce::String& paramId, float base) const
{
    ModulationDisplay d;
    const int t = getTargetIndex (paramId);
    if (t < 0)
        return d;

    float lo = 0.0f, hi = 0.0f;
    int firstSource = -1;
    {
        const juce::ScopedLock sl (routingLock);
        for (auto& r : routings)
        {
            if (! r.enabled || r.targetId != paramId)
                continue;
            const int src = getSourceIndex (r.source);
            if (src < 0)
                continue;

            const float a = isSourceBipolar (src) ? -r.depth : 0.0f;
            const float b = r.depth;
            lo += juce::jmin (a, b);
            hi += juce::jmax (a, b);
            if (firstSource < 0)
                firstSource = src;
        }
    }

    if (firstSource < 0)
        return d;

    d.active = true;
    d.lo = juce::jlimit (0.0f, 1.0f, base + lo);
    d.hi = juce::jlimit (0.0f, 1.0f, base + hi);
    d.current = getModulatedNormalised (t, base);
    d.colour = getSourceColour (firstSource);
    return d;
}

} // namespace st::mod
