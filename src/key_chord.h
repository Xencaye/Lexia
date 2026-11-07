#pragma once
// The name of a key chord as settings.json writes it ("F8", "Ctrl+Alt+R"), from the Windows virtual-key code
// the Settings window's key capture hears (K.4). Pure and Win32-free: the few VK numbers it needs are
// constants here, so the rule lives in one tested place and the window only passes what Windows says.
//
// It is the C++ twin of keyName() in the old ui/js/settings.js, with the same names in the same order, and
// it writes the names ParseHotkey (settings.cpp) reads back.
#include <string>

// The name of the main key (no modifiers) of `vk`, or "" when Lexia cannot use that key. Usable keys:
// F1-F24, Space, Enter, Insert, Delete, Home, End, PageUp, PageDown, Up, Down, Left, Right, Pause,
// ScrollLock, PrintScreen, A-Z and 0-9 (the top row; the numeric keypad's digits are not named: they are
// other keys than the ones ParseHotkey would register).
std::string KeyName(int vk);

// Is `vk` a modifier key on its own (Shift, Ctrl, Alt, either side, or a Windows key)? The capture keeps
// waiting for the main key when one comes alone.
bool IsModifierKey(int vk);

// "Ctrl+Alt+Shift+Windows+<main key>", the modifiers in that order. "" when `vk` has no name (the capture
// keeps waiting).
std::string KeyChordName(int vk, bool ctrl, bool alt, bool shift, bool win);
