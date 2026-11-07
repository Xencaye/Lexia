// KeyChordName (K.4): the name of a captured key chord as settings.json writes it. Every key group, the
// modifier order, and the keys that have no name (the capture keeps waiting).
#include "key_chord.h"
#include "settings.h"

#include <doctest/doctest.h>

#include <string>
#include <vector>

namespace {
constexpr int kVkReturn = 0x0D, kVkShift = 0x10, kVkControl = 0x11, kVkMenu = 0x12, kVkPause = 0x13,
              kVkEscape = 0x1B, kVkSpace = 0x20, kVkPrior = 0x21, kVkNext = 0x22, kVkEnd = 0x23, kVkHome = 0x24,
              kVkLeft = 0x25, kVkUp = 0x26, kVkRight = 0x27, kVkDown = 0x28, kVkSnapshot = 0x2C,
              kVkInsert = 0x2D, kVkDelete = 0x2E, kVkLWin = 0x5B, kVkNumpad0 = 0x60, kVkF1 = 0x70,
              kVkF24 = 0x87, kVkScroll = 0x91;

std::string Plain(int vk) { return KeyChordName(vk, false, false, false, false); }
}  // namespace

TEST_CASE("KeyChordName: the function keys F1 to F24") {
    CHECK(Plain(kVkF1) == "F1");
    CHECK(Plain(kVkF1 + 7) == "F8");
    CHECK(Plain(kVkF1 + 11) == "F12");
    CHECK(Plain(kVkF24) == "F24");
    CHECK(Plain(kVkF24 + 1).empty());  // no F25
    CHECK(Plain(kVkF1 - 1).empty());   // the key before F1 (VK_NUMPAD... end) has no name
}

TEST_CASE("KeyChordName: letters and digits of the main block") {
    for (char c = 'A'; c <= 'Z'; ++c) CHECK(Plain(c) == std::string(1, c));
    for (char c = '0'; c <= '9'; ++c) CHECK(Plain(c) == std::string(1, c));
    // The numeric keypad is not the same key as the digit row: no name (ParseHotkey would register another key).
    CHECK(Plain(kVkNumpad0).empty());
    CHECK(Plain(kVkNumpad0 + 5).empty());
}

TEST_CASE("KeyChordName: the named keys") {
    CHECK(Plain(kVkSpace) == "Space");
    CHECK(Plain(kVkReturn) == "Enter");
    CHECK(Plain(kVkInsert) == "Insert");
    CHECK(Plain(kVkDelete) == "Delete");
    CHECK(Plain(kVkHome) == "Home");
    CHECK(Plain(kVkEnd) == "End");
    CHECK(Plain(kVkPrior) == "PageUp");
    CHECK(Plain(kVkNext) == "PageDown");
    CHECK(Plain(kVkUp) == "Up");
    CHECK(Plain(kVkDown) == "Down");
    CHECK(Plain(kVkLeft) == "Left");
    CHECK(Plain(kVkRight) == "Right");
    CHECK(Plain(kVkPause) == "Pause");
    CHECK(Plain(kVkScroll) == "ScrollLock");
    CHECK(Plain(kVkSnapshot) == "PrintScreen");
}

TEST_CASE("KeyChordName: keys that have no name keep the capture waiting") {
    CHECK(Plain(kVkEscape).empty());
    CHECK(Plain(0x09).empty());  // Tab
    CHECK(Plain(0x08).empty());  // Backspace
    CHECK(Plain(0xBA).empty());  // ';'
    CHECK(Plain(0).empty());
    CHECK(KeyChordName(0xBA, true, true, true, true).empty());  // modifiers do not give a nameless key a name
    // A modifier alone has no name either; IsModifierKey says it is one.
    for (int vk : {kVkShift, kVkControl, kVkMenu, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, kVkLWin, 0x5C}) {
        CHECK(Plain(vk).empty());
        CHECK(IsModifierKey(vk));
    }
    CHECK_FALSE(IsModifierKey(kVkF1));
    CHECK_FALSE(IsModifierKey('R'));
    CHECK_FALSE(IsModifierKey(kVkSpace));
}

TEST_CASE("KeyChordName: the modifiers come in the order Ctrl, Alt, Shift, Windows") {
    CHECK(KeyChordName('R', true, false, false, false) == "Ctrl+R");
    CHECK(KeyChordName('R', false, true, false, false) == "Alt+R");
    CHECK(KeyChordName('R', false, false, true, false) == "Shift+R");
    CHECK(KeyChordName('R', false, false, false, true) == "Windows+R");
    CHECK(KeyChordName('R', true, true, false, false) == "Ctrl+Alt+R");
    CHECK(KeyChordName('R', true, false, true, false) == "Ctrl+Shift+R");
    CHECK(KeyChordName('R', false, true, true, false) == "Alt+Shift+R");
    CHECK(KeyChordName('R', true, true, true, true) == "Ctrl+Alt+Shift+Windows+R");
    CHECK(KeyChordName(kVkF1 + 7, false, false, true, false) == "Shift+F8");
    CHECK(KeyChordName(kVkSnapshot, false, true, false, false) == "Alt+PrintScreen");
}

TEST_CASE("KeyChordName: every name it gives is a key ParseHotkey reads back") {
    // The capture's answer goes to settings.json and back through ParseHotkey: they must agree.
    std::vector<int> vks = {kVkSpace, kVkReturn, kVkInsert, kVkDelete, kVkHome, kVkEnd, kVkPrior, kVkNext,
                            kVkUp, kVkDown, kVkLeft, kVkRight, kVkPause, kVkScroll, kVkSnapshot, 'A', 'Z', '0', '9'};
    for (int f = kVkF1; f <= kVkF24; ++f) vks.push_back(f);
    for (int vk : vks) {
        const std::string name = KeyChordName(vk, true, true, true, true);
        CHECK_MESSAGE(!name.empty(), "a name for VK ", vk);
        Hotkey hk;
        CHECK_MESSAGE(ParseHotkey(name, hk), name);
        CHECK(static_cast<int>(hk.vk) == vk);  // and it is the same key
    }
}
