#pragma once
// Start-up of Lexia (D6): the Run key's value. Pure and Win32-free; the registry itself is
// autostart.*. The command-line switches are command_line.h.
#include <string>

// The value of HKCU\...\Run\Lexia when Run at startup is on: "<exe>", and --minimised when the user
// wants a start at logon to stay in the tray (Start minimised). A manual start (no switch) always
// opens the Settings window.
std::string RunValueText(const std::string& exePath, bool minimised);
