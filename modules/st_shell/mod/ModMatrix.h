#pragma once

namespace st::mod
{

/** One source -> target connection. Depth is in normalised parameter units (-1..+1). */
struct Routing
{
    SourceId source;
    juce::String targetId;   // parameter ID
    float depth = 0.0f;
    bool enabled = true;
};

/** Static description of a source (for UI). */
struct SourceInfo
{
    SourceId id;
    int index = 0;            // flat index (0 .. getNumSources()-1)
    juce::String name;        // "LFO 1", "ENV 2", "MACRO 3"
    juce::String shortName;   // "LFO 1", "ENV 2", "MAC 3"
    int colourIndex = 0;      // index into Theme::series
};

//==============================================================================
/** Modulation matrix: LFOs, envelope followers and macros routed to registered parameters.

    Threading
    - Construction, addTarget(), routing edits and getDisplay(): message thread.
    - prepare() / reset(): audio setup (prepareToPlay).
    - setTransport(), feedEnvelopeInput(), advance(), getModulated*(), isTargetModulated(): audio thread,
      real-time safe (no locks, no allocation). Routing edits reach the audio thread through a
      pre-allocated table guarded by a SpinLock the audio thread only ever try-locks.

    Persistence
    Routings live in a "MODULATION" child of apvts.state, so they are part of presets, sessions,
    A/B slots and undo (edits go through apvts.undoManager). replaceState() is detected and the
    routing table is rebuilt automatically.

    Typical audio callback:
        matrix.setTransport (*getPlayHead()->getPosition());
        for each sub-block (<= 64 samples):
            matrix.feedEnvelopeInput (0, ptrs, numCh, n);
            matrix.advance (n);
            float drive = matrix.getModulatedPlain (driveTarget);
*/
class ModMatrix : public juce::ChangeBroadcaster, private juce::ValueTree::Listener
{
public:
    /** Adds the host-automatable source parameters (LFO/ENV/macro) to a layout. */
    static void addParameters (juce::AudioProcessorValueTreeState::ParameterLayout&, const Config& = {});

    /** Construct after the APVTS exists (e.g. as a member declared after it). */
    ModMatrix (juce::AudioProcessorValueTreeState&, Config = {});
    ~ModMatrix() override;

    const Config& getConfig() const noexcept { return config; }
    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return apvts; }
    juce::UndoManager* getUndoManager() const noexcept { return apvts.undoManager; }

    //==============================================================================
    // Targets (register in the processor constructor, before prepare()).
    /** Registers a modulatable parameter. Returns its target index (stable), or -1 if the ID is unknown. */
    int addTarget (const juce::String& paramId);
    int getNumTargets() const noexcept { return (int) targets.size(); }
    int getTargetIndex (const juce::String& paramId) const noexcept;
    bool isTarget (const juce::String& paramId) const noexcept { return getTargetIndex (paramId) >= 0; }
    juce::String getTargetParamId (int targetIndex) const;
    juce::RangedAudioParameter* getTargetParameter (int targetIndex) const noexcept;

    //==============================================================================
    // Sources. Flat index order: LFOs, envelopes, macros.
    int getNumSources() const noexcept { return numSources; }
    SourceId getSourceId (int flatIndex) const noexcept;
    int getSourceIndex (SourceId) const noexcept;   // -1 if out of range for this Config
    bool isValidSource (SourceId s) const noexcept { return getSourceIndex (s) >= 0; }
    SourceInfo getSourceInfo (int flatIndex) const;
    juce::String getSourceName (SourceId) const;
    int getSourceColourIndex (SourceId) const noexcept;

    /** Full parameter ID of a source parameter, e.g. (lfo 0, ids::rate) -> "mod_lfo1_rate".
        For macros the suffix is ignored ("mod_macro1"). */
    juce::String getParamId (SourceId, const juce::String& suffix = {}) const;
    static juce::String getParamId (const Config&, SourceId, const juce::String& suffix = {});

    /** Choice index of an envelope follower's "input" parameter (audio-thread safe). */
    int getEnvelopeInputChoice (int envelopeIndex) const noexcept;

    //==============================================================================
    // Routings (message thread; undoable through apvts.undoManager).
    /** Adds (or, if it exists, updates the depth of) a routing. False if the target isn't registered,
        the source is invalid or maxRoutings is reached. */
    bool addRouting (SourceId, const juce::String& paramId, float depth = 0.25f);
    bool removeRouting (SourceId, const juce::String& paramId);
    void removeAllRoutingsForTarget (const juce::String& paramId);
    /** Removes every routing of a source. Pass newTransaction = false to fold it into the caller's undo step. */
    void removeAllRoutingsForSource (SourceId, bool newTransaction = true);
    void clearRoutings();
    /** For continuous drags call beginUndoTransaction() once on mouse-down; consecutive depth
        changes then coalesce into a single undo step. */
    bool setRoutingDepth (SourceId, const juce::String& paramId, float depth);
    bool setRoutingEnabled (SourceId, const juce::String& paramId, bool enabled);
    void beginUndoTransaction (const juce::String& name = {});

    std::vector<Routing> getRoutings() const;
    std::vector<Routing> getRoutingsForTarget (const juce::String& paramId) const;
    std::vector<Routing> getRoutingsForSource (SourceId) const;
    std::optional<Routing> getRouting (SourceId, const juce::String& paramId) const;
    int getNumRoutings() const;

    //==============================================================================
    // Source visibility (UI, message thread). Which sources a modulation strip shows is stored in the
    // MODULATION tree ("visible" property), so it travels with presets/sessions and is undoable.
    // A source with routings is always shown. Fresh state: the first LFO, envelope and macro.
    bool isSourceShown (SourceId) const;
    /** Shows/hides a source (no new undo transaction is started: call beginUndoTransaction() first). */
    void setSourceShown (SourceId, bool shown);
    /** Flat indices of shown sources in display order (order they were added; routed extras appended). */
    std::vector<int> getShownSources() const;

    //==============================================================================
    // Audio thread.
    void prepare (double sampleRate, int maxBlockSize);
    /** Resets LFO phases and envelopes. */
    void reset() noexcept;
    /** Call once per processBlock (before advance). Synced LFOs lock to PPQ while playing. */
    void setTransport (const juce::AudioPlayHead::PositionInfo&) noexcept;
    /** Feed the signal for envelope follower `envelopeIndex` (whichever buffer matches
        getEnvelopeInputChoice()). Call for the same samples you are about to advance(). */
    void feedEnvelopeInput (int envelopeIndex, const float* const* channels, int numChannels, int numSamples) noexcept;
    /** Advances all sources by numSamples (block rate; call every <= 64 samples) and updates target offsets. */
    void advance (int numSamples) noexcept;

    /** base + modulation, clamped to 0..1. Returns base unchanged for unknown targets. */
    float getModulatedNormalised (int targetIndex, float baseNormalised) const noexcept;
    /** Convenience: reads the parameter's current value and returns the modulated plain (denormalised) value. */
    float getModulatedPlain (int targetIndex) const noexcept;
    /** Raw summed offset (normalised units, unclamped). */
    float getOffset (int targetIndex) const noexcept;
    bool isTargetModulated (int targetIndex) const noexcept;

    //==============================================================================
    // UI feed (any thread).
    /** Current source output: -1..1 for bipolar LFOs, 0..1 for everything else. */
    float getSourceValue (int flatIndex) const noexcept;
    /** Source output mapped to 0..1 for meters. */
    float getSourceValueNormalised (int flatIndex) const noexcept;
    bool isSourceBipolar (int flatIndex) const noexcept;
    /** Current output phase (0..1, includes the phase offset) of an LFO. */
    float getLfoPhase (int lfoIndex) const noexcept;

    /** Ring display for a knob (message thread). */
    ModulationDisplay getDisplay (const juce::String& paramId, float baseNormalised) const;

    /** Colours used by getDisplay(), indexed by SourceInfo::colourIndex (the UI sets Theme::series). */
    void setSourcePalette (const juce::Array<juce::Colour>&);
    juce::Colour getSourceColour (int flatIndex) const;

private:
    struct TargetSlot
    {
        juce::String paramId;
        juce::RangedAudioParameter* param = nullptr;
        std::atomic<float> offset { 0.0f };
        std::atomic<bool> modulated { false };
    };

    struct AudioRoute
    {
        int source = -1, target = -1;
        float depth = 0.0f;
    };

    struct Lfo
    {
        params::Ref rate, sync, division, shape, phase, unipolar;
        double phaseAcc = 0.0;
        float lastP = 0.0f;
        double lastCycle = 0.0;
        float rndFrom = 0.0f, rndTo = 0.0f;
        std::atomic<float> uiPhase { 0.0f };
    };

    struct Env
    {
        params::Ref attack, release, gain, input;
        dsp::EnvelopeFollower follower;
        float lastAttack = -1.0f, lastRelease = -1.0f;
        float level = 0.0f;
    };

    juce::AudioProcessorValueTreeState& apvts;
    const Config config;
    const int numSources;

    std::vector<std::unique_ptr<TargetSlot>> targets;
    std::unique_ptr<Lfo[]> lfos;
    std::unique_ptr<Env[]> envs;
    std::vector<params::Ref> macros;

    // Audio-thread state
    std::vector<float> sourceValues;             // per flat source
    std::vector<float> scratchOffsets;           // per target
    std::vector<std::uint8_t> scratchModulated;  // per target
    std::vector<AudioRoute> activeRoutes;
    int numActiveRoutes = 0;
    double sampleRate = 44100.0;
    bool prepared = false;
    std::uint32_t rngState = 0x9e3779b9u;

    // Transport (audio thread)
    bool transportPlaying = false, transportHasPpq = false;
    double transportPpq = 0.0, transportBpm = 120.0, quarterNotesPerBar = 4.0;
    std::int64_t samplesSinceTransport = 0;

    // Message -> audio handoff
    juce::SpinLock pendingLock;
    std::vector<AudioRoute> pendingRoutes;
    int numPendingRoutes = 0;
    std::atomic<bool> pendingDirty { false };

    // UI feed
    std::unique_ptr<std::atomic<float>[]> uiSourceValues;
    std::map<juce::String, int> targetLookup;

    // Message-thread routing cache (also written from whichever thread calls replaceState)
    mutable juce::CriticalSection routingLock;
    std::vector<Routing> routings;
    juce::Array<juce::Colour> palette;

    juce::ValueTree getModTree() const;
    juce::ValueTree getOrCreateModTree();
    juce::ValueTree findRoutingTree (SourceId, const juce::String& paramId) const;
    juce::StringArray getVisibleKeys() const;
    void rebuildFromState();
    float nextRandom() noexcept;
    void updateLfo (int i, int numSamples) noexcept;
    void applyPendingRoutes() noexcept;

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;
    void valueTreeRedirected (juce::ValueTree&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModMatrix)
};

} // namespace st::mod
