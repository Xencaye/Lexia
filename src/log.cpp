// The logger's console and file sinks (see log.h).
#include "log.h"

#include <windows.h>

#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iostream>
#include <iterator>
#include <mutex>

#include "common.h"

namespace fs = std::filesystem;

namespace {

constexpr std::uintmax_t kMaxFileBytes = 1024 * 1024;

// The one logger of the process. Every field is used under `mu`.
struct Logger {
    std::mutex mu;  // OCR runs on worker threads
    LogLevel level = LogLevel::Info;
    bool levelFixed = false;  // LEXIA_LOG read, or SetLogLevel() called
    fs::path file;            // set by SetLogFile(), else resolved on first use
    bool fileSet = false;
    bool fileTried = false;
    std::ofstream out;
    std::uintmax_t size = 0;
};

// Made on first use, so logging works even from static initialisers.
Logger& L() {
    static Logger l;
    return l;
}

// The level as five characters, so the messages in the file line up.
const char* Tag(LogLevel lv) {
    switch (lv) {
        case LogLevel::Error: return "ERROR";
        case LogLevel::Warn: return "WARN ";
        case LogLevel::Info: return "INFO ";
        case LogLevel::Debug: return "DEBUG";
    }
    return "?????";
}

// LEXIA_LOG, read once on first use. Unicode API so nothing depends on the ANSI code page.
void ApplyEnvLevel(Logger& l) {
    if (l.levelFixed) return;
    l.levelFixed = true;
    wchar_t buf[16];
    const DWORD n = GetEnvironmentVariableW(L"LEXIA_LOG", buf, static_cast<DWORD>(std::size(buf)));
    if (n == 0 || n >= std::size(buf)) return;
    const std::wstring v(buf, n);
    if (v == L"error") l.level = LogLevel::Error;
    else if (v == L"warn") l.level = LogLevel::Warn;
    else if (v == L"info") l.level = LogLevel::Info;
    else if (v == L"debug") l.level = LogLevel::Debug;
}

// %LOCALAPPDATA%\Lexia\lexia.log; empty when LOCALAPPDATA is not set (then there is no log file).
fs::path DefaultLogFile() {
    const fs::path dataDir = LexiaDataDir();  // logs nothing itself
    return dataDir.empty() ? dataDir : dataDir / L"lexia.log";
}

// Opens the log file for appending; leaves `out` closed if anything goes wrong.
void OpenFile(Logger& l) {
    l.fileTried = true;
    try {
        if (!l.fileSet) l.file = DefaultLogFile();
        if (l.file.empty()) return;
        std::error_code ec;
        if (l.file.has_parent_path()) fs::create_directories(l.file.parent_path(), ec);
        const std::uintmax_t existing = fs::file_size(l.file, ec);
        l.size = ec ? 0 : existing;
        l.out.open(l.file, std::ios::app | std::ios::binary);  // binary: we write "\n" ourselves
        if (!l.out) l.out.close();
    } catch (...) {
        l.out.close();
    }
}

// Keeps one previous file: lexia.log -> lexia.old.log, then starts a fresh lexia.log.
void Rotate(Logger& l) {
    l.out.close();
    std::error_code ec;
    fs::path old = l.file;
    old.replace_extension(L".old" + l.file.extension().wstring());
    fs::remove(old, ec);
    fs::rename(l.file, old, ec);
    if (ec) fs::remove(l.file, ec);  // could not keep it: at least start fresh
    l.size = 0;
    l.out.open(l.file, std::ios::trunc | std::ios::binary);
    if (!l.out) l.out.close();
}

// Local time with milliseconds: "2026-11-05 03:56:50.123".
std::string Timestamp() {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const std::time_t t = system_clock::to_time_t(now);
    const int ms = static_cast<int>(duration_cast<milliseconds>(now.time_since_epoch()).count() % 1000);
    std::tm tm{};
    localtime_s(&tm, &t);
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d.%03d", tm.tm_year + 1900,
                  tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, ms);
    return buf;
}

// Every line after the first of a message is indented, and other control characters are written
// as escapes ("\r", "\t", else "\xNN"): only a real entry starts at the left edge, so a Voice or
// file name holding a newline cannot forge a log line. A message of several lines stays readable.
std::string EscapeControls(const std::string& msg) {
    std::string out;
    out.reserve(msg.size());
    for (const char ch : msg) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c >= 0x20) {
            out += ch;
        } else if (c == '\n') {
            out += "\n    ";
        } else if (c == '\r') {
            out += "\\r";
        } else if (c == '\t') {
            out += "\\t";
        } else {
            char hex[8];
            std::snprintf(hex, sizeof(hex), "\\x%02X", static_cast<unsigned>(c));
            out += hex;
        }
    }
    return out;
}

// One entry in the file: "<time> [LEVEL] <message>". The file is opened on first use and rotated
// before it would pass kMaxFileBytes. Each entry is flushed at once.
void WriteFile(Logger& l, LogLevel level, const std::string& msg) {
    if (!l.fileTried) OpenFile(l);
    if (!l.out.is_open()) return;
    const std::string line = Timestamp() + " [" + Tag(level) + "] " + EscapeControls(msg) + "\n";
    if (l.size > 0 && l.size + line.size() > kMaxFileBytes) {
        Rotate(l);
        if (!l.out.is_open()) return;
    }
    l.out.write(line.data(), static_cast<std::streamsize>(line.size()));
    l.out.flush();  // a crash must not lose the last lines
    if (!l.out) {
        l.out.close();  // disk full etc.: give up on the file, keep the console
        return;
    }
    l.size += line.size();
}

}  // namespace

void SetLogLevel(LogLevel level) {
    Logger& l = L();
    std::lock_guard<std::mutex> lock(l.mu);
    l.levelFixed = true;  // an explicit call wins over LEXIA_LOG
    l.level = level;
}

// The next entry opens this file instead (the tests use it).
void SetLogFile(const fs::path& file) {
    Logger& l = L();
    std::lock_guard<std::mutex> lock(l.mu);
    l.out.close();
    l.file = file;
    l.fileSet = true;
    l.fileTried = false;
}

// One message (LogError / LogWarn / LogInfo / LogDebug build it): to the console, and to the file
// without its leading blank lines (they only space the console output).
void LogWrite(LogLevel level, const std::string& utf8) {
    Logger& l = L();
    std::lock_guard<std::mutex> lock(l.mu);
    ApplyEnvLevel(l);
    if (level > l.level) return;

    try {
        // Console: errors and warnings on stderr, the rest on stdout. UTF-8 bytes as they are;
        // main() sets the console code page to UTF-8.
        std::ostream& con = level <= LogLevel::Warn ? std::cerr : std::cout;
        con << utf8 << std::endl;

        size_t skip = 0;
        while (skip < utf8.size() && utf8[skip] == '\n') ++skip;
        WriteFile(l, level, utf8.substr(skip));
    } catch (...) {
        // Logging must never take Lexia down.
    }
}

// Console only, never the file: this is where recognised text goes (ADR-0005).
void ConsolePrint(const std::string& utf8) {
    Logger& l = L();
    std::lock_guard<std::mutex> lock(l.mu);
    try {
        std::cout << utf8 << std::endl;
    } catch (...) {
        // Same: the console may be gone.
    }
}
