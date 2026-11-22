// Non-Apple fallback for the shared ST registry and pins (on macOS st_shell_registry.mm is compiled instead):
// structures for this binary only.
#include "st_shell.h"

#include <cstdlib>

namespace st::shared
{

static void initRecursiveMutex (pthread_mutex_t& m)
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init (&attr);
    pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init (&m, &attr);
    pthread_mutexattr_destroy (&attr);
}

Registry& getSharedRegistry()
{
    static Registry* r = []
    {
        auto* reg = static_cast<Registry*> (std::calloc (1, sizeof (Registry)));
        reg->magic = kMagic;
        reg->version = kVersion;
        reg->size = (uint32_t) sizeof (Registry);
        initRecursiveMutex (reg->lock);
        reg->nextUid = 1;
        return reg;
    }();
    return *r;
}

Pins& getSharedPins()
{
    static Pins* p = []
    {
        auto* pins = static_cast<Pins*> (std::calloc (1, sizeof (Pins)));
        pins->magic = kPinsMagic;
        pins->version = kPinsVersion;
        pins->size = (uint32_t) sizeof (Pins);
        initRecursiveMutex (pins->lock);
        return pins;
    }();
    return *p;
}

} // namespace st::shared
