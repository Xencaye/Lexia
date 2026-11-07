#pragma once
// The pages lexia_settingsshot can draw (K.1c). A page builder fills a nui::Screen with widgets in the
// states the picture should show, with no window and no Win32 events: hover and keyboard focus are forced
// through the toolkit (Widget::PreviewHover, Screen::SetFocus, Screen::HandleMouse), so the same
// widget code runs here as in the window.
//
// Today only `gallery` exists: one of each toolkit control in its main states, for comparing the look
// with wireframe/html-reference/. The next task adds `home|voices|settings|report|help|about` (and
// `--sheet <name>`) to BuildShotPage, built from a sample UiState; the tool and the smoke test do not
// change for that.
#include <string_view>

#include "native_ui/screen.h"

namespace shot {

// Builds the page `page` into `screen` (800 x 600 DIPs). `canvas` is only used to measure text while the
// widgets are placed. `dark` is the theme the picture will be drawn in (a page may need it for state).
// False if there is no such page.
bool BuildShotPage(std::string_view page, nui::Screen& screen, nui::Canvas& canvas, bool dark);

// ---- forcing states (for the page builders)

// The hovered look on `w`, no pointer needed; any number of widgets at once.
inline void ForceHover(nui::Widget& w) { w.PreviewHover(true); }

// A real left-button press on the centre of `w` (it stays down: the pressed look). The pointer then
// rests there, so `w` is hovered too. One widget at a time.
void ForcePress(nui::Screen& screen, nui::Widget& w);

// Keyboard focus on `w`, with the focus ring and the grown slider thumb (the keyboard is "in use").
void ForceKeyboardFocus(nui::Screen& screen, nui::Widget& w);

}  // namespace shot
