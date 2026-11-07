#pragma once
// Data an older Lexia left behind that this one no longer uses. Lexia 0.5 and the first 0.6 builds showed the
// Settings window with the Microsoft Edge WebView2 Runtime, which kept its profile (cache, cookies, local
// storage) in %LOCALAPPDATA%\Lexia\WebView2. The native Settings window (ADR-0009) does not use it, so
// Lexia deletes that folder once, at start. Win32 (file attributes), no Lexia state.
#include <filesystem>

enum class StaleRemoval {
    Absent,   // there was no such folder: nothing to do (the usual case after the first start)
    Removed,  // the folder and everything in it is gone
    Failed,   // something could not be deleted (in use, no right); what could be was removed
};

// Deletes `dataDir`\WebView2 and what is in it, and nothing else. A reparse point (a junction or a symbolic
// link, at the top or anywhere inside) is NEVER followed: the link itself is removed, what it points at is
// untouched. `dataDir` must be the Lexia data folder (LexiaDataDir()); an empty path does nothing.
StaleRemoval RemoveStaleWebView2Data(const std::filesystem::path& dataDir);

// The folder name under the data folder (exposed for the log and the tests).
const wchar_t* StaleWebView2FolderName();
