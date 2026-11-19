namespace st
{

namespace
{
    bool onMessageThread()
    {
        auto* mm = juce::MessageManager::getInstanceWithoutCreating();
        return mm != nullptr && mm->isThisTheMessageThread();
    }

    struct SharedLock
    {
        shared::Registry& r;
        explicit SharedLock (shared::Registry& reg) : r (reg) { pthread_mutex_lock (&r.lock); }
        ~SharedLock() { pthread_mutex_unlock (&r.lock); }
    };

    void notifyShared (int event, uint64_t uid)
    {
        auto& r = shared::getSharedRegistry();
        shared::ListenerFn fns[shared::kMaxListeners];
        void* data[shared::kMaxListeners];
        int n;
        {
            SharedLock sl (r);
            n = r.numListeners;
            for (int i = 0; i < n; ++i) { fns[i] = r.listeners[i]; data[i] = r.listenerData[i]; }
        }
        for (int i = 0; i < n; ++i) fns[i] (data[i], event, uid);
    }

    void copyString (const juce::String& s, char* dest, int size)
    {
        if (dest == nullptr || size <= 0) return;
        s.copyToUTF8 (dest, (size_t) size);
    }

    //==============================================================================
    // Module views this binary serves to other binaries: rendered off screen, driven by forwarded mouse events.
    struct StripHost : public juce::Component
    {
        std::unique_ptr<juce::Component> content;
        bool wheelUnhandled = false;
        void resized() override { if (content != nullptr) content->setBounds (getLocalBounds()); }
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override { wheelUnhandled = true; }
    };

    struct View
    {
        std::unique_ptr<StripHost> host;
        ProcessorBase* owner = nullptr;
        juce::Component::SafePointer<juce::Component> hover, down;
        juce::Point<float> downPos;
        juce::Time downTime;
    };

    struct ViewServer
    {
        juce::CriticalSection lock;
        std::map<uint64_t, View> views;
        uint64_t nextId = 1;

        static ViewServer& get() { static ViewServer s; return s; }

        /** Deletes every view of `owner` (on the message thread, or with it locked). */
        void dropViewsOf (ProcessorBase* owner)
        {
            std::vector<std::unique_ptr<StripHost>> dead;
            {
                const juce::ScopedLock sl (lock);
                for (auto it = views.begin(); it != views.end();)
                {
                    if (it->second.owner == owner) { dead.push_back (std::move (it->second.host)); it = views.erase (it); }
                    else ++it;
                }
            }
            if (dead.empty()) return;
            if (onMessageThread()) { dead.clear(); return; }
            const juce::MessageManagerLock mml;
            dead.clear();
        }
    };

    juce::ModifierKeys toModifiers (uint32_t m)
    {
        int f = 0;
        if (m & shared::modShift) f |= juce::ModifierKeys::shiftModifier;
        if (m & shared::modCtrl)  f |= juce::ModifierKeys::ctrlModifier;
        if (m & shared::modAlt)   f |= juce::ModifierKeys::altModifier;
        if (m & shared::modCmd)   f |= juce::ModifierKeys::commandModifier;
        if (m & shared::modLeft)  f |= juce::ModifierKeys::leftButtonModifier;
        if (m & shared::modRight) f |= juce::ModifierKeys::rightButtonModifier;
        return juce::ModifierKeys (f);
    }

    // --- the C api of this binary's instances -------------------------------------------------------------
    void apiGetInfo (void* ctx, char* kind, int kindSize, char* track, int trackSize, uint32_t* argb, int* number)
    {
        auto* p = static_cast<ProcessorBase*> (ctx);
        copyString (p->getName(), kind, kindSize);
        copyString (p->getTrackName(), track, trackSize);
        if (argb != nullptr) *argb = p->getTrackColour().getARGB();
        if (number != nullptr) *number = p->getInstanceNumber();
    }

    uint64_t apiCreateView (void* ctx)
    {
        auto* p = static_cast<ProcessorBase*> (ctx);
        auto content = p->createStripView();
        if (content == nullptr) return 0;
        auto host = std::make_unique<StripHost>();
        host->addAndMakeVisible (*content);
        host->content = std::move (content);
        host->setSize (400, 120);
        host->setVisible (true);   // hit-testing (getComponentAt) skips invisible components
        auto& s = ViewServer::get();
        const juce::ScopedLock sl (s.lock);
        const uint64_t id = s.nextId++;
        auto& v = s.views[id];
        v.host = std::move (host);
        v.owner = p;
        return id;
    }

    void apiDestroyView (uint64_t id)
    {
        std::unique_ptr<StripHost> dead;
        {
            auto& s = ViewServer::get();
            const juce::ScopedLock sl (s.lock);
            auto it = s.views.find (id);
            if (it == s.views.end()) return;
            dead = std::move (it->second.host);
            s.views.erase (it);
        }
    }

    int apiPreferredWidth (uint64_t id, int height)
    {
        auto& s = ViewServer::get();
        const juce::ScopedLock sl (s.lock);
        auto it = s.views.find (id);
        return it == s.views.end() ? 0 : it->second.owner->getStripWidth (height);
    }

    void apiSetViewSize (uint64_t id, int w, int h)
    {
        auto& s = ViewServer::get();
        const juce::ScopedLock sl (s.lock);
        if (auto it = s.views.find (id); it != s.views.end()) it->second.host->setSize (juce::jmax (1, w), juce::jmax (1, h));
    }

    int apiRender (uint64_t id, uint32_t* pixels, int pw, int ph, int stride, float scale)
    {
        auto& s = ViewServer::get();
        const juce::ScopedLock sl (s.lock);
        auto it = s.views.find (id);
        if (it == s.views.end() || pixels == nullptr || pw <= 0 || ph <= 0) return 0;

        juce::Image img (juce::Image::ARGB, pw, ph, true, juce::SoftwareImageType());
        {
            juce::Graphics g (img);
            g.addTransform (juce::AffineTransform::scale (scale));
            it->second.host->paintEntireComponent (g, true);
        }
        const juce::Image::BitmapData bd (img, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < ph; ++y)
            std::memcpy (reinterpret_cast<uint8_t*> (pixels) + (size_t) y * (size_t) stride, bd.getLinePointer (y), (size_t) pw * 4);
        return 1;
    }

    int apiMouse (uint64_t id, int type, float x, float y, uint32_t mods, float wheelX, float wheelY, int clicks)
    {
        auto& s = ViewServer::get();
        const juce::ScopedLock sl (s.lock);
        auto it = s.views.find (id);
        if (it == s.views.end()) return 0;
        auto& v = it->second;
        auto* host = v.host.get();
        const juce::Point<float> pos (x, y);
        const auto modifiers = toModifiers (mods);
        const auto now = juce::Time::getCurrentTime();
        auto source = juce::Desktop::getInstance().getMainMouseSource();

        auto event = [&] (juce::Component* target, int numClicks, bool dragged)
        {
            const auto local = target->getLocalPoint (host, pos);
            const auto downLocal = target->getLocalPoint (host, v.downPos);
            return juce::MouseEvent (source, local, modifiers, juce::MouseInputSource::defaultPressure, 0.0f, 0.0f, 0.0f, 0.0f,
                                     target, target, now, downLocal, v.downTime, juce::jmax (1, numClicks), dragged);
        };
        auto targetAt = [&] () -> juce::Component*
        {
            auto* c = host->getComponentAt (pos.toInt());
            return c != nullptr ? c : host;
        };

        switch (type)
        {
            case shared::mouseMove:
            {
                auto* t = targetAt();
                if (t != v.hover.getComponent())
                {
                    if (auto* h = v.hover.getComponent()) h->mouseExit (event (h, 1, false));
                    v.hover = t;
                    t->mouseEnter (event (t, 1, false));
                }
                t->mouseMove (event (t, 1, false));
                return 1;
            }
            case shared::mouseExit:
                if (auto* h = v.hover.getComponent()) h->mouseExit (event (h, 1, false));
                v.hover = nullptr;
                return 1;
            case shared::mouseDown:
            {
                auto* t = targetAt();
                v.down = t;
                v.downPos = pos;
                v.downTime = now;
                t->mouseDown (event (t, clicks, false));
                return 1;
            }
            case shared::mouseDrag:
                if (auto* d = v.down.getComponent()) d->mouseDrag (event (d, clicks, true));
                return 1;
            case shared::mouseUp:
                if (auto* d = v.down.getComponent()) d->mouseUp (event (d, clicks, pos.getDistanceFrom (v.downPos) > 2.0f));
                v.down = nullptr;
                return 1;
            case shared::mouseDoubleClick:
            {
                auto* t = targetAt();
                t->mouseDoubleClick (event (t, 2, false));
                return 1;
            }
            case shared::mouseWheel:
            {
                auto* t = targetAt();
                host->wheelUnhandled = false;
                juce::MouseWheelDetails w { wheelX, wheelY, false, false, false };
                if (t == host) return 0;
                t->mouseWheelMove (event (t, 1, false), w);
                return host->wheelUnhandled ? 0 : 1;
            }
            default:
                return 0;
        }
    }

    const shared::InstanceApi localApi { (uint32_t) sizeof (shared::InstanceApi), apiGetInfo, apiCreateView, apiDestroyView,
                                         apiPreferredWidth, apiSetViewSize, apiRender, apiMouse };
}

//==============================================================================
InstanceRegistry& InstanceRegistry::get()
{
    // Function-local static: created by the first instance, destroyed when the binary unloads
    // (after every instance has been destroyed).
    static InstanceRegistry registry;
    return registry;
}

InstanceRegistry::InstanceRegistry()
{
    auto& r = shared::getSharedRegistry();
    SharedLock sl (r);
    if (r.numListeners < shared::kMaxListeners)
    {
        r.listeners[r.numListeners] = &InstanceRegistry::sharedListener;
        r.listenerData[r.numListeners] = this;
        ++r.numListeners;
    }
}

InstanceRegistry::~InstanceRegistry()
{
    cancelPendingUpdate();
    auto& r = shared::getSharedRegistry();
    SharedLock sl (r);
    for (int i = 0; i < r.numListeners; ++i)
        if (r.listenerData[i] == this)
        {
            for (int j = i; j < r.numListeners - 1; ++j) { r.listeners[j] = r.listeners[j + 1]; r.listenerData[j] = r.listenerData[j + 1]; }
            --r.numListeners;
            break;
        }
}

juce::Array<ProcessorBase*> InstanceRegistry::getInstances() const
{
    const juce::ScopedLock sl (lock);
    return instances;
}

std::vector<InstanceRegistry::Info> InstanceRegistry::getAllInstances() const
{
    std::vector<shared::Instance> all;
    {
        auto& r = shared::getSharedRegistry();
        SharedLock sl (r);
        all.assign (r.instances, r.instances + r.count);
    }
    std::vector<Info> out;
    out.reserve (all.size());
    for (auto& e : all)
    {
        Info info;
        info.uid = e.uid;
        info.api = e.api;
        info.ctx = e.ctx;
        info.local = e.api == &localApi ? static_cast<ProcessorBase*> (e.ctx) : nullptr;
        char kind[64] = {}, track[256] = {};
        uint32_t argb = 0;
        int number = 0;
        if (e.api != nullptr && e.api->getInfo != nullptr) e.api->getInfo (e.ctx, kind, (int) sizeof (kind), track, (int) sizeof (track), &argb, &number);
        info.kind = juce::String::fromUTF8 (kind);
        info.track = juce::String::fromUTF8 (track);
        info.trackColour = juce::Colour (argb);
        info.number = number;
        info.label = info.track.isNotEmpty() ? info.track : info.kind + " " + juce::String (number);
        out.push_back (std::move (info));
    }
    return out;
}

int InstanceRegistry::getNumInstances() const
{
    auto& r = shared::getSharedRegistry();
    SharedLock sl (r);
    return r.count;
}

bool InstanceRegistry::isAlive (uint64_t uid) const
{
    auto& r = shared::getSharedRegistry();
    SharedLock sl (r);
    for (int i = 0; i < r.count; ++i)
        if (r.instances[i].uid == uid) return true;
    return false;
}

uint64_t InstanceRegistry::getUid (const ProcessorBase* p) const
{
    const juce::ScopedLock sl (lock);
    auto it = uids.find (p);
    return it == uids.end() ? 0 : it->second;
}

int InstanceRegistry::add (ProcessorBase* p)
{
    int n;
    uint64_t uid = 0;
    {
        const juce::ScopedLock sl (lock);
        if (instances.contains (p)) return p->getInstanceNumber();
        instances.add (p);
        n = nextNumber++;
    }
    {
        auto& r = shared::getSharedRegistry();
        SharedLock sl (r);
        if (r.count < shared::kMaxInstances)
        {
            uid = r.nextUid++;
            r.instances[r.count++] = { uid, p, &localApi };
        }
    }
    {
        const juce::ScopedLock sl (lock);
        uids[p] = uid;
    }
    if (uid != 0) notifyShared (1, uid);
    return n;
}

void InstanceRegistry::remove (ProcessorBase* p)
{
    uint64_t uid = 0;
    {
        const juce::ScopedLock sl (lock);
        if (! instances.contains (p)) return;
        instances.removeFirstMatchingValue (p);
        if (auto it = uids.find (p); it != uids.end()) { uid = it->second; uids.erase (it); }
        if (uid != 0) removedLocal[uid] = p;
    }
    // 1. nobody can find it any more, 2. the views it served elsewhere are gone, 3. every binary is told.
    if (uid != 0)
    {
        auto& r = shared::getSharedRegistry();
        SharedLock sl (r);
        for (int i = 0; i < r.count; ++i)
            if (r.instances[i].uid == uid)
            {
                for (int j = i; j < r.count - 1; ++j) r.instances[j] = r.instances[j + 1];
                --r.count;
                break;
            }
    }
    ViewServer::get().dropViewsOf (p);
    if (uid != 0) notifyShared (2, uid);
    const juce::ScopedLock sl (lock);
    removedLocal.erase (uid);
}

void InstanceRegistry::changed()
{
    uint64_t uid = 0;
    notifyShared (3, uid);
}

void InstanceRegistry::sharedListener (void* self, int event, uint64_t uid)
{
    static_cast<InstanceRegistry*> (self)->handleShared (event, uid);
}

void InstanceRegistry::handleShared (int event, uint64_t uid)
{
    if (event == 2)
    {
        {
            const juce::ScopedLock sl (lock);
            pendingRemovals.push_back (uid);
        }
        if (onMessageThread()) dispatchRemovals();
        else triggerAsyncUpdate();
        return;
    }
    triggerAsyncUpdate();
}

void InstanceRegistry::dispatchRemovals()
{
    std::vector<std::pair<uint64_t, ProcessorBase*>> todo;
    {
        const juce::ScopedLock sl (lock);
        for (auto uid : pendingRemovals)
        {
            auto it = removedLocal.find (uid);
            todo.push_back ({ uid, it != removedLocal.end() ? it->second : nullptr });
        }
        pendingRemovals.clear();
    }
    for (auto& item : todo)
        listeners.call ([&item] (Listener& l) { l.instanceRemoved (item.first, item.second); });
}

void InstanceRegistry::handleAsyncUpdate()
{
    dispatchRemovals();
    listeners.call ([] (Listener& l) { l.instancesChanged(); });
}

} // namespace st
