#include "mouse_role.h"

// The primary button Picks and the other one Leaves the Overlay. Lexia's left-handed setting swaps
// them (`!=` acts as an exclusive or: Left and not left-handed, or Right and left-handed).
MouseRole RoleOf(MouseButton button, bool leftHanded) {
    const bool primary = (button == MouseButton::Left) != leftHanded;
    return primary ? MouseRole::Pick : MouseRole::Leave;
}
