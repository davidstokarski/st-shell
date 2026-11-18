#include "PluginEditor.h"

ExampleEditor::ExampleEditor (ExampleProcessor& p)
    : EditorBase (p, designWidth, designHeight),
      drive (p.param ("drive"), &p.undoManager, "Drive"),
      mix (p.param ("mix"), &p.undoManager, "Mix"),
      footer (p)
{
    header.setProductName ("Example");

    drive.setSize (st::Knob::Size::large);
    drive.setLook (st::Knob::Look::ringed);      // hero control: coloured rim
    drive.setColour (st::Theme::get().accent);
    mix.setSize (st::Knob::Size::medium);         // supporting control: classic matte knob

    info.setButtonText (juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xa2\xe2\x80\xa2\xe2\x80\xa2")));
    info.onClick = [this]
    {
        if (popover != nullptr) { popover.reset(); return; }
        popover = std::make_unique<InfoPopover>();
        popover->onClose = [this] { juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<ExampleEditor> (this)] { if (safe) safe->popover.reset(); }); };
        getContent().addAndMakeVisible (*popover);
        const auto size = st::Popover::totalSize (360, 120);
        popover->setBounds (designWidth / 2 - size.x / 2, 80 - st::Popover::margin, size.x, size.y);
        popover->grabKeyboardFocus();
    };

    auto& content = getContent();
    for (auto* c : { (juce::Component*) &drive, (juce::Component*) &mix, (juce::Component*) &info, (juce::Component*) &footer })
        content.addAndMakeVisible (c);

    footer.setBufferedToImage (true);   // static: cache it so live repaints elsewhere stay cheap
    header.toFront (false);
    relayout();                          // always call at the end of the constructor
}

void ExampleEditor::layout (juce::Rectangle<int> body)
{
    footer.setBounds (getContent().getLocalBounds().removeFromBottom (40));
    auto r = body.withTrimmedBottom (40).reduced (40);
    drive.setBounds (r.withSizeKeepingCentre (130, 150).translated (-90, 0));
    mix.setBounds (r.withSizeKeepingCentre (80, 96).translated (90, 10));
    info.setBounds (mix.getRight() - 4, mix.getY() - 6, 22, 14);
}
