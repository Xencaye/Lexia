#pragma once
// The tray icon and its native popup menu (D2; spec ui/dev/tray.html). Thin Win32 glue: what the
// menu holds is decided by tray_menu.h (pure, tested). Everything runs on the UI thread.
#include <windows.h>

#include <string>
#include <vector>

#include "tray_menu.h"

class TrayIcon {
public:
    TrayIcon() = default;
    ~TrayIcon() { Remove(); }
    TrayIcon(const TrayIcon&) = delete;
    TrayIcon& operator=(const TrayIcon&) = delete;

    // Puts the icon in the notification area; `callbackMsg` is the window message it sends to
    // `hwnd` (version 4: LOWORD(lParam) is the event). Also what to call again after the taskbar
    // was re-created. False if Windows refused (logged).
    bool Add(HWND hwnd, UINT callbackMsg, bool on, const std::wstring& tooltip);
    // The On / Off look and the tooltip. Adds the icon if it is missing.
    void Update(bool on, const std::wstring& tooltip);
    void Remove();
    bool Added() const { return added_; }

    // The popup menu at `screenPos`, owned by `hwnd`. `labels[i]` is the text of items[i].
    // Returns the chosen command, None if it was dismissed.
    static TrayCommand ShowMenu(HWND hwnd, POINT screenPos, const std::vector<TrayMenuItem>& items,
                                const std::vector<std::wstring>& labels);

private:
    bool Notify(DWORD message, bool on, const std::wstring& tooltip);

    HWND hwnd_ = nullptr;
    UINT callbackMsg_ = 0;
    bool added_ = false;
};
