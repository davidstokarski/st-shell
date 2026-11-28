#pragma once

namespace st
{

/** Vertical stereo peak/RMS meter with peak hold and clip readout. */
class LevelMeter : public juce::Component
{
public:
    explicit LevelMeter (MeterSource&, const juce::String& caption = {});

    void setRange (float minDb, float maxDb) { lo = minDb; hi = maxDb; repaint(); }
    void setShowScale (bool b) { showScale = b; repaint(); }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { maxPeakDb = -100.0f; repaint(); }

    /** Map dB -> y within the bar area. */
    float dbToY (float db, juce::Rectangle<float> bar) const;

private:
    MeterSource& source;
    juce::String caption;
    float lo = -60.0f, hi = 6.0f;
    bool showScale = false;

    std::array<float, 2> peakDb { -100.0f, -100.0f }, rmsDb { -100.0f, -100.0f }, holdDb { -100.0f, -100.0f };
    std::array<double, 2> holdTime { 0.0, 0.0 };
    float maxPeakDb = -100.0f;
    double now = 0.0;
    FrameClock clock { *this, [this] (double dt) { frame (dt); } };   // in step with the display refresh

    void frame (double dt);
};

//==============================================================================
/** Horizontal bipolar gain-change meter (upward = positive, downward = negative). */
class GainChangeMeter : public juce::Component
{
public:
    explicit GainChangeMeter (std::function<float()> getGainDb, float rangeDb = 24.0f);
    void paint (juce::Graphics&) override;
    void setColour (juce::Colour c) { colour = c; }

private:
    std::function<float()> getter;
    float range, value = 0.0f;
    juce::Colour colour;
    FrameClock clock { *this, [this] (double) { frame(); } };
    void frame();
};

} // namespace st
