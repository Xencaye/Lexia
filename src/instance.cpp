#include "instance.h"

static constexpr const wchar_t* kMutexName = L"Local\\Lexia.SingleInstance";

// The message a second launch posts to the first Lexia: "open your Settings window". A registered
// message has the same number in every process that registers the same name.
UINT OpenSettingsMessage() {
    static const UINT msg = RegisterWindowMessageW(L"Lexia.OpenSettings");
    return msg;
}

// The message the installer posts to a running Lexia: "quit, the clean way" (App::Quit). The name is also
// written in installer/lexia.iss; change both together.
UINT QuitMessage() {
    static const UINT msg = RegisterWindowMessageW(L"Lexia.Quit");
    return msg;
}

SingleInstance::SingleInstance(unsigned waitMs) { Acquire(waitMs); }

SingleInstance::~SingleInstance() { Release(); }

// Takes the named mutex if no other Lexia holds it. With waitMs > 0 (the elevated relaunch,
// ADR-0008) it keeps trying for that long, while the old Lexia ends.
void SingleInstance::Acquire(unsigned waitMs) {
    Release();
    const ULONGLONG start = GetTickCount64();
    for (;;) {
        mutex_ = CreateMutexW(nullptr, FALSE, kMutexName);
        // If the mutex could not be made at all, run: a second copy is better than none.
        first_ = mutex_ == nullptr || GetLastError() != ERROR_ALREADY_EXISTS;
        if (first_ || GetTickCount64() - start >= waitMs) return;
        // The name lives while any process holds a handle to it: let go of ours, and look again
        // soon, until the other instance has ended.
        CloseHandle(mutex_);
        mutex_ = nullptr;
        Sleep(100);
    }
}

// Lets go of the mutex (the elevated relaunch does this before it starts the new Lexia).
void SingleInstance::Release() {
    if (mutex_) CloseHandle(mutex_);
    mutex_ = nullptr;
    first_ = false;
}

// A second launch: find the first Lexia's hidden App window (for up to 3 s, in case it is still
// starting) and ask it to open its Settings window. False if it was not found.
bool SingleInstance::AskFirstToOpenSettings() {
    for (int i = 0; i < 30; ++i) {
        HWND w = FindWindowW(kAppWindowClass, nullptr);
        if (w) {
            AllowSetForegroundWindow(ASFW_ANY);  // the first Lexia may bring its window to the front
            return PostMessageW(w, OpenSettingsMessage(), 0, 0) != 0;
        }
        Sleep(100);
    }
    return false;
}
