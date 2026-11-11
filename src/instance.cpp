#include "instance.h"

#include <sddl.h>

#include <vector>

#include "log.h"

static constexpr const wchar_t* kMutexName = L"Local\\Lexia.SingleInstance";

MutexOutcome ClassifyMutexCreate(bool created, DWORD lastError) {
    if (created) return lastError == ERROR_ALREADY_EXISTS ? MutexOutcome::AlreadyRunning : MutexOutcome::First;
    // No handle. Access denied = the mutex exists and we are not let in (see the header). Any other failure:
    // run anyway.
    return lastError == ERROR_ACCESS_DENIED ? MutexOutcome::AlreadyRunning : MutexOutcome::FailedRunAnyway;
}

// The current user's SID as text ("S-1-5-21-..."), or empty. The token's user, not its owner: for an elevated
// administrator the owner is the Administrators group, but the user is still the same person's SID in the
// elevated and in the non-elevated token, which is what lets both open the mutex.
static std::wstring CurrentUserSidText() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return L"";
    std::wstring text;
    DWORD size = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &size);  // asks for the size
    if (size > 0) {
        std::vector<BYTE> buf(size);
        if (GetTokenInformation(token, TokenUser, buf.data(), size, &size)) {
            LPWSTR sid = nullptr;
            if (ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(buf.data())->User.Sid, &sid)) {
                text = sid;
                LocalFree(sid);
            }
        }
    }
    CloseHandle(token);
    return text;
}

// The DACL has two entries and nothing else: the current user and SYSTEM, full access. The user entry is what
// matters (R36): the default DACL of an elevated process lists only SYSTEM and Administrators, and the
// Administrators group is deny-only in the same person's non-elevated token, so a normal launch (or Setup run
// as that person) could not open an elevated Lexia's mutex. Nobody else is listed: another user, a guest or a
// service account gets access denied. The SACL holds a Medium mandatory label (ME) with the "no write up"
// policy (NW): without it a mutex made by a High process can be labelled High, and a Medium process is then
// kept out; with it Medium opens it, and a Low process (a sandboxed program) can read the name but not take
// the mutex or change it. Full access for the user is not a loss: a process of the same user and level can
// already end Lexia.
std::wstring SingleInstanceSecurityText() {
    const std::wstring sid = CurrentUserSidText();
    if (sid.empty()) return L"";
    return L"D:(A;;GA;;;" + sid + L")(A;;GA;;;SY)S:(ML;;NW;;;ME)";
}

HANDLE CreateSharedMutex(const wchar_t* name, DWORD* lastError) {
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = FALSE;
    PSECURITY_DESCRIPTOR sd = nullptr;
    const std::wstring sddl = SingleInstanceSecurityText();
    if (!sddl.empty() &&
        ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &sd, nullptr)) {
        sa.lpSecurityDescriptor = sd;
    } else {
        // Fall back to the default descriptor: a normal start still works; only a launch from the other
        // integrity level might not see an elevated Lexia, which ClassifyMutexCreate still handles.
        static bool logged = false;
        if (!logged) {
            logged = true;
            LogWarn("The single-instance mutex gets the default security descriptor (error ", GetLastError(), ").");
        }
        sd = nullptr;
    }
    HANDLE h = CreateMutexW(sd ? &sa : nullptr, FALSE, name);
    if (lastError) *lastError = GetLastError();  // read before LocalFree can change it
    if (sd) LocalFree(sd);
    if (lastError) SetLastError(*lastError);
    return h;
}

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
    bool loggedDenied = false;
    for (;;) {
        DWORD err = 0;
        mutex_ = CreateSharedMutex(kMutexName, &err);
        const MutexOutcome outcome = ClassifyMutexCreate(mutex_ != nullptr, err);
        if (outcome == MutexOutcome::FailedRunAnyway) {
            // The mutex could not be made at all: run, a second copy is better than none (mutex_ is null).
            LogWarn("The single-instance mutex could not be made (error ", err, "); Lexia starts anyway.");
        } else if (outcome == MutexOutcome::AlreadyRunning && !mutex_ && !loggedDenied) {
            // An elevated Lexia (or a Lexia of another level) owns it and the DACL keeps us out: R36.
            loggedDenied = true;
            LogInfo("The single-instance mutex exists and may not be opened; another Lexia is running.");
        }
        first_ = outcome != MutexOutcome::AlreadyRunning;
        if (first_ || GetTickCount64() - start >= waitMs) {
            // Not the first: a handle we did get (ALREADY_EXISTS) is not needed; the other Lexia holds the name.
            if (!first_ && mutex_) {
                CloseHandle(mutex_);
                mutex_ = nullptr;
            }
            return;
        }
        // The name lives while any process holds a handle to it: let go of ours, and look again
        // soon, until the other instance has ended. (mutex_ is null when access was denied.)
        if (mutex_) CloseHandle(mutex_);
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
