#pragma once
// Small types and helpers shared by several modules. Win32-free on purpose: layout.* and the
// tests include this header, so anything that needs <windows.h> is only declared here and
// defined in common.cpp (which also holds the std-only ReadFileToString).
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

// Plain geometry for the Lines and Blocks, so layout.* needs no Win32 types. Same shape as
// RECT / POINT; win32_geom.h converts at the Win32 boundary.
struct Rect {
    int left = 0, top = 0, right = 0, bottom = 0;  // right/bottom exclusive, like RECT
};

struct Point {
    int x = 0, y = 0;
};

struct TextLine {
    Rect box;
    std::string text;
    float conf = 0.f;  // 0..100
};

struct TextBlock {  // a paragraph, a heading, a single name... whatever belongs together
    Rect box;
    std::string text;
    std::vector<TextLine> lines;  // the Lines it was built from, in reading order; only `text` re-joins hyphens
};

// `s` without the spaces, tabs and line ends at its start and end ("" if it holds nothing else).
inline std::string Trim(const std::string& s) {
    const size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    const size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// 'A'..'Z' to 'a'..'z'; every other byte (UTF-8 included) stays as it is.
inline std::string ToLowerAscii(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

inline bool HasLetterOrDigit(const std::string& s) {
    for (unsigned char c : s) {
        if (c >= 0x80 || std::isalnum(c)) return true;  // >= 0x80 covers Cyrillic etc.
    }
    return false;
}

// The whole file into `out` (bytes as they are). False if it cannot be opened or read; what was
// read before a read error is still in `out`. With `maxBytes`, also false, and nothing read, when
// the file is bigger than that or its size cannot be found. std only (common.cpp).
bool ReadFileToString(const std::filesystem::path& file, std::string* out,
                      std::uintmax_t maxBytes = std::numeric_limits<std::uintmax_t>::max());

// The folder that contains lexia.exe (settings and models are looked up there).
// Defined in common.cpp (Win32).
std::filesystem::path ExeDir();

// UTF-8 to UTF-16, for Win32 calls. Defined in common.cpp (Win32).
std::wstring Utf8ToWide(const std::string& s);

// UTF-16 to UTF-8, for text that comes from Win32. nullptr and "" give "". Defined in common.cpp (Win32).
std::string WideToUtf8(const wchar_t* w);
inline std::string WideToUtf8(const std::wstring& w) { return WideToUtf8(w.c_str()); }

// This program's command-line arguments after the program name, as UTF-8, split by Windows' own
// rules (CommandLineToArgvW). Defined in common.cpp (Win32).
std::vector<std::string> ProgramArguments();

// `%LOCALAPPDATA%\Lexia`, where settings.json, lexia.log and the user's Voices live; empty if
// LOCALAPPDATA is not set. Logs nothing, so the logger can call it. Defined in common.cpp (Win32).
std::filesystem::path LexiaDataDir();

// `%LOCALAPPDATA%\Lexia\voices`, where Voices the user adds or downloads live (tts/voice_library.h);
// empty if LOCALAPPDATA is not set. Defined in common.cpp (Win32).
std::filesystem::path UserVoicesDir();
