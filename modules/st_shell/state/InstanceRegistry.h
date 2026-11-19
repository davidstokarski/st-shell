#pragma once

namespace st
{

class ProcessorBase;

/** Every ST plugin instance of this plugin binary in this process, for cross-instance features:
    instance lists, editing another instance's parameters, collision analysis between tracks,
    using another instance's spectrum.

    ProcessorBase registers itself on construction and unregisters on destruction, so a plugin gets this for
    free. Each instance has a session-unique number (1, 2, 3... in creation order) and the track name /
    colour the host reports (AudioProcessor::updateTrackProperties).

    Threading: message thread. Listeners are called synchronously on the message thread for removals (so UI
    holding another instance can drop it before it is destroyed) and asynchronously for everything else.
    If an instance is destroyed off the message thread the removal is announced asynchronously instead —
    UI should therefore also hold instances through juce::WeakReference<ProcessorBase>. */
class InstanceRegistry : private juce::AsyncUpdater
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        /** The list, a track name or a track colour changed. */
        virtual void instancesChanged() = 0;
        /** `instance` is about to be destroyed: drop every reference to it now. */
        virtual void instanceRemoved (ProcessorBase* instance) { juce::ignoreUnused (instance); instancesChanged(); }
    };

    /** The process-wide registry (lives while any instance or holder exists). */
    static InstanceRegistry& get();

    /** Snapshot of the registered instances, in creation order. */
    juce::Array<ProcessorBase*> getInstances() const;
    int getNumInstances() const;

    void addListener (Listener* l)    { listeners.add (l); }
    void removeListener (Listener* l) { listeners.remove (l); }

    // called by ProcessorBase
    int add (ProcessorBase*);
    void remove (ProcessorBase*);
    void changed() { triggerAsyncUpdate(); }
    /** Delivers a pending (async) change notification now (tests, offline use). */
    void flush() { handleUpdateNowIfNeeded(); }

private:
    InstanceRegistry() = default;
    mutable juce::CriticalSection lock;
    juce::Array<ProcessorBase*> instances;
    int nextNumber = 1;
    juce::ListenerList<Listener> listeners;
    void handleAsyncUpdate() override { listeners.call ([] (Listener& l) { l.instancesChanged(); }); }
};

} // namespace st
