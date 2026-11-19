namespace st::params
{

juce::String formatHz (float hz, bool withUnit)
{
    if (hz >= 10000.0f) return juce::String (hz / 1000.0f, 1) + (withUnit ? " kHz" : "k");
    if (hz >= 1000.0f)  return juce::String (hz / 1000.0f, 2) + (withUnit ? " kHz" : "k");
    if (hz >= 100.0f)   return juce::String (juce::roundToInt (hz)) + (withUnit ? " Hz" : "");
    return juce::String (hz, 1) + (withUnit ? " Hz" : "");
}

juce::String formatDb (float db, int decimals, bool withUnit, bool showPlus)
{
    if (db <= -99.5f) return juce::String ("-inf") + (withUnit ? " dB" : "");
    juce::String s (db, decimals);
    if (showPlus && db > 0.0f && s.getFloatValue() != 0.0f) s = "+" + s;
    if (s == "-0" || s == "-0.0" || s == "-0.00") s = s.substring (1);
    return s + (withUnit ? " dB" : "");
}

juce::String formatPercent (float pct, bool bipolar)
{
    const int v = juce::roundToInt (pct);
    return (bipolar && v > 0 ? "+" : "") + juce::String (v) + "%";
}

juce::String formatMs (float ms)
{
    if (ms >= 1000.0f) return juce::String (ms / 1000.0f, 2) + " s";
    if (ms >= 100.0f)  return juce::String (juce::roundToInt (ms)) + " ms";
    if (ms >= 10.0f)   return juce::String (ms, 1) + " ms";
    return juce::String (ms, 2) + " ms";
}

juce::String formatRatio (float r)
{
    if (r >= 99.5f) return juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x9e:1"));
    return juce::String (r, r < 10.0f ? 1 : 0) + ":1";
}

float parseHz (const juce::String& text)
{
    auto t = text.trim().toLowerCase().removeCharacters (" ");
    if (t.isEmpty()) return 0.0f;

    // note names (A4 = 440 Hz), optional cents: "C#3", "Bb2-20", "a4+13"
    if (t[0] >= 'a' && t[0] <= 'g')
    {
        static const int base[] = { 9, 11, 0, 2, 4, 5, 7 };   // a b c d e f g
        int semis = base[t[0] - 'a'];
        int i = 1;
        if (t[i] == '#') { ++semis; ++i; }
        else if (t[i] == 'b' && i + 1 < t.length() && (juce::CharacterFunctions::isDigit (t[i + 1]) || t[i + 1] == '-')) { --semis; ++i; }
        int j = i;
        if (t[j] == '-') ++j;
        while (j < t.length() && juce::CharacterFunctions::isDigit (t[j])) ++j;
        if (j > i && juce::CharacterFunctions::isDigit (t[j - 1]))
        {
            const int octave = t.substring (i, j).getIntValue();
            const float cents = j < t.length() ? t.substring (j).getFloatValue() : 0.0f;
            const float midi = (float) ((octave + 1) * 12 + semis) + cents * 0.01f;
            return 440.0f * std::exp2 ((midi - 69.0f) / 12.0f);
        }
    }

    float v = t.retainCharacters ("0123456789.").getFloatValue();
    if (t.containsChar ('k')) v *= 1000.0f;
    return v;
}

float parseDb (const juce::String& text)
{
    auto t = text.trim().toLowerCase().removeCharacters (" ");
    if (t.startsWith ("-inf")) return -100.0f;
    const float v = t.retainCharacters ("-+.0123456789").getFloatValue();
    // amplitude ratios: "2x" = +6 dB, "0.5x" = -6 dB, "50%" = -6 dB
    if (t.endsWithChar ('x') || t.endsWithChar ('%'))
    {
        const float ratio = t.endsWithChar ('%') ? v * 0.01f : v;
        return ratio > 0.0f ? 20.0f * std::log10 (ratio) : -100.0f;
    }
    return v;
}

juce::AudioParameterFloatAttributes hzAttributes()
{
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return formatHz (v); })
        .withValueFromStringFunction ([] (const juce::String& s) { return parseHz (s); })
        .withLabel ("Hz");
}

juce::AudioParameterFloatAttributes dbAttributes (int decimals)
{
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction ([decimals] (float v, int) { return formatDb (v, decimals); })
        .withValueFromStringFunction ([] (const juce::String& s) { return parseDb (s); })
        .withLabel ("dB");
}

juce::AudioParameterFloatAttributes percentAttributes (bool bipolar)
{
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction ([bipolar] (float v, int) { return formatPercent (v, bipolar); })
        .withValueFromStringFunction ([] (const juce::String& s) { return s.retainCharacters ("-+.0123456789").getFloatValue(); })
        .withLabel ("%");
}

juce::AudioParameterFloatAttributes msAttributes()
{
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return formatMs (v); })
        .withValueFromStringFunction ([] (const juce::String& s)
        {
            auto t = s.trim().toLowerCase();
            float v = t.retainCharacters ("-.0123456789").getFloatValue();
            if (t.endsWith ("s") && ! t.endsWith ("ms")) v *= 1000.0f;
            return v;
        })
        .withLabel ("ms");
}

juce::AudioParameterFloatAttributes ratioAttributes()
{
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return formatRatio (v); })
        .withValueFromStringFunction ([] (const juce::String& s)
        {
            if (s.containsIgnoreCase ("inf") || s.contains (juce::CharPointer_UTF8 ("\xe2\x88\x9e"))) return 100.0f;
            return s.upToFirstOccurrenceOf (":", false, false).getFloatValue();
        });
}

std::unique_ptr<juce::AudioParameterFloat> makeFloat (const juce::String& id, const juce::String& name,
                                                      juce::NormalisableRange<float> range, float def,
                                                      juce::AudioParameterFloatAttributes attrs)
{
    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, def, attrs);
}

std::unique_ptr<juce::AudioParameterBool> makeBool (const juce::String& id, const juce::String& name, bool def)
{
    return std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def);
}

std::unique_ptr<juce::AudioParameterChoice> makeChoice (const juce::String& id, const juce::String& name,
                                                        const juce::StringArray& choices, int def)
{
    return std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, choices, def);
}

} // namespace st::params
