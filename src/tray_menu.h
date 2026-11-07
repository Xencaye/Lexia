#pragma once
// The rows of the tray icon's menu, as data (spec: old ui/dev/tray.html). Pure and Win32-free: the
// tray code in lexia.exe turns these rows into a native popup menu and the chosen command into
// an App action. The words are string ids; the caller looks them up in the Interface language.
#include <vector>

// The value of a command is its menu item id in the native menu; None (0) is "nothing chosen".
enum class TrayCommand : unsigned { None = 0, On = 1, Off = 2, Open = 3, Quit = 4 };

struct TrayMenuItem {
    enum class Kind { Normal, Radio, Separator };
    Kind kind = Kind::Normal;
    TrayCommand command = TrayCommand::None;  // None for a separator
    const char* textId = "";                  // a strings::id constant; "" for a separator
    bool checked = false;                     // the radio item that is current
    bool isDefault = false;                   // drawn bold; also what a double-click does
};

// The rows while Lexia is On (`on`) or Off.
std::vector<TrayMenuItem> TrayMenuItems(bool on);

// The string id of the icon's tooltip.
const char* TrayTooltipId(bool on);
