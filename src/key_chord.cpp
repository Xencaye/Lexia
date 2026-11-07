#include "key_chord.h"

namespace {

// Windows virtual-key codes (winuser.h), written out so this file needs no <windows.h>.
constexpr int kVkReturn = 0x0D, kVkShift = 0x10, kVkControl = 0x11, kVkMenu = 0x12, kVkPause = 0x13,
              kVkSpace = 0x20, kVkPrior = 0x21, kVkNext = 0x22, kVkEnd = 0x23, kVkHome = 0x24, kVkLeft = 0x25,
              kVkUp = 0x26, kVkRight = 0x27, kVkDown = 0x28, kVkSnapshot = 0x2C, kVkInsert = 0x2D,
              kVkDelete = 0x2E, kVkLWin = 0x5B, kVkRWin = 0x5C, kVkF1 = 0x70, kVkF24 = 0x87, kVkScroll = 0x91;
// 0xA0..0xA5 are the left and right Shift, Control and Menu (Alt) keys.
constexpr int kVkLShift = 0xA0, kVkRMenu = 0xA5;

}  // namespace

std::string KeyName(int vk) {
    if (vk >= kVkF1 && vk <= kVkF24) return "F" + std::to_string(vk - kVkF1 + 1);
    // The digits and letters of the main block have the VK of their ASCII digit / capital letter.
    if ((vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z')) return std::string(1, static_cast<char>(vk));
    switch (vk) {
        case kVkSpace: return "Space";
        case kVkReturn: return "Enter";
        case kVkInsert: return "Insert";
        case kVkDelete: return "Delete";
        case kVkHome: return "Home";
        case kVkEnd: return "End";
        case kVkPrior: return "PageUp";
        case kVkNext: return "PageDown";
        case kVkUp: return "Up";
        case kVkDown: return "Down";
        case kVkLeft: return "Left";
        case kVkRight: return "Right";
        case kVkPause: return "Pause";
        case kVkScroll: return "ScrollLock";
        case kVkSnapshot: return "PrintScreen";
        default: return std::string();
    }
}

bool IsModifierKey(int vk) {
    return vk == kVkShift || vk == kVkControl || vk == kVkMenu || (vk >= kVkLShift && vk <= kVkRMenu) ||
           vk == kVkLWin || vk == kVkRWin;
}

std::string KeyChordName(int vk, bool ctrl, bool alt, bool shift, bool win) {
    const std::string main = KeyName(vk);
    if (main.empty()) return std::string();  // not a key Lexia can use: the capture keeps waiting
    std::string name;
    if (ctrl) name += "Ctrl+";
    if (alt) name += "Alt+";
    if (shift) name += "Shift+";
    if (win) name += "Windows+";
    return name + main;
}
