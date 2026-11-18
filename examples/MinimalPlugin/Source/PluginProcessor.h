#pragma once
#include <st_shell/st_shell.h>

/** A one-knob saturator + output gain, built the ST way:
    ProcessorBase (APVTS, undo, presets, A/B, meters, analysers), params via st::params, smoothing via st::dsp. */
class ExampleProcessor : public st::ProcessorBase
{
public:
    ExampleProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;

private:
    st::params::Ref drive, output, mix;
    st::dsp::LinearSmoother driveGain, outGain, mixAmount;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
};
