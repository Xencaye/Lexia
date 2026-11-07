// Unit tests for instance.h: the registered window messages that the installer and a second launch post
// to a running Lexia. The names are also written in installer/lexia.iss, so they must not drift.
#include <doctest/doctest.h>

#include <string>

#include "instance.h"

TEST_CASE("Lexia.Quit is a registered message of its own") {
    const UINT quit = QuitMessage();
    CHECK(quit >= 0xC000);  // registered messages are numbered from 0xC000
    CHECK(quit == QuitMessage());  // the same number every time
    CHECK(quit != OpenSettingsMessage());
    // RegisterWindowMessage of the same name gives the same number: this is how Setup's message reaches Lexia.
    CHECK(quit == RegisterWindowMessageW(L"Lexia.Quit"));
    CHECK(OpenSettingsMessage() == RegisterWindowMessageW(L"Lexia.OpenSettings"));
}

TEST_CASE("The App window class is the one the installer looks for") {
    // installer/lexia.iss finds Lexia's hidden window by this class name.
    CHECK(std::wstring(kAppWindowClass) == L"LexiaApp");
}