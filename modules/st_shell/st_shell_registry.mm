// The process-wide ST registry (macOS). Every ST plug-in binary finds the same instance through the
// Objective-C runtime, which is shared by everything in the process: the first binary registers a class
// with a fixed name and hangs the registry's address on it (keyed by a selector, which is also uniqued
// process-wide). Class registration is atomic, so two binaries loading at once still agree on one registry.
// The registry is never freed (it must outlive every binary that might use it).

#include "st_shell.h"

#import <Foundation/Foundation.h>
#include <objc/runtime.h>
#include <cstdlib>
#include <thread>

namespace st::shared
{

static Registry* createRegistry()
{
    auto* r = static_cast<Registry*> (std::calloc (1, sizeof (Registry)));
    r->magic = kMagic;
    r->version = kVersion;
    r->size = (uint32_t) sizeof (Registry);
    pthread_mutexattr_t attr;
    pthread_mutexattr_init (&attr);
    pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init (&r->lock, &attr);
    pthread_mutexattr_destroy (&attr);
    r->nextUid = 1;
    return r;
}

Registry& getSharedRegistry()
{
    static Registry* cached = nullptr;
    if (cached != nullptr) return *cached;

    static const char* className = "STShellSharedRegistryV1";
    const SEL key = sel_registerName ("stShellSharedRegistryV1");

    if (objc_getClass (className) == nil)
    {
        if (Class c = objc_allocateClassPair ([NSObject class], className, 0))
        {
            objc_registerClassPair (c);
            objc_setAssociatedObject (c, key, [NSValue valueWithPointer: createRegistry()], OBJC_ASSOCIATION_RETAIN);
        }
    }

    Class cls = objc_getClass (className);
    NSValue* value = nil;
    for (int tries = 0; cls != nil && tries < 2000 && value == nil; ++tries)   // another binary may still be publishing it
    {
        value = objc_getAssociatedObject (cls, key);
        if (value == nil) std::this_thread::yield();
    }

    auto* r = value != nil ? static_cast<Registry*> ([value pointerValue]) : nullptr;
    if (r == nullptr || r->magic != kMagic || r->version != kVersion || r->size != (uint32_t) sizeof (Registry))
    {
        // Not ours (or a different layout): use a private registry rather than corrupt a foreign one.
        static Registry* privateRegistry = createRegistry();
        r = privateRegistry;
    }
    cached = r;
    return *r;
}

} // namespace st::shared
