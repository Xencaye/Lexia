// Run at startup (D6): the per-user Run value in the registry that starts Lexia at logon. The value
// text itself (the quoted exe path, and --minimised for Start minimised) is built by RunValueText in startup.*, which is
// pure and tested; this file only reads and writes the registry.
#include "autostart.h"

#include <windows.h>

#include <vector>

#include "command_line.h"
#include "common.h"
#include "log.h"
#include "startup.h"

namespace {

// HKEY_CURRENT_USER\<kRunKey>\<kRunValue>: no administrator rights are needed for it.
const wchar_t* const kRunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const wchar_t* const kRunValue = L"Lexia";

// The full path of lexia.exe, of any length. GetModuleFileNameW cuts the path off when the buffer is
// too small (and then returns the buffer size), so the buffer grows until the path fits.
std::wstring ExePath() {
    std::vector<wchar_t> buf(MAX_PATH);
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
        if (n == 0) return std::wstring();
        if (n < buf.size()) return std::wstring(buf.data(), n);
        buf.resize(buf.size() * 2);
    }
}

}  // namespace

// Run at startup is on when the value exists. Its text is not checked, so a value with an old path
// (Lexia was moved) still reads as on.
bool ReadRunAtStartup() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return false;
    const LSTATUS st = RegQueryValueExW(key, kRunValue, nullptr, nullptr, nullptr, nullptr);
    RegCloseKey(key);
    return st == ERROR_SUCCESS;
}

// On: writes the value (REG_SZ, its size in bytes includes the closing L'\0'). Off: deletes it.
// Returns false, and logs why, if the registry refuses.
bool WriteRunAtStartup(bool on, bool minimised) {
    HKEY key = nullptr;
    LSTATUS st = RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr);
    if (st != ERROR_SUCCESS) {
        LogWarn("[startup] The Run key could not be opened (error ", static_cast<long>(st), ").");
        return false;
    }
    if (on) {
        const std::wstring exe = ExePath();
        if (exe.empty()) {
            RegCloseKey(key);
            LogWarn("[startup] The path of lexia.exe is not known; Run at startup was not set.");
            return false;
        }
        const std::wstring value = Utf8ToWide(RunValueText(WideToUtf8(exe), minimised));
        st = RegSetValueExW(key, kRunValue, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
                            static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    } else {
        st = RegDeleteValueW(key, kRunValue);
        if (st == ERROR_FILE_NOT_FOUND) st = ERROR_SUCCESS;  // already off
    }
    RegCloseKey(key);
    if (st != ERROR_SUCCESS) {
        LogWarn("[startup] The Run value could not be ", on ? "set" : "removed", " (error ", static_cast<long>(st), ").");
        return false;
    }
    return true;
}

// Was Lexia started with --minimised (by the Run value at logon)? Then the Settings window stays
// closed at start. Uses the same parser as wWinMain, so both read the command line the same way.
bool StartedMinimised() { return ParseCommandLine(ProgramArguments()).minimised; }
