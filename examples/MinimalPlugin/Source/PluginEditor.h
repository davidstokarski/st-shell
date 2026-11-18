#pragma once
#include "PluginProcessor.h"

/** Shows the core ST UI vocabulary: full-window backdrop + floating header (from EditorBase), a ringed hero
    knob, classic knobs, a popover opened from "•••", a flat footer tray, cached static components. */
class ExampleEditor : public st::EditorBase
{
public:
    explicit ExampleEditor (ExampleProcessor&);

    static constexpr int designWidth = 640, designHeight = 400;

private:
    struct InfoPopover : st::Popover
    {
        InfoPopover() : st::Popover ("About", "ST Example") {}
        void layoutContent (juce::Rectangle<int>) override {}
        void paintContent (juce::Graphics& g) override
        {
            g.setColour (st::Theme::get().textDim);
            g.setFont (st::fonts::regular (13.0f));
            g.drawFittedText ("Popovers come from st::Popover: shadow margin, title, close button and Esc are built in.",
                              getContentBounds(), juce::Justification::topLeft, 4);
        }
    };

    struct Footer : juce::Component
    {
        explicit Footer (ExampleProcessor& p) : mix (p.param ("mix"), &p.undoManager, "Mix"), out (p.param ("output"), &p.undoManager, "Out")
        {
            mix.setFlatStyle (true);
            out.setFlatStyle (true);
            addAndMakeVisible (mix);
            addAndMakeVisible (out);
        }
        void paint (juce::Graphics& g) override { st::paint::tray (g, getLocalBounds().toFloat()); }
        void resized() override
        {
            auto r = getLocalBounds().reduced (24, 8);
            out.setBounds (r.removeFromRight (100));
            mix.setBounds (r.removeFromRight (100));
        }
        st::NumberBox mix, out;
    };

    st::Knob drive, mix;
    st::FlatButton info { "", "About" };
    Footer footer;
    std::unique_ptr<InfoPopover> popover;

    void layout (juce::Rectangle<int> body) override;
};
