#include "tray.h"

#include <shellapi.h>

#include "log.h"
#include "resource_ids.h"

// The tray icon (D2): Lexia's only visible sign while the Settings window is closed. Clicks on it come
// to the App window as callbackMsg_; the right-click menu is a native popup menu.
namespace {

const UINT kTrayId = 1;  // Lexia has one icon; the shell knows it by (window, id)

HICON LoadTrayIcon(bool on) {
    // The frame nearest the notification area's size: 16 px at 100 %, 20, 24, 32 at higher scaling.
    const UINT dpi = GetDpiForSystem();
    const int cx = GetSystemMetricsForDpi(SM_CXSMICON, dpi);
    const int cy = GetSystemMetricsForDpi(SM_CYSMICON, dpi);
    return static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr),
                                         MAKEINTRESOURCEW(on ? IDI_LEXIA : IDI_LEXIA_OFF), IMAGE_ICON,
                                         cx, cy, LR_DEFAULTCOLOR | LR_SHARED));
}

}  // namespace

// One Shell_NotifyIconW call (NIM_ADD or NIM_MODIFY) with the icon of the On / Off state and the
// tooltip. A tooltip longer than the shell's 128 characters is cut off.
bool TrayIcon::Notify(DWORD message, bool on, const std::wstring& tooltip) {
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd_;
    nid.uID = kTrayId;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    nid.uCallbackMessage = callbackMsg_;
    nid.hIcon = LoadTrayIcon(on);
    wcsncpy_s(nid.szTip, tooltip.c_str(), _TRUNCATE);
    if (!Shell_NotifyIconW(message, &nid)) return false;
    if (message == NIM_ADD) {
        nid.uVersion = NOTIFYICON_VERSION_4;  // WM_CONTEXTMENU / NIN_SELECT, the icon's own position
        Shell_NotifyIconW(NIM_SETVERSION, &nid);
    }
    return true;
}

bool TrayIcon::Add(HWND hwnd, UINT callbackMsg, bool on, const std::wstring& tooltip) {
    hwnd_ = hwnd;
    callbackMsg_ = callbackMsg;
    Remove();  // a stale entry (we are adding again) is replaced, not doubled
    added_ = Notify(NIM_ADD, on, tooltip);
    if (!added_) LogWarn("[tray] The tray icon could not be added; use the keys. Windows may add it later.");
    return added_;
}

// Shows a new state. If the icon is gone (for example, Explorer restarted and did not take it back),
// it is added again.
void TrayIcon::Update(bool on, const std::wstring& tooltip) {
    if (!hwnd_) return;
    if (!added_ || !Notify(NIM_MODIFY, on, tooltip)) Add(hwnd_, callbackMsg_, on, tooltip);
}

void TrayIcon::Remove() {
    if (!added_) return;
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd_;
    nid.uID = kTrayId;
    Shell_NotifyIconW(NIM_DELETE, &nid);
    added_ = false;
}

// Shows the menu at screenPos (the menu's bottom-right corner goes there, as the tray is usually at the
// bottom right) and waits until the user chooses a row or closes the menu. labels[i] is the text of
// items[i]. Returns the chosen command, or None.
TrayCommand TrayIcon::ShowMenu(HWND hwnd, POINT screenPos, const std::vector<TrayMenuItem>& items,
                               const std::vector<std::wstring>& labels) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return TrayCommand::None;
    for (size_t i = 0; i < items.size(); ++i) {
        const TrayMenuItem& item = items[i];
        MENUITEMINFOW mi = {};
        mi.cbSize = sizeof(mi);
        if (item.kind == TrayMenuItem::Kind::Separator) {
            mi.fMask = MIIM_FTYPE;
            mi.fType = MFT_SEPARATOR;
        } else {
            mi.fMask = MIIM_FTYPE | MIIM_STATE | MIIM_ID | MIIM_STRING;
            mi.fType = MFT_STRING | (item.kind == TrayMenuItem::Kind::Radio ? MFT_RADIOCHECK : 0);
            mi.fState = (item.checked ? MFS_CHECKED : MFS_UNCHECKED) | (item.isDefault ? MFS_DEFAULT : 0);
            mi.wID = static_cast<UINT>(item.command);
            mi.dwTypeData = const_cast<wchar_t*>(labels[i].c_str());
        }
        InsertMenuItemW(menu, static_cast<UINT>(i), TRUE, &mi);
    }
    // Windows asks for this order with a tray menu: our window in front, the menu, then WM_NULL,
    // else the menu does not close when the user clicks somewhere else.
    SetForegroundWindow(hwnd);
    const UINT cmd = static_cast<UINT>(TrackPopupMenu(
        menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_RIGHTALIGN, screenPos.x, screenPos.y, 0,
        hwnd, nullptr));
    PostMessageW(hwnd, WM_NULL, 0, 0);
    DestroyMenu(menu);
    switch (cmd) {  // only the commands the menu has; 0 (closed) and anything else = None
    case static_cast<UINT>(TrayCommand::On): return TrayCommand::On;
    case static_cast<UINT>(TrayCommand::Off): return TrayCommand::Off;
    case static_cast<UINT>(TrayCommand::Open): return TrayCommand::Open;
    case static_cast<UINT>(TrayCommand::Quit): return TrayCommand::Quit;
    default: return TrayCommand::None;
    }
}
