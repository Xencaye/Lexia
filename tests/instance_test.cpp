// Unit tests for instance.h: the registered window messages that the installer and a second launch post
// to a running Lexia. The names are also written in installer/lexia.iss, so they must not drift.
#include <doctest/doctest.h>

#include <string>

#include "instance.h"  // includes <windows.h>, which <sddl.h> needs first

#include <sddl.h>

TEST_CASE("Lexia.Quit is a registered message of its own") {
    const UINT quit = QuitMessage();
    CHECK(quit >= 0xC000);  // registered messages are numbered from 0xC000
    CHECK(quit == QuitMessage());  // the same number every time
    CHECK(quit != OpenSettingsMessage());
    // RegisterWindowMessage of the same name gives the same number: this is how Setup's message reaches Lexia.
    CHECK(quit == RegisterWindowMessageW(L"Lexia.Quit"));
    CHECK(OpenSettingsMessage() == RegisterWindowMessageW(L"Lexia.OpenSettings"));
}

TEST_CASE("ClassifyMutexCreate: access denied means another Lexia runs (R36)") {
    // A new mutex: first. (CreateMutexW sets ERROR_SUCCESS or leaves an old error; neither is ALREADY_EXISTS.)
    CHECK(ClassifyMutexCreate(true, ERROR_SUCCESS) == MutexOutcome::First);
    // An existing mutex we could open: not first.
    CHECK(ClassifyMutexCreate(true, ERROR_ALREADY_EXISTS) == MutexOutcome::AlreadyRunning);
    // No handle and access denied: the mutex exists (an elevated Lexia's): not first. This was the bug.
    CHECK(ClassifyMutexCreate(false, ERROR_ACCESS_DENIED) == MutexOutcome::AlreadyRunning);
    // No handle for any other reason: run anyway.
    CHECK(ClassifyMutexCreate(false, ERROR_NOT_ENOUGH_MEMORY) == MutexOutcome::FailedRunAnyway);
    CHECK(ClassifyMutexCreate(false, ERROR_INVALID_NAME) == MutexOutcome::FailedRunAnyway);
}

TEST_CASE("The mutex security descriptor lists the current user and SYSTEM, with a Medium label") {
    const std::wstring sddl = SingleInstanceSecurityText();
    REQUIRE(!sddl.empty());
    CHECK(sddl.find(L"D:(A;;GA;;;S-1-") == 0);  // the first ACE is the user's SID
    CHECK(sddl.find(L"(A;;GA;;;SY)") != std::wstring::npos);
    CHECK(sddl.find(L"S:(ML;;NW;;;ME)") != std::wstring::npos);
    // The text must parse as a real descriptor.
    PSECURITY_DESCRIPTOR sd = nullptr;
    CHECK(ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &sd, nullptr));
    if (sd) LocalFree(sd);
}

TEST_CASE("CreateSharedMutex: the second call sees the first, in this process") {
    // A name of the test's own, so that a real Lexia on the machine never takes part.
    const wchar_t* name = L"Local\\Lexia.SingleInstance.UnitTest";
    DWORD e1 = 0, e2 = 0;
    HANDLE a = CreateSharedMutex(name, &e1);
    REQUIRE(a != nullptr);
    CHECK(ClassifyMutexCreate(true, e1) == MutexOutcome::First);
    HANDLE b = CreateSharedMutex(name, &e2);
    REQUIRE(b != nullptr);  // the same user can open it: the DACL grants the user full access
    CHECK(ClassifyMutexCreate(true, e2) == MutexOutcome::AlreadyRunning);
    CloseHandle(b);
    CloseHandle(a);
}

TEST_CASE("The App window class is the one the installer looks for") {
    // installer/lexia.iss finds Lexia's hidden window by this class name.
    CHECK(std::wstring(kAppWindowClass) == L"LexiaApp");
}