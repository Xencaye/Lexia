#include "startup.h"

#include "command_line.h"

// The text of the Run value: "<exe path>", then --minimised if asked for. The quotes keep a path with
// spaces (such as C:\Program Files\Lexia\lexia.exe) as one program name; --minimised keeps a start at
// logon in the tray.
std::string RunValueText(const std::string& exePath, bool minimised) {
    std::string value = "\"" + exePath + "\"";
    if (minimised) value += std::string(" ") + kMinimisedSwitch;
    return value;
}
