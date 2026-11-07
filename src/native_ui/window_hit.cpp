#include "native_ui/window_hit.h"

#include "native_ui/scroll_area.h"
#include "native_ui/screen.h"

namespace nui {

namespace {

// Is a widget with this role only to be looked at? Everything else takes a press itself.
bool IsPassive(Role role) {
    switch (role) {
        case Role::None:
        case Role::Text:
        case Role::Image:
        case Role::Group:
        case Role::Pane:
        case Role::ProgressBar:
            return true;
        default:
            return false;
    }
}

}  // namespace

WindowHit HitTestWindow(const Screen& screen, PointF p) {
    // A sheet (modal) or an open list (closed by a press outside it) must get every press, also the one on
    // the scrim or on paper: a window move would swallow it.
    if (screen.SheetCount() > 0 || screen.Popup()) return WindowHit::Client;

    for (const Widget* w = screen.WidgetAt(p); w; w = w->Parent()) {
        // A ScrollArea has no role of its own, but its scrollbar thumb is pressed and dragged.
        if (dynamic_cast<const ScrollArea*>(w)) return WindowHit::Client;
        if (!IsPassive(w->Access().role)) return WindowHit::Client;
    }
    return WindowHit::Caption;
}

}  // namespace nui
