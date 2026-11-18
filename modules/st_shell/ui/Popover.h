#pragma once

namespace st
{

/** Floating editor panel (Saturn-style popovers: dynamics detail, reverb settings, source editors...).

    - The component is `margin` px larger than the visible panel on every side, so the drop shadow
      is painted inside our own bounds (never clipped into dark square corners) and clicks in the
      margin fall through to whatever is underneath.
    - Draws the frame, a title (+ optional dimmed subtitle) and a close button; Esc closes too.
    - Subclasses lay out controls in `layoutContent()` and draw extras in `paintContent()`.

    Size it with `Popover::totalSize (panelW, panelH)` and position it so the PANEL sits where you want
    (i.e. subtract `margin`). See st-sat Source/PluginEditor.cpp (togglePopover / animate) for an example.
*/
class Popover : public juce::Component
{
public:
    static constexpr int margin = 22;
    static constexpr int headerHeight = 24;

    explicit Popover (const juce::String& title, const juce::String& subtitle = {});

    static juce::Point<int> totalSize (int panelWidth, int panelHeight) noexcept { return { panelWidth + 2 * margin, panelHeight + 2 * margin }; }

    /** Called when the user clicks × or presses Esc. The owner should delete the popover (asynchronously). */
    std::function<void()> onClose;

    juce::Rectangle<int> getPanelBounds() const noexcept { return getLocalBounds().reduced (margin); }
    /** Inner area below the header row (padding applied). */
    juce::Rectangle<int> getContentBounds() const noexcept;
    /** Free space in the header row to the left of the close button (for header controls). */
    juce::Rectangle<int> getHeaderBounds() const noexcept;

    void setTitle (const juce::String& t, const juce::String& sub = {}) { title = t; subtitle = sub; repaint(); }

    void paint (juce::Graphics&) final;
    void resized() final;
    bool keyPressed (const juce::KeyPress&) override;
    bool hitTest (int x, int y) override { return getPanelBounds().contains (x, y); }

protected:
    virtual void layoutContent (juce::Rectangle<int> content) = 0;
    virtual void paintContent (juce::Graphics&) {}
    void requestClose() { if (onClose) onClose(); }

private:
    juce::String title, subtitle;
    IconButton closeButton { icons::Icon::close, "Close" };
};

} // namespace st
