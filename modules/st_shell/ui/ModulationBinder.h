#pragma once

namespace st
{

/** Connects every st::Knob under a root component to a mod::ModMatrix.

    For each knob whose parameter was registered with ModMatrix::addTarget() it:
    - draws modulation rings (Knob::setModulationSource -> ModMatrix::getDisplay),
    - accepts modulation-source drops from a ModulationPanel (creates a routing),
    - adds a "Modulation" section to the knob's right-click menu (remove / add routings).

    Create it in the editor after the knobs exist; it rescans the hierarchy twice a second
    (or call refresh() after building UI dynamically). Knobs inside a ModulationPanel are skipped.
    Previous extendContextMenu handlers are chained, and restored when the binder is destroyed.
*/
class ModulationBinder : private juce::Timer
{
public:
    ModulationBinder (mod::ModMatrix&, juce::Component& root, ModulationPanel* panel = nullptr);
    ~ModulationBinder() override;

    /** Lets the panel highlight drop targets while a source is dragged and selects the source on drop. */
    void attachPanel (ModulationPanel*);
    /** Scans the root component for new knobs. */
    void refresh();
    /** Periodic rescans (default on). */
    void setAutoRefresh (bool);

    /** Depth for routings created by drag-and-drop or the context menu. */
    float defaultDepth = 0.25f;
    /** Called after a routing was created from the UI. */
    std::function<void (mod::SourceId, const juce::String& paramId)> onRoutingCreated;

private:
    struct Binding
    {
        juce::Component::SafePointer<Knob> knob;
        juce::String paramId;
        std::function<void (juce::PopupMenu&)> previousMenu;
    };

    mod::ModMatrix& matrix;
    juce::Component::SafePointer<juce::Component> root;
    juce::Component::SafePointer<ModulationPanel> panel;
    std::vector<Binding> bindings;
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);
    int dragSource = -1;
    bool autoRefresh = true;

    void bind (Knob&, const juce::String& paramId);
    void unbind (Binding&);
    void createRouting (int sourceIndex, const juce::String& paramId);
    void setDragSource (int sourceIndex);
    void addMenuItems (juce::PopupMenu&, const juce::String& paramId);
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModulationBinder)
};

} // namespace st
