#pragma once
// What lexia.exe's command-line switches mean. Pure and Win32-free: wWinMain turns the wide
// command line into UTF-8 arguments (without the program name) and calls this.
#include <optional>
#include <string>
#include <vector>

// The switch names, compared without regard to case.
inline constexpr const char* kConsoleSwitch = "--console";
inline constexpr const char* kMinimisedSwitch = "--minimised";
inline constexpr const char* kElevatedRelaunchSwitch = "--elevated-relaunch";
// --set-run-as-admin on|off: the installer's way to write the Run as admin setting (task K.7). The value is
// the next argument, "on" or "off" in any letter case.
inline constexpr const char* kSetRunAsAdminSwitch = "--set-run-as-admin";

struct CommandLine {
    bool console = false;    // --console: open a console window (development; Ctrl+C ends Lexia there)
    bool minimised = false;  // --minimised: start with the tray only, no Settings window
    bool elevatedRelaunch = false;  // --elevated-relaunch: an earlier instance started this one with "runas" (D8.3, ADR-0008)
    // --set-run-as-admin on|off: save only that setting and end (no window, no elevation, no single-instance check).
    // Empty when the switch is not there. `setRunAsAdminInvalid` is true when the switch is there but its value is
    // missing or is not on / off; wWinMain then logs it and ends with 1.
    std::optional<bool> setRunAsAdmin;
    bool setRunAsAdminInvalid = false;
    std::vector<std::string> unknown;  // anything else, in order; logged, never fatal
};

// Switches are compared without regard to case.
CommandLine ParseCommandLine(const std::vector<std::string>& args);

// The arguments for the elevated relaunch (ADR-0008): --elevated-relaunch, and --minimised when
// the first instance was started minimised, so the relaunched one is too. UTF-8, switches
// separated by one space; ParseCommandLine reads them back.
std::string RelaunchArgs(bool minimised);
