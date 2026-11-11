#pragma once
// One Lexia at a time. The first one keeps a named mutex; a second launch asks the first to open
// its Settings window (a registered window message posted to the first one's hidden window) and
// ends. Names are per Windows session (Local\), so another signed-in user can run their own.
#include <windows.h>

#include <string>

// The class name of App's hidden window (the second launch finds the first through it).
inline constexpr const wchar_t* kAppWindowClass = L"LexiaApp";

// The registered message that means "open the Settings window".
UINT OpenSettingsMessage();

// The registered message "Lexia.Quit": end Lexia like the tray's Quit does. Only the installer sends it
// (installer/lexia.iss uses the same name), so that Setup and the uninstaller can close a running Lexia
// cleanly. The App window lets it through UIPI, as it does OpenSettingsMessage (ADR-0008).
UINT QuitMessage();

// What the result of CreateMutexW means for "is another Lexia running?" (R36).
enum class MutexOutcome {
    First,           // we made the mutex: this is the first Lexia
    AlreadyRunning,  // it existed already (ERROR_ALREADY_EXISTS), or we may not open it (ERROR_ACCESS_DENIED)
    FailedRunAnyway  // it could not be made for another reason: a second copy is better than none
};

// Pure: `created` = CreateMutexW returned a handle, `lastError` = GetLastError() read straight after the call.
// ERROR_ACCESS_DENIED with no handle means the mutex exists but its owner's DACL keeps us out. That is how an
// elevated Lexia (Run as admin, ADR-0008) looks to a non-elevated launch with the default DACL of the
// elevated token, so it counts as "another Lexia runs", never as "first".
MutexOutcome ClassifyMutexCreate(bool created, DWORD lastError);

// The SDDL text of the security descriptor the mutex gets: full access for the current user and for SYSTEM
// only, and a Medium mandatory label (no write up). Empty if the user's SID cannot be read.
std::wstring SingleInstanceSecurityText();

// Makes (or opens) the named mutex with that security descriptor. `lastError` gets GetLastError() of the
// CreateMutexW call. The name is a parameter so that the tests can use their own and never meet a real Lexia.
HANDLE CreateSharedMutex(const wchar_t* name, DWORD* lastError);

class SingleInstance {
public:
    // Takes the mutex. With `waitMs` > 0 and another instance holding it, looks again every 100 ms for that
    // long (an `--elevated-relaunch` start, while the instance that started it ends; ADR-0008). If the wait
    // runs out, IsFirst() is false and the caller hands over to the other instance as usual.
    explicit SingleInstance(unsigned waitMs = 0);
    ~SingleInstance();
    SingleInstance(const SingleInstance&) = delete;
    SingleInstance& operator=(const SingleInstance&) = delete;

    bool IsFirst() const { return first_; }

    // Lets go of the mutex now (the destructor does too), e.g. before this instance starts an elevated
    // one that will take it. IsFirst() is false afterwards.
    void Release();
    // Takes the mutex again (as the constructor does), e.g. after the elevated relaunch did not happen.
    void Acquire(unsigned waitMs = 0);

    // For a second launch: lets the first Lexia open its Settings window. It waits a few seconds
    // for the first one's window, which does not exist yet while that one is starting.
    // False if there was no window to ask.
    static bool AskFirstToOpenSettings();

private:
    HANDLE mutex_ = nullptr;
    bool first_ = false;
};
