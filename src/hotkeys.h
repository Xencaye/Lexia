#pragma once
// Ids of the global hotkeys (the bindings are in settings.h). Win32-free.
// HK_ESC is only registered while the overlay is open (the overlay never takes focus,
// so it can't receive the Esc key itself).
#include <initializer_list>

enum HotkeyId { HK_READ = 1, HK_PAUSE = 2, HK_LANG = 4, HK_ESC = 5, HK_SPEECH = 6 };

// Whether the key `id` should be registered now (ADR-0001). `on`: Lexia is On; `speaking`:
// something is being spoken or paused. The Pause and Speech keys are only taken then, so a game
// keeps F7 / F6 while Lexia is silent. While Off no key is taken. There is no Quit key: the tray (D2) quits.
// HK_ESC is the Session's own and never wanted here.
inline bool KeyWanted(int id, bool on, bool speaking) {
    switch (id) {
    case HK_READ:
    case HK_LANG: return on;
    case HK_PAUSE:
    case HK_SPEECH: return on && speaking;
    default: return false;
    }
}
