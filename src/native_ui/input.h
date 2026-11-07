#pragma once
// Input events of the native Settings window (K1.b, ADR-0009). The window turns its Win32 messages into
// these (the K1 window task); the widget tree and the tests only see these. Pure, Win32-free, no
// virtual-key numbers: a key the toolkit does not name is Key::Other with the platform's code.
#include "native_ui/canvas.h"

namespace nui {

enum class MouseButton { None, Left, Right };

enum class MouseKind {
    Down,   // a button went down (button says which)
    Up,     // a button went up
    Move,   // the pointer moved (also during a drag)
    Leave,  // the pointer left the window
    Wheel,  // the wheel turned: wheelDelta
};

struct MouseEvent {
    MouseKind kind = MouseKind::Move;
    MouseButton button = MouseButton::None;
    PointF pos;               // DIPs, window client area
    float wheelDelta = 0;     // DIPs to scroll; positive = the view moves down (content moves up)
};

// What one notch of the wheel scrolls, in DIPs (the window multiplies a WM_MOUSEWHEEL notch by this).
constexpr float kWheelNotchDips = 48.0f;

enum class Key {
    Tab, Enter, Space, Escape,
    Left, Right, Up, Down,
    PageUp, PageDown, Home, End,
    Other,  // any other key: KeyEvent::code holds the platform code (a Windows virtual-key code)
};

// A key going down (the toolkit has no key-up events: Space and Enter act on the press).
struct KeyEvent {
    Key key = Key::Other;
    int code = 0;           // platform code, always set by the window; the toolkit only reads it for Other
    bool ctrl = false;
    bool alt = false;
    bool shift = false;
    bool win = false;
    bool repeat = false;    // an auto-repeat of a key that is already down

    bool NoModifiers() const { return !ctrl && !alt && !shift && !win; }
};

// What the pointer should look like over the widget under it.
enum class CursorHint { Arrow, Hand };

}  // namespace nui
