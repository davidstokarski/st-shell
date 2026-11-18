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
                   const juce::String& companyName = "ST Audio");
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

protected:
    /** Call at the start of prepareToPlay to set up meters and analysers. */
    void prepareShell (double sampleRate);

private:
    JUCE_DECLARE_WEAK_REFERENCEABLE (ProcessorBase)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProcessorBase)
};

} // namespace st
