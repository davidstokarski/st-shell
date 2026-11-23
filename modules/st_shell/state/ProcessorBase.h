#pragma once

namespace st
{

/** Base class for every ST plugin processor.

    Gives you: APVTS with undo, presets, A/B, input/output meters, pre/post
    analyser feeds, session-only UI state, state save/restore and the usual
    AudioProcessor boilerplate. Subclasses implement prepare/process/editor.
*/
class ProcessorBase : public juce::AudioProcessor
{
public:
    ProcessorBase (const BusesProperties&, juce::AudioProcessorValueTreeState::ParameterLayout,
                   const juce::String& companyName = kCompanyName);
    ~ProcessorBase() override;

    //==============================================================================
    juce::UndoManager undoManager { 60000, 60 };
    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets;
    ABCompare ab;

    MeterSource inputMeter, outputMeter;
    AnalyzerSource preAnalyzer, postAnalyzer;

    /** Session-only UI state (editor scale, selected tab...). Message thread only. Not part of presets. */
    juce::ValueTree uiState { "UI" };

    /** Parameter by ID (asserts if it doesn't exist). */
    juce::RangedAudioParameter& param (const juce::String& id)
    {
        auto* p = apvts.getParameter (id);
        jassert (p != nullptr);
        return *p;
    }

    float getUiScale() const { return (float) (double) uiState.getProperty ("scale", 1.0); }
    void setUiScale (float s) { uiState.setProperty ("scale", s, nullptr); }

    //==============================================================================
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return presets.getCurrentName(); }
    void changeProgramName (int, const juce::String&) override {}

    bool isBusesLayoutSupported (const BusesLayout&) const override;

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    /** Called on the message thread after state was restored from the host or a preset. */
    virtual void stateRestored() {}

    //==============================================================================
    /** Cross-instance identity (see InstanceRegistry): session number, host track name / colour. */
    int getInstanceNumber() const noexcept { return instanceNumber; }
    juce::String getTrackName() const;
    juce::Colour getTrackColour() const;
    /** Track name, or "<plugin> <n>" when the host doesn't report one. */
    juce::String getInstanceLabel() const;
    void updateTrackProperties (const TrackProperties&) override;
    /** Leaves the instance registry now (call first thing in a subclass destructor if UI elsewhere may be
        using subclass members of this instance). Safe to call twice. */
    void unregisterInstance();
    /** Process-unique id in the instance registry (0 if not registered). */
    uint64_t getInstanceUid() const;

    /** The compact module this instance shows in the multiview (st::MultiView), or null for none. It may be
        created by another plug-in's multiview and is then rendered off screen and driven by forwarded mouse
        events: keep it to knobs, displays and buttons (no text entry), and don't capture the mouse. */
    virtual std::unique_ptr<juce::Component> createStripView() { return nullptr; }
    /** Width of the module at a given height. */
    virtual int getStripWidth (int height) const { return height * 2; }

protected:
    /** Call at the start of prepareToPlay to set up meters and analysers. */
    void prepareShell (double sampleRate);

private:
    int instanceNumber = 0;
    mutable juce::CriticalSection trackLock;
    TrackProperties track;

    JUCE_DECLARE_WEAK_REFERENCEABLE (ProcessorBase)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProcessorBase)
};

} // namespace st
