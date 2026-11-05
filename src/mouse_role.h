#pragma once
// Which mouse button does what on the frozen screenshot (the Overlay), and the left-handed
// choice. Pure and Win32-free.
//
// Normal: the left button Picks (click or drag to read), the right button Leaves (ends the
// Session). Left-handed swaps them. This is Lexia's own setting. Windows' "swap primary and
// secondary buttons" setting is applied by Windows before Lexia sees the messages (WM_LBUTTON*
// is then the physical right button), so it needs no code here; both swapped means normal again.

enum class MouseButton { Left, Right };  // as the messages say: WM_LBUTTON* / WM_RBUTTON*

enum class MouseRole {
    Pick,   // choose text to read
    Leave,  // end the Session
};

MouseRole RoleOf(MouseButton button, bool leftHanded);
