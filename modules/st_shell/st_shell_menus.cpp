// Non-Apple fallback for st::showMenu (on macOS st_shell_menus.mm is compiled instead).
#include "st_shell.h"

namespace st
{
void showMenu (juce::PopupMenu menu, juce::Component* target, std::function<void (int)> onResult, bool atMouse)
{
    auto opts = juce::PopupMenu::Options();
    if (target != nullptr && ! atMouse)
        opts = opts.withTargetComponent (target).withMinimumWidth (target->getWidth());
    menu.showMenuAsync (opts, [cb = std::move (onResult)] (int r) { if (cb) cb (r); });
}
}
