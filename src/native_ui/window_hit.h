#pragma once
// Where a press on the Settings window moves the window (K10). The window has no Windows title bar: the
// whole paper is the "caption", except where a control is. Pure and Win32-free, in lexia_core; the window's
// WM_NCHITTEST asks it and maps the answer to HTCAPTION / HTCLIENT, the tests ask it with a built tree.
#include "native_ui/canvas.h"

namespace nui {

class Screen;

enum class WindowHit {
    Client,   // a control, a sheet or a list is here: the press belongs to the widget tree
    Caption,  // bare paper, a picture or plain text: a press and drag moves the window
};

// `p` is in window DIPs. Caption only when ALL of these hold:
//   - no sheet and no popup (the open list of a Select) is up: they are modal or close on a press outside, and a
//     press that Windows takes for a window move would never reach them;
//   - the press would not land on a control: the widget under `p` (Screen::WidgetAt) and its ancestors are all
//     passive ones (text, picture, progress bar, group, pane). A control (button, link, switch, slider, combo box,
//     list, key chip, edit) or a ScrollArea (its scrollbar thumb is dragged) makes it Client.
// Nothing under `p` (empty paper; a Panel is not a hit target) is Caption.
WindowHit HitTestWindow(const Screen& screen, PointF p);

}  // namespace nui
