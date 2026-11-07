#pragma once
// The pages lexia_settingsshot can draw (K.1c). A page builder fills a nui::Screen with widgets in the
// states the picture should show, with no window and no Win32 events: hover and keyboard focus are forced
// through the toolkit (Widget::PreviewHover, Screen::SetFocus, Screen::HandleMouse), so the same
// widget code runs here as in the window.
//
//   gallery                                            one of each toolkit control in its main states
//   home | voices | settings | report | help | about   the real SettingsUi (frame + page) with a sample UiState
//                                                      (settingsshot_sample.*), as the window shows it
// K.3 / K.4 add `--sheet <name>` and the key-capture states to BuildShotPage; the tool and the smoke test do
// not change for that.
#include <string>
#include <string_view>

#include "native_ui/screen.h"

namespace shot {

// What the command line asks of a real page.
struct ShotOptions {
    bool on = false;    // Lexia On (Home: the accent circle, "disable", the bee in its On tint)
    std::string voices;  // Voices page: "" | none | changed | user (settingsshot_sample.h)
    std::string sheet;   // Voices page: delete | deleteFailed | catalogue | offline | downloading | done | failed |
                         // failed-baddownload | added | refused | refused-notmedium | presetSaved | presetLoaded | presetFailed | presetLoadedOther | presetNotInstalled
    std::string hover;  // a widget to show hovered: power, enable, home|voices|settings|report|help|about (the
                        // links), volume, mail, discord, github, itch, notices
    std::string select;   // Settings: the Key whose card shows (read | pause | speech)
    std::string capture;  // Settings: the Key whose chip waits for a key (the card shows that Key)
    bool switches = false;  // Settings: Run at startup, Run as admin and Left-handed on (the reference's look)
};

// Builds the page `page` into `screen` (800 x 600 DIPs). `canvas` is only used to measure text while the
// widgets are placed. `dark` is the theme the picture will be drawn in (a page may need it for state).
// False if there is no such page.
bool BuildShotPage(std::string_view page, nui::Screen& screen, nui::Canvas& canvas, bool dark,
                   const ShotOptions& options = {});

// ---- forcing states (for the page builders)

// The hovered look on `w`, no pointer needed; any number of widgets at once.
inline void ForceHover(nui::Widget& w) { w.PreviewHover(true); }

// A real left-button press on the centre of `w` (it stays down: the pressed look). The pointer then
// rests there, so `w` is hovered too. One widget at a time.
void ForcePress(nui::Screen& screen, nui::Widget& w);

// Keyboard focus on `w`, with the focus ring and the grown slider thumb (the keyboard is "in use").
void ForceKeyboardFocus(nui::Screen& screen, nui::Widget& w);

}  // namespace shot
