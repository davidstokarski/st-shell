#pragma once

#include <cstdint>
#include <pthread.h>

/*  Process-wide registry shared by every ST plug-in binary (ST EQ, ST Sat...).

    Each plug-in is its own binary with its own copy of JUCE and of this shell, so instances of different
    plug-ins can't see each other through C++ statics. This is the meeting point: one plain-C structure per
    process, found by name (see getSharedRegistry), that every binary adds its instances to.

    Everything that crosses a binary boundary is plain C: integers, C strings filled into caller buffers, raw
    32-bit premultiplied ARGB pixels and function pointers into the binary that owns the instance. Never pass
    JUCE objects across. The layout is frozen: a different layout needs a new version / registry name.

    Rules
      - An instance's api functions live in its owner's binary: never call them after the instance has been
        removed (InstanceRegistry tells every binary synchronously, and the owner invalidates its views first).
      - Views are ids, not pointers; unknown ids are ignored, so a stale id is harmless.
      - Everything runs on the message thread except add / remove, which may happen on any thread. */

namespace st::shared
{

constexpr uint32_t kMagic = 0x53545231;   // 'STR1'
constexpr uint32_t kVersion = 1;
constexpr int kMaxInstances = 256, kMaxListeners = 32;

/** Mouse events forwarded to a remote view. */
enum MouseType : int { mouseMove = 0, mouseDown, mouseDrag, mouseUp, mouseExit, mouseDoubleClick, mouseWheel };
/** Modifier bits for MouseType events. */
enum ModifierBits : uint32_t { modShift = 1, modCtrl = 2, modAlt = 4, modCmd = 8, modLeft = 16, modRight = 32 };

/** Implemented by the binary that owns an instance. */
struct InstanceApi
{
    uint32_t size;
    /** Kind ("ST EQ"), host track name (may be empty), track colour (0 = none), instance number (per kind). */
    void (*getInfo) (void* ctx, char* kind, int kindSize, char* track, int trackSize, uint32_t* trackArgb, int* number);
    /** A compact module view of the instance for the multiview; 0 = the plug-in has none. */
    uint64_t (*createView) (void* ctx);
    void (*destroyView) (uint64_t view);
    /** Width the module wants at a given height. */
    int (*preferredWidth) (uint64_t view, int height);
    void (*setViewSize) (uint64_t view, int width, int height);
    /** Renders the view into premultiplied ARGB pixels (pixelW x pixelH = size * scale). 1 = rendered. */
    int (*render) (uint64_t view, uint32_t* pixels, int pixelW, int pixelH, int strideBytes, float scale);
    /** Forwards a mouse event (view coordinates). Returns 1 if consumed (a wheel the view didn't use returns 0). */
    int (*mouse) (uint64_t view, int type, float x, float y, uint32_t mods, float wheelX, float wheelY, int clicks);
};

struct Instance
{
    uint64_t uid;
    void* ctx;
    const InstanceApi* api;
};

/** event: 1 = added, 2 = removed, 3 = changed (track name / colour). May be called on any thread. */
using ListenerFn = void (*) (void* userData, int event, uint64_t uid);

struct Registry
{
    uint32_t magic, version, size;
    pthread_mutex_t lock;
    int count;
    Instance instances[kMaxInstances];
    int numListeners;
    ListenerFn listeners[kMaxListeners];
    void* listenerData[kMaxListeners];
    uint64_t nextUid;
};

/** The registry of this process (created by whichever ST binary asks first). */
Registry& getSharedRegistry();

} // namespace st::shared
