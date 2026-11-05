#pragma once
// Tells a window when settings.json changes on disk. A thread waits on the file's folder and,
// when the file's size or time stamp differs from what it saw last, posts `msg` to `hwnd`. It
// posts once per change and says nothing about what changed; the receiver debounces and reads
// the file itself. Other files in the folder (lexia.log is one) are ignored.
#include <windows.h>

#include <filesystem>
#include <thread>

class SettingsWatcher {
public:
    SettingsWatcher(const std::filesystem::path& file, HWND hwnd, UINT msg);
    ~SettingsWatcher();  // stops the thread
    SettingsWatcher(const SettingsWatcher&) = delete;
    SettingsWatcher& operator=(const SettingsWatcher&) = delete;

private:
    HANDLE stop_ = nullptr;  // manual-reset event, set by the destructor
    std::thread thread_;
};
