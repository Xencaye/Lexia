#pragma once
// The logger. Every diagnostic goes through here: to the console, and to
// %LOCALAPPDATA%\Lexia\lexia.log so a friend's PC (no console) still leaves something to send.
// Win32-free header, so lexia_tests and lexia_cli can use it; the file handling is in log.cpp.
//
//   LogError("Could not load the OCR models from: ", dir);   // parts are streamed together
//   LogWarn / LogInfo / LogDebug likewise.
//
// Paths are written with u8string() (never string(), which throws or turns Cyrillic into
// mojibake). Messages are UTF-8. Thread-safe. A log file that cannot be opened is skipped
// silently and console output carries on.
//
// PRIVACY (ADR-0005): recognised text must never reach the log, at any level, because the log
// is meant to be attached to bug reports. Show it to the user with ConsolePrint(), which
// writes to the console only. Do not pass recognised text to LogDebug either.
#include <filesystem>
#include <sstream>
#include <string>

enum class LogLevel { Error, Warn, Info, Debug };

// Default level is Info; the environment variable LEXIA_LOG (error|warn|info|debug) overrides it.
// The level applies to the console and the file alike.
void SetLogLevel(LogLevel level);

// Where the file goes. The default is %LOCALAPPDATA%\Lexia\lexia.log (folder created if
// missing); this exists for tests. When the file passes ~1 MB it becomes <name>.old<ext>
// (lexia.old.log), replacing the previous one, and a fresh file is started.
void SetLogFile(const std::filesystem::path& file);

// One message (no trailing newline needed). A leading '\n' puts a blank line before it on the
// console and is dropped in the file. In the file the later lines of a message are indented and
// every other control character is written as an escape ("\r", "\t", "\xNN"), so only a real
// entry starts at the left edge.
void LogWrite(LogLevel level, const std::string& utf8);

// Text for the user's eyes only: console, never the file, at no level. For recognised text.
void ConsolePrint(const std::string& utf8);

namespace log_detail {
template <class T>
void Append(std::ostringstream& os, const T& v) { os << v; }
inline void Append(std::ostringstream& os, const std::filesystem::path& p) { os << p.u8string(); }

template <class... A>
std::string Join(const A&... parts) {
    std::ostringstream os;
    (Append(os, parts), ...);
    return os.str();
}
}  // namespace log_detail

template <class... A> void LogError(const A&... p) { LogWrite(LogLevel::Error, log_detail::Join(p...)); }
template <class... A> void LogWarn(const A&... p) { LogWrite(LogLevel::Warn, log_detail::Join(p...)); }
template <class... A> void LogInfo(const A&... p) { LogWrite(LogLevel::Info, log_detail::Join(p...)); }
template <class... A> void LogDebug(const A&... p) { LogWrite(LogLevel::Debug, log_detail::Join(p...)); }
