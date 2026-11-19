// Native macOS menus for st::showMenu.
#define JUCE_CORE_INCLUDE_OBJC_HELPERS 1
#define JUCE_CORE_INCLUDE_NATIVE_HEADERS 1
#include "st_shell.h"

namespace st
{

namespace
{
    struct MenuTarget final : juce::ObjCClass<NSObject>
    {
        MenuTarget() : ObjCClass ("STMenuTarget_")
        {
            addIvar<int*> ("result");
            addMethod (@selector (itemChosen:), [] (id self, SEL, NSMenuItem* item)
            {
                if (auto* r = juce::getIvar<int*> (self, "result"))
                    *r = (int) [item tag];
            });
            registerClass();
        }

        static MenuTarget& get() { static MenuTarget c; return c; }
    };

    NSString* toNS (const juce::String& s) { return [NSString stringWithUTF8String: s.toRawUTF8()]; }

    /** Renders an item's Drawable into a 16 pt template image (macOS tints it for the menu's appearance). */
    NSImage* toNSImage (const juce::Drawable& d)
    {
        constexpr int pts = 16, scale = 2;
        juce::Image img (juce::Image::ARGB, pts * scale, pts * scale, true);
        {
            juce::Graphics g (img);
            d.drawWithin (g, juce::Rectangle<float> (0.0f, 0.0f, (float) (pts * scale), (float) (pts * scale)),
                          juce::RectanglePlacement::centred, 1.0f);
        }
        juce::MemoryOutputStream png;
        juce::PNGImageFormat().writeImageToStream (img, png);
        NSData* data = [NSData dataWithBytes: png.getData() length: png.getDataSize()];
        NSImage* image = [[[NSImage alloc] initWithData: data] autorelease];
        [image setSize: NSMakeSize (pts, pts)];
        [image setTemplate: YES];
        return image;
    }

    NSMenu* build (const juce::PopupMenu& menu, id target, std::vector<const juce::PopupMenu::Item*>& leaves)
    {
        NSMenu* ns = [[[NSMenu alloc] initWithTitle: @""] autorelease];
        [ns setAutoenablesItems: NO];

        juce::PopupMenu::MenuItemIterator it (menu);
        while (it.next())
        {
            const auto& item = it.getItem();

            if (item.isSeparator)
            {
                [ns addItem: [NSMenuItem separatorItem]];
                continue;
            }

            if (item.isSectionHeader)
            {
                if (@available (macOS 14.0, *))
                    [ns addItem: [NSMenuItem sectionHeaderWithTitle: toNS (item.text)]];
                else
                {
                    if ([ns numberOfItems] > 0) [ns addItem: [NSMenuItem separatorItem]];
                    NSMenuItem* h = [[[NSMenuItem alloc] initWithTitle: toNS (item.text) action: nil keyEquivalent: @""] autorelease];
                    [h setEnabled: NO];
                    [ns addItem: h];
                }
                continue;
            }

            NSMenuItem* ni = [[[NSMenuItem alloc] initWithTitle: toNS (item.text) action: nil keyEquivalent: @""] autorelease];
            [ni setEnabled: item.isEnabled ? YES : NO];
            [ni setState: item.isTicked ? NSControlStateValueOn : NSControlStateValueOff];
            if (item.image != nullptr)
                [ni setImage: toNSImage (*item.image)];

            if (item.subMenu != nullptr)
            {
                [ni setSubmenu: build (*item.subMenu, target, leaves)];
            }
            else
            {
                leaves.push_back (&item);
                [ni setTag: (NSInteger) leaves.size()];   // 1-based index into leaves
                [ni setTarget: target];
                [ni setAction: @selector (itemChosen:)];
            }

            [ns addItem: ni];
        }
        return ns;
    }
}

static void showNow (juce::PopupMenu menu, juce::Component* target, juce::Component* underMouse, std::function<void (int)> onResult,
                     bool atMouse, juce::Point<float> mouseAtRequest)
{
    // Context menus pass no target: anchor them to the window under the mouse so they are native too.
    auto* anchor = target != nullptr ? target : underMouse;
    auto* peerComp = anchor != nullptr ? anchor->getTopLevelComponent() : nullptr;
    auto* peer = peerComp != nullptr ? peerComp->getPeer() : nullptr;

    if (peer == nullptr)
    {
        // Not on screen (or no target): fall back to JUCE's menu.
        menu.showMenuAsync (juce::PopupMenu::Options(), [cb = std::move (onResult)] (int r) { if (cb) cb (r); });
        return;
    }

    auto* view = (NSView*) peer->getNativeHandle();

    juce::Point<float> screenPos;
    if (atMouse || target == nullptr)
        screenPos = mouseAtRequest;
    else
    {
        const auto area = target->getScreenBounds().toFloat();
        screenPos = { area.getX(), area.getBottom() + 2.0f };
    }

    const auto local = peer->globalToLocal (screenPos);

    std::vector<const juce::PopupMenu::Item*> leaves;
    int result = 0;

    id menuTarget = [MenuTarget::get().createInstance() init];
    object_setInstanceVariable (menuTarget, "result", &result);

    NSMenu* ns = build (menu, menuTarget, leaves);
    [ns setAppearance: [NSAppearance appearanceNamed: NSAppearanceNameDarkAqua]];
    if (target != nullptr && ! atMouse)
        [ns setMinimumWidth: (CGFloat) target->getScreenBounds().getWidth()];

    // Synchronous tracking (standard for NSMenu); dispatch the result afterwards.
    const NSPoint p = NSMakePoint ((CGFloat) local.x, [view isFlipped] ? (CGFloat) local.y : [view bounds].size.height - (CGFloat) local.y);
    [ns popUpMenuPositioningItem: nil atLocation: p inView: view];
    [menuTarget release];

    std::function<void()> action;
    int itemID = 0;
    if (result > 0 && result <= (int) leaves.size())
    {
        action = leaves[(size_t) result - 1]->action;
        itemID = leaves[(size_t) result - 1]->itemID;
    }

    juce::MessageManager::callAsync ([action, itemID, cb = std::move (onResult)]
    {
        if (action) action();
        if (cb) cb (itemID);
    });
}

void showMenu (juce::PopupMenu menu, juce::Component* target, std::function<void (int)> onResult, bool atMouse)
{
    // Defer: NSMenu tracking is modal, so never start it from inside a JUCE mouse handler.
    const auto mouse = juce::Desktop::getMousePosition().toFloat();
    juce::Component::SafePointer<juce::Component> safe (target);
    juce::Component::SafePointer<juce::Component> under (juce::Desktop::getInstance().getMainMouseSource().getComponentUnderMouse());
    const bool hadTarget = target != nullptr;

    juce::MessageManager::callAsync ([pending = std::move (menu), safe, under, hadTarget, cb = std::move (onResult), atMouse, mouse]() mutable
    {
        if (hadTarget && safe == nullptr) return;
        showNow (std::move (pending), safe.getComponent(), under.getComponent(), std::move (cb), atMouse, mouse);
    });
}

} // namespace st
