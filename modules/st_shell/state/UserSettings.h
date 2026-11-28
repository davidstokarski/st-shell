#pragma once

namespace st
{

/** Preferences shared by every ST plug-in for this user (not per instance or preset), stored in
    ~/Library/Application Support/Stokarski Studios/settings.settings (or the platform equivalent).
    Message thread. Example: tooltips on / off. */
namespace userSettings
{
    bool getBool (const juce::String& key, bool defaultValue);
    void setBool (const juce::String& key, bool value);

    inline const juce::String showTooltips = "showTooltips";
    inline const juce::String capFrameRate = "capFrameRate";

    /** "Cap frame rate" (settings menu, shared by every ST plug-in): false (default) = animations and displays
        redraw at the display's own refresh rate (60 / 120 / 144 Hz ...); true = at most ~60 fps, to save power on
        high-refresh monitors. Cheap to call every frame (cached; re-read when an editor opens). */
    bool isFrameRateCapped() noexcept;
    void setFrameRateCapped (bool);
    /** Re-reads the cached settings from disk (another ST plug-in may have changed them). */
    void refreshCache();
}

} // namespace st
