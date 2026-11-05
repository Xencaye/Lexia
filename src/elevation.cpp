// Run as admin (D8.3, ADR-0008): is this Lexia elevated, and start an elevated copy of it.
#include "elevation.h"

#include <windows.h>

#include <string>

#include <shellapi.h>

#include "log.h"

// True when the process token is elevated (Lexia runs with administrator rights after a UAC prompt).
// Any failure reads as "not elevated".
bool IsElevated() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION elevation{};
    DWORD size = 0;
    const bool ok = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size) != FALSE;
    CloseHandle(token);
    return ok && elevation.TokenIsElevated != 0;
}

// Starts lexia.exe again with the "runas" verb, which shows the UAC prompt. The call returns when the
// user has answered the prompt (SEE_MASK_NOASYNC); this process does not wait for the new one, but lets
// it take the foreground (its window must come to the front). The
// caller ends this Lexia when the result is Started (see ADR-0008 for the order of the steps).
RelaunchResult RelaunchElevated(const std::wstring& args) {
    wchar_t exe[MAX_PATH * 2];
    const DWORD n = GetModuleFileNameW(nullptr, exe, static_cast<DWORD>(std::size(exe)));
    if (n == 0 || n >= std::size(exe)) {
        LogError("Cannot relaunch as administrator: the exe path is unknown.");
        return RelaunchResult::Failed;
    }

    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    info.lpVerb = L"runas";
    info.lpFile = exe;
    info.lpParameters = args.empty() ? nullptr : args.c_str();
    info.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&info)) {
        const DWORD error = GetLastError();
        if (error == ERROR_CANCELLED) {  // the user chose No in the UAC prompt
            LogInfo("The administrator prompt was cancelled: Lexia carries on without it.");
            return RelaunchResult::Cancelled;
        }
        LogError("Cannot relaunch as administrator (Windows error " + std::to_string(error) + ").");
        return RelaunchResult::Failed;
    }
    if (info.hProcess) {
        // Windows lets only the foreground process give the foreground away. This Lexia is about to end,
        // so it passes the right on: else the elevated Lexia's Settings window opens behind other windows.
        AllowSetForegroundWindow(GetProcessId(info.hProcess));
        CloseHandle(info.hProcess);
    }
    return RelaunchResult::Started;
}
