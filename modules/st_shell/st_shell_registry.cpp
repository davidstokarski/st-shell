// Non-Apple fallback for the shared ST registry (on macOS st_shell_registry.mm is compiled instead):
// a registry for this binary only.
#include "st_shell.h"

#include <cstdlib>

namespace st::shared
{

Registry& getSharedRegistry()
{
    static Registry* r = []
    {
        auto* reg = static_cast<Registry*> (std::calloc (1, sizeof (Registry)));
        reg->magic = kMagic;
        reg->version = kVersion;
        reg->size = (uint32_t) sizeof (Registry);
        pthread_mutexattr_t attr;
        pthread_mutexattr_init (&attr);
        pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init (&reg->lock, &attr);
        pthread_mutexattr_destroy (&attr);
        reg->nextUid = 1;
        return reg;
    }();
    return *r;
}

} // namespace st::shared
