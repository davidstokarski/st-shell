#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout ExampleProcessor::createLayout()
{
    using namespace st::params;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (makeFloat ("drive", "Drive", skewed (0.0f, 36.0f, 9.0f, 0.01f), 6.0f, dbAttributes()));
    layout.add (makeFloat ("output", "Output", linear (-24.0f, 24.0f, 0.01f), 0.0f, dbAttributes()));
    layout.add (makeFloat ("mix", "Mix", linear (0.0f, 100.0f, 0.01f), 100.0f, percentAttributes()));
    return layout;
}

ExampleProcessor::ExampleProcessor()
    : ProcessorBase (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                     createLayout())
{
    drive.bind (apvts, "drive");
    output.bind (apvts, "output");
    mix.bind (apvts, "mix");

    presets.addFactoryPreset ("Gentle", "Basics", { { "drive", 4.0f } });
    presets.addFactoryPreset ("Hot", "Basics", { { "drive", 24.0f }, { "output", -6.0f } });
}

void ExampleProcessor::prepareToPlay (double sampleRate, int)
{
    prepareShell (sampleRate);   // meters + analysers
    for (auto* s : { &driveGain, &outGain, &mixAmount })
        s->prepare (sampleRate, 30.0);
}

void ExampleProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    inputMeter.push (buffer, n);
    preAnalyzer.push (buffer, n);

    driveGain.setTarget (st::dsp::dbToGain (drive.get()));
    outGain.setTarget (st::dsp::dbToGain (output.get()));
    mixAmount.setTarget (mix.get() * 0.01f);

    for (int i = 0; i < n; ++i)
    {
        const float d = driveGain.getNext(), o = outGain.getNext(), m = mixAmount.getNext();
        for (int c = 0; c < buffer.getNumChannels(); ++c)
        {
            auto* x = buffer.getWritePointer (c);
            const float wet = std::tanh (x[i] * d) / std::sqrt (d);   // crude level match
            x[i] = (x[i] + m * (wet - x[i])) * o;
        }
    }

    outputMeter.push (buffer, n);
    postAnalyzer.push (buffer, n);
}

juce::AudioProcessorEditor* ExampleProcessor::createEditor() { return new ExampleEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ExampleProcessor(); }
