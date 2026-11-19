#pragma once

namespace st::params
{

/** Ranges ------------------------------------------------------------------ */
inline juce::NormalisableRange<float> linear (float min, float max, float interval = 0.0f)
{
    return { min, max, interval };
}

/** Logarithmic (frequency-style) range. */
inline juce::NormalisableRange<float> logarithmic (float min, float max)
{
    return { min, max,
             [] (float s, float e, float t) { return s * std::pow (e / s, t); },
             [] (float s, float e, float v) { return std::log (v / s) / std::log (e / s); },
             [] (float s, float e, float v) { return juce::jlimit (s, e, v); } };
}

/** Range with a skew so that `centre` sits at the middle of the knob travel. */
inline juce::NormalisableRange<float> skewed (float min, float max, float centre, float interval = 0.0f)
{
    juce::NormalisableRange<float> r (min, max, interval);
    r.setSkewForCentre (centre);
    return r;
}

/** Text formatting ---------------------------------------------------------- */
juce::String formatHz (float hz, bool withUnit = true);
juce::String formatDb (float db, int decimals = 1, bool withUnit = true, bool showPlus = true);
juce::String formatPercent (float pct, bool bipolar = false);
juce::String formatMs (float ms);
juce::String formatRatio (float ratio);
/** "1k", "1.5 kHz", "250" and note names ("A4" = 440 Hz, "C#3+13" with cents). */
float parseHz (const juce::String&);
/** "-6", "+3.5 dB", "-inf", and amplitude ratios: "2x" = +6 dB, "50%" = -6 dB. */
float parseDb (const juce::String&);

/** Attribute presets for juce::AudioParameterFloat */
juce::AudioParameterFloatAttributes hzAttributes();
juce::AudioParameterFloatAttributes dbAttributes (int decimals = 1);
juce::AudioParameterFloatAttributes percentAttributes (bool bipolar = false);
juce::AudioParameterFloatAttributes msAttributes();
juce::AudioParameterFloatAttributes ratioAttributes();

/** Builders. All parameters use version hint 1 so hosts keep automation across updates. */
std::unique_ptr<juce::AudioParameterFloat> makeFloat (const juce::String& id, const juce::String& name,
                                                      juce::NormalisableRange<float> range, float def,
                                                      juce::AudioParameterFloatAttributes attrs = {});
std::unique_ptr<juce::AudioParameterBool>  makeBool (const juce::String& id, const juce::String& name, bool def);
std::unique_ptr<juce::AudioParameterChoice> makeChoice (const juce::String& id, const juce::String& name,
                                                        const juce::StringArray& choices, int def);

/** Cached raw-value pointer for fast audio-thread reads. */
struct Ref
{
    std::atomic<float>* ptr = nullptr;
    juce::RangedAudioParameter* param = nullptr;

    void bind (juce::AudioProcessorValueTreeState& s, const juce::String& id)
    {
        ptr = s.getRawParameterValue (id);
        param = s.getParameter (id);
        jassert (ptr != nullptr && param != nullptr);
    }

    float get() const noexcept      { return ptr->load (std::memory_order_relaxed); }
    int   getInt() const noexcept   { return (int) std::lround (get()); }
    bool  getBool() const noexcept  { return get() >= 0.5f; }
};

} // namespace st::params
