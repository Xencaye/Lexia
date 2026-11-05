#include "tray_menu.h"

#include "strings/string_ids.h"

// The rows of the tray icon's right-click menu, top to bottom. The fields after the text id are
// "checked" (the On / Off radio pair shows the current state) and "default" (Open Lexia is shown in
// bold and is what a double-click on the icon does).
std::vector<TrayMenuItem> TrayMenuItems(bool on) {
    using Kind = TrayMenuItem::Kind;
    std::vector<TrayMenuItem> items;
    items.push_back({Kind::Radio, TrayCommand::On, strings::id::kTrayOn, on, false});
    items.push_back({Kind::Radio, TrayCommand::Off, strings::id::kTrayOff, !on, false});
    items.push_back({Kind::Separator, TrayCommand::None, "", false, false});
    items.push_back({Kind::Normal, TrayCommand::Open, strings::id::kTrayOpen, false, true});
    items.push_back({Kind::Normal, TrayCommand::Quit, strings::id::kTrayQuit, false, false});
    return items;
}

// The text shown when the mouse rests on the tray icon: "Lexia is On" or "Lexia is Off".
const char* TrayTooltipId(bool on) {
    return on ? strings::id::kTrayTooltipOn : strings::id::kTrayTooltipOff;
}
