#include "command_line.h"

#include "common.h"

// lexia.exe's command line: the switches Lexia knows, in any order and any letter case. Any other
// argument is kept in `unknown`, so that the caller can log it; it does not stop Lexia.
CommandLine ParseCommandLine(const std::vector<std::string>& args) {
    CommandLine c;
    for (const std::string& arg : args) {
        const std::string a = ToLowerAscii(arg);  // "--Console" works too
        if (a == kConsoleSwitch) {
            c.console = true;
        } else if (a == kElevatedRelaunchSwitch) {
            c.elevatedRelaunch = true;
        } else if (a == kMinimisedSwitch) {
            c.minimised = true;
        } else {
            c.unknown.push_back(arg);
        }
    }
    return c;
}

// The arguments for the elevated copy of Lexia (ADR-0008). --elevated-relaunch tells it to wait for
// this instance to let go of the single-instance mutex; --minimised keeps it in the tray when this
// one was started that way.
std::string RelaunchArgs(bool minimised) {
    std::string args = kElevatedRelaunchSwitch;
    if (minimised) args += std::string(" ") + kMinimisedSwitch;
    return args;
}
