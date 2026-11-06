#include "tooltip_window.h"

#include <algorithm>

#include "common.h"
#include "control_bar_layout.h"
#include "log.h"
#include "native_ui/fonts.h"
#include "native_ui/palette.h"

namespace {

constexpr wchar_t kClassName[] = L"LexiaTooltip";

// 0xRRGGBB (the palette's form) -> a GDI COLORREF.
COLORREF ToColorRef(uint32_t rgb) {
    return RGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
}

}  // namespace

bool TooltipWindow::EnsureClass() {
    static bool registered = false;
    if (registered) return true;
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpfnWndProc = Proc;
    wc.lpszClassName = kClassName;
    if (!RegisterClassExW(&wc)) {
        // The class may exist already (made by another module's copy of this code); that is fine.
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
    }
    registered = true;
    return true;
}

TooltipWindow::~TooltipWindow() { Destroy(); }

int TooltipWindow::Scale(int v) const { return ScaleForDpi(v, dpi_); }

bool TooltipWindow::Create(HWND owner, int dpi) {
    SetDpi(dpi);
    if (hwnd_) return true;
    if (!EnsureClass()) {
        LogError("[tooltip] Could not register the window class (", GetLastError(), ").");
        return false;
    }
    // A popup that is never active, always on top, not in the taskbar or Alt+Tab.
    hwnd_ = CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_TOOLWINDOW, kClassName, L"", WS_POPUP, 0, 0, 1, 1,
                            owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!hwnd_) {
        LogError("[tooltip] Could not create the window (", GetLastError(), ").");
        return false;
    }
    SetWindowLongPtrW(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    return true;
}

void TooltipWindow::Destroy() {
    if (!hwnd_) return;
    // The owner's destruction may have taken the popup already.
    if (IsWindow(hwnd_)) {
        SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0);  // no paint may reach this object any more
        DestroyWindow(hwnd_);
    }
    hwnd_ = nullptr;
}

bool TooltipWindow::Visible() const { return hwnd_ && IsWindow(hwnd_) && IsWindowVisible(hwnd_); }

void TooltipWindow::Hide() {
    if (hwnd_ && IsWindow(hwnd_)) ShowWindow(hwnd_, SW_HIDE);
}

void TooltipWindow::Show(const std::string& textUtf8, const RECT& anchor, bool dark) {
    if (!hwnd_ || !IsWindow(hwnd_)) return;
    if (textUtf8.empty()) {
        Hide();
        return;
    }
    text_ = Utf8ToWide(textUtf8);
    dark_ = dark;

    // The size of the text in the UI font, measured on the window's own DC.
    HDC dc = GetDC(hwnd_);
    HFONT font = nui::CreateGdiFont(Scale(15), FW_NORMAL);
    const HGDIOBJ old = SelectObject(dc, font);
    SIZE ts = {};
    GetTextExtentPoint32W(dc, text_.c_str(), static_cast<int>(text_.size()), &ts);
    SelectObject(dc, old);
    DeleteObject(font);
    ReleaseDC(hwnd_, dc);

    const int w = ts.cx + 2 * Scale(12), h = ts.cy + 2 * Scale(6);
    // The monitor that holds the anchor decides where the label may go.
    MONITORINFO mi = {sizeof(mi)};
    GetMonitorInfoW(MonitorFromRect(&anchor, MONITOR_DEFAULTTONEAREST), &mi);
    int x = (anchor.left + anchor.right) / 2 - w / 2;
    int y = anchor.bottom + Scale(8);
    if (y + h > mi.rcMonitor.bottom) y = anchor.top - Scale(8) - h;  // no room below: above
    x = std::max(static_cast<int>(mi.rcMonitor.left), std::min(x, static_cast<int>(mi.rcMonitor.right) - w));
    SetWindowPos(hwnd_, HWND_TOPMOST, x, y, w, h, SWP_NOACTIVATE);
    // Rounded corners; the system owns the region after this call.
    SetWindowRgn(hwnd_, CreateRoundRectRgn(0, 0, w + 1, h + 1, Scale(10) * 2, Scale(10) * 2), TRUE);
    InvalidateRect(hwnd_, nullptr, FALSE);
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
}

void TooltipWindow::Paint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    RECT rc;
    GetClientRect(hwnd, &rc);
    // Ink for the label, paper for its text: the same pair as the Control bar's words.
    const nui::Palette& pal = nui::PaletteFor(dark_);
    HBRUSH bg = CreateSolidBrush(ToColorRef(pal.ink.rgb));
    FillRect(dc, &rc, bg);
    DeleteObject(bg);
    HFONT font = nui::CreateGdiFont(Scale(15), FW_NORMAL);
    const HGDIOBJ old = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, ToColorRef(pal.paper.rgb));
    DrawTextW(dc, text_.c_str(), static_cast<int>(text_.size()), &rc,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(dc, old);
    DeleteObject(font);
    EndPaint(hwnd, &ps);
}

// The tooltip's procedure: it takes no focus and no mouse, and paints its text.
LRESULT CALLBACK TooltipWindow::Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    TooltipWindow* self = reinterpret_cast<TooltipWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_NCHITTEST:
        return HTTRANSPARENT;  // a tooltip is never under the mouse: clicks go to what is below
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        if (self) {
            self->Paint(hwnd);
        } else {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
        }
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
