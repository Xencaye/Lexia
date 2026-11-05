#pragma once
// One Lexia at a time. The first one keeps a named mutex; a second launch asks the first to open
// its Settings window (a registered window message posted to the first one's hidden window) and
// ends. Names are per Windows session (Local\), so another signed-in user can run their own.
#include <windows.h>

// The class name of App's hidden window (the second launch finds the first through it).
inline constexpr const wchar_t* kAppWindowClass = L"LexiaApp";

// The registered message that means "open the Settings window".
UINT OpenSettingsMessage();

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
