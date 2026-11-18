#pragma once

namespace st
{

/** Shows a menu using the platform's own menus (NSMenu on macOS, dark appearance) and falls back
    to JUCE's PopupMenu elsewhere.

    - `target`: the component the menu drops down from (nullptr = at the mouse, for context menus).
    - `onResult`: called with the chosen item ID (0 when dismissed). Item `action`s are invoked too.
    - `atMouse`: open at the mouse position even when a target is given (right-click menus).
*/
void showMenu (juce::PopupMenu menu, juce::Component* target, std::function<void (int)> onResult = {}, bool atMouse = false);

} // namespace st
