#pragma once

namespace st
{

/** Factory + user preset handling.

    Convention: everything that belongs in a preset lives inside apvts.state
    (parameters plus any child trees, e.g. modulation routing). Session-only UI
    state is kept elsewhere (see ProcessorBase::uiState).

    User presets are XML files with the extension ".stpreset", stored in
    ~/Library/Audio/Presets/<Company>/<Product>/[Category/]Name.stpreset
*/
class PresetManager : public juce::ChangeBroadcaster
{
public:
    struct Preset
    {
        juce::String name, category;
        bool isFactory = false;
        juce::File file;          // user presets only
        juce::ValueTree state;    // factory presets only (user presets load lazily)
    };

    PresetManager (juce::AudioProcessorValueTreeState&, const juce::String& companyName, const juce::String& productName);

    /** Call from the processor constructor (after the APVTS exists). Values are plain (denormalised) parameter values;
        anything not listed keeps its default. The optional lambda may edit non-parameter child trees. */
    void addFactoryPreset (const juce::String& name, const juce::String& category,
                           std::initializer_list<std::pair<juce::String, float>> values,
                           std::function<void (juce::ValueTree&)> customise = {});
    void addFactoryPreset (const juce::String& name, const juce::String& category,
                           const std::vector<std::pair<juce::String, float>>& values,
                           std::function<void (juce::ValueTree&)> customise = {});

    /** All presets: factory first (in registration order), then user presets sorted by category/name. */
    const std::vector<Preset>& getPresets() const noexcept { return presets; }
    void rescanUserPresets();

    bool loadPreset (int index);
    bool loadPresetFile (const juce::File&);
    void loadDefault();          // "Init"
    void loadNext();
    void loadPrevious();

    bool saveUserPreset (const juce::String& name, const juce::String& category = {});
    bool deleteUserPreset (int index);

    juce::String getCurrentName() const;
    int getCurrentIndex() const noexcept { return currentIndex; }
    /** True when parameters differ from the last loaded/saved preset. Cheap enough for a UI timer. */
    bool isModified() const;

    juce::File getUserDirectory() const { return userDir; }
    static constexpr const char* fileExtension = ".stpreset";

    /** Called after any preset is loaded (message thread). */
    std::function<void()> onPresetLoaded;

    /** Parameter IDs that belong to the instance rather than the sound (e.g. a display mode): loading a preset keeps
        their current values, and changing them doesn't mark the preset as modified. */
    juce::StringArray keepParameters;

    /** Re-sync after the host restores a session (keeps the stored preset name). */
    void sessionRestored();

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::String product;
    juce::File userDir;
    juce::ValueTree defaultState;
    std::vector<Preset> presets;
    int numFactory = 0;
    int currentIndex = -1;
    std::vector<float> snapshot;

    void applyState (const juce::ValueTree&, const juce::String& name, int index);
    void takeSnapshot();
    static juce::ValueTree readFile (const juce::File&, juce::String* nameOut = nullptr);
};

//==============================================================================
/** A/B comparison slots. */
class ABCompare : public juce::ChangeBroadcaster
{
public:
    explicit ABCompare (juce::AudioProcessorValueTreeState& s) : apvts (s) {}

    /** Parameter IDs that switching A / B leaves as they are (see PresetManager::keepParameters). */
    juce::StringArray keepParameters;

    int getActive() const noexcept { return active; }
    void select (int slot);          // 0 = A, 1 = B
    void toggle() { select (1 - active); }
    void copyActiveToOther();

private:
    juce::AudioProcessorValueTreeState& apvts;
    std::array<juce::ValueTree, 2> slots;
    int active = 0;
};

} // namespace st
