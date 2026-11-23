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
}

} // namespace st
