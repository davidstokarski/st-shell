#pragma once

#include "SharedInstances.h"

namespace st
{

class ProcessorBase;

/** Every ST plug-in instance in the process — this plug-in's and the other ST plug-ins' (through the shared
    registry, see SharedInstances.h) — for cross-instance features: the multiview (st::MultiView), editing
    another instance, collision analysis, sharing spectra.

    ProcessorBase registers itself on construction and unregisters on destruction, so a plug-in gets this for
    free. Instances are identified by a process-unique `uid`. Each has a kind (plug-in name), a number per
    kind (1, 2, 3... in creation order), and the host's track name / colour (updateTrackProperties).

    Instances of this binary are also available as ProcessorBase* (`getInstances`, Info::local). Instances of
    other binaries are reached only through their C api (Info::api / ctx, used by MultiView's remote modules).

    Threading: message thread. Listeners are told synchronously on the message thread when an instance goes
    away (`instanceRemoved`: drop everything that refers to it, never call into it again) and asynchronously
    about everything else (`instancesChanged`). */
class InstanceRegistry : private juce::AsyncUpdater
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        /** Instances were added, or a track name / colour changed. */
        virtual void instancesChanged() = 0;
        /** Instance `uid` is gone or about to be destroyed (`local` is set when it's one of this binary's). */
        virtual void instanceRemoved (uint64_t uid, ProcessorBase* local) { juce::ignoreUnused (uid, local); instancesChanged(); }
    };

    struct Info
    {
        uint64_t uid = 0;
        juce::String kind, track, label;   // label: track name, or "<kind> <number>"
        juce::Colour trackColour;          // transparent if the host doesn't say
        int number = 0;
        ProcessorBase* local = nullptr;    // set for instances of this binary
        const shared::InstanceApi* api = nullptr;
        void* ctx = nullptr;
    };

    /** This binary's registry (lives while the binary is loaded). */
    static InstanceRegistry& get();

    /** This binary's instances, in creation order. */
    juce::Array<ProcessorBase*> getInstances() const;
    /** Every ST instance in the process (all plug-ins), in creation order. */
    std::vector<Info> getAllInstances() const;
    int getNumInstances() const;   // all plug-ins
    bool isAlive (uint64_t uid) const;
    uint64_t getUid (const ProcessorBase*) const;

    void addListener (Listener* l)    { listeners.add (l); }
    void removeListener (Listener* l) { listeners.remove (l); }

    // called by ProcessorBase
    int add (ProcessorBase*);
    void remove (ProcessorBase*);
    void changed();
    /** Delivers pending (async) change notifications now (tests, offline use). */
    void flush() { handleUpdateNowIfNeeded(); }

    ~InstanceRegistry() override;

private:
    InstanceRegistry();
    mutable juce::CriticalSection lock;
    juce::Array<ProcessorBase*> instances;
    std::map<const ProcessorBase*, uint64_t> uids;
    std::map<uint64_t, ProcessorBase*> removedLocal;
    std::vector<uint64_t> pendingRemovals;
    int nextNumber = 1;
    juce::ListenerList<Listener> listeners;

    static void sharedListener (void* self, int event, uint64_t uid);
    void handleShared (int event, uint64_t uid);
    void dispatchRemovals();
    void handleAsyncUpdate() override;
};

} // namespace st
