#include "settings_watch.h"

#include <cstdint>
#include <system_error>

#include "log.h"

namespace fs = std::filesystem;

namespace {

// Size and last-write time together say "the file changed" well enough for a person editing it.
struct Stamp {
    std::uintmax_t size = 0;
    fs::file_time_type time{};
    bool exists = false;
    bool operator==(const Stamp& o) const {
        return exists == o.exists && size == o.size && time == o.time;
    }
};

Stamp StampOf(const fs::path& file) {
    Stamp s;
    std::error_code ec;
    s.size = fs::file_size(file, ec);
    if (ec) return s;
    s.time = fs::last_write_time(file, ec);
    s.exists = !ec;
    return s;
}

}  // namespace

// Live settings reload (C4): a thread that waits for a change in the folder of settings.json and posts
// `msg` to `hwnd` when the file's Stamp changed. The App debounces the messages (an editor can write
// a file several times in one save) and then reads the file.
SettingsWatcher::SettingsWatcher(const fs::path& file, HWND hwnd, UINT msg) {
    stop_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);  // manual reset: set once, by the destructor
    thread_ = std::thread([this, file, hwnd, msg]() {
        // Watch the folder, not the file: editors often save by writing a new file and renaming it.
        HANDLE change = FindFirstChangeNotificationW(
            file.parent_path().c_str(), FALSE,
            FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_SIZE);
        if (change == INVALID_HANDLE_VALUE) {
            LogWarn("[settings] Can't watch ", file.parent_path(),
                    " for changes; edits to settings.json need a restart.");
            return;
        }
        Stamp seen = StampOf(file);
        // Loop while the folder changes (handle 1). The stop event (handle 0), or a failure, ends it.
        // A change of another file in the folder gives the same Stamp, so it posts nothing.
        const HANDLE handles[2] = {stop_, change};
        while (WaitForMultipleObjects(2, handles, FALSE, INFINITE) == WAIT_OBJECT_0 + 1) {
            const Stamp now = StampOf(file);
            if (!(now == seen)) {
                seen = now;
                PostMessageW(hwnd, msg, 0, 0);
            }
            if (!FindNextChangeNotification(change)) break;
        }
        FindCloseChangeNotification(change);
    });
}

SettingsWatcher::~SettingsWatcher() {
    SetEvent(stop_);
    if (thread_.joinable()) thread_.join();
    CloseHandle(stop_);
}
