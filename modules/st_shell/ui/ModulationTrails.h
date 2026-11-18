#pragma once

namespace st
{

/** Glowing light trails from each ModulationStrip connector up to the knobs it modulates.

    A transparent, mouse-transparent overlay that adds itself to `root` and always covers it
    (create it after the rest of the UI so it sits on top, or call toFront (false) later;
    popovers opened afterwards stay above it). Each source's trails are rendered once into a
    small blurred image (re-rendered only when routings or layout change) and composited with
    an opacity that gently follows the source's live value, so per-frame cost is a few image
    draws in the trail area, at most ~30 times a second.

    While a source is dragged from the strip, a brighter live trail follows the mouse.
*/
class ModulationTrails : public juce::Component, private juce::ChangeListener, private juce::ComponentListener
{
public:
    ModulationTrails (mod::ModMatrix&, ModulationStrip&, juce::Component& root);
    ~ModulationTrails() override;

    /** Overall glow strength (0..2, default 1). */
    void setStrength (float);
    /** How much the glow follows the source's live value (0 = static, default 1). */
    void setLiveResponse (float);
    /** Rescans `root` for knobs (also happens automatically twice a second). */
    void refresh();

    void paint (juce::Graphics&) override;
    bool hitTest (int, int) override { return false; }

private:
    struct Link
    {
        juce::Point<float> from, to;          // connector top -> knob anchor (overlay coordinates)
        juce::Rectangle<float> dial;          // knob dial (overlay coordinates)
        bool enabled = true;
    };

    struct Glow
    {
        int source = -1;
        juce::Colour colour;
        std::vector<Link> links;
        juce::Image image;                    // blurred, low resolution
        juce::Rectangle<float> area;          // where `image` goes (overlay coordinates)
        std::uint64_t signature = 0;
        Animated presence, level;
        float requestedAlpha = -1.0f;   // alpha of the last repaint request
        bool live = false;
    };

    mod::ModMatrix& matrix;
    juce::Component::SafePointer<ModulationStrip> strip;
    juce::Component::SafePointer<juce::Component> root;
    struct KnobRef { juce::Component::SafePointer<Knob> knob; juce::String paramId; };
    std::vector<KnobRef> knobs;
    std::vector<std::unique_ptr<Glow>> glows;
    std::unique_ptr<FrameClock> clock;
    float strength = 1.0f, liveResponse = 1.0f;
    double sinceUpdate = 1.0, sinceScan = 0.0;
    bool needsScan = true;

    // live drag trail
    int dragSource = -1;
    juce::Path dragPath;
    juce::Rectangle<float> dragArea;

    void tick (double dt);
    void update (double dt);
    void updateDrag();
    bool isVisibleWithin (const juce::Component&) const;
    float alphaFor (const Glow&) const;
    Glow& glowFor (int source);
    static juce::Path makeTrailPath (const Link&);
    void render (Glow&);

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void componentMovedOrResized (juce::Component&, bool wasMoved, bool wasResized) override;
    void componentChildrenChanged (juce::Component&) override;
    void componentBeingDeleted (juce::Component&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModulationTrails)
};

} // namespace st
