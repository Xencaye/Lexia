#pragma once
// TooltipWindow (lexia.exe): the small dark label that names a button (the Control bar's, and the
// Settings window's). Its own top-level popup, so it can reach outside the window that owns it.
//
// It never takes focus or the mouse (ADR-0001: the Overlay and the Control bar are over a game that
// must stay the foreground window): WS_EX_NOACTIVATE, WM_MOUSEACTIVATE -> MA_NOACTIVATE,
// WM_NCHITTEST -> HTTRANSPARENT, shown with SW_SHOWNOACTIVATE. Colours are the palette's (ink
// background, paper text), the font is Atkinson Hyperlegible Next Regular (native_ui/fonts.h), sizes
// are scaled with the DPI.
//
// UI thread only (the thread that made it).
#include <windows.h>

#include <string>

class TooltipWindow {
public:
    TooltipWindow() = default;
    ~TooltipWindow();
    TooltipWindow(const TooltipWindow&) = delete;
    TooltipWindow& operator=(const TooltipWindow&) = delete;

    // Makes the (hidden) window. The popup is owned by `owner`, so it is always above it and goes when
    // it goes. `dpi` scales the text and the padding. False (logged) if it cannot be made.
    bool Create(HWND owner, int dpi);
    void Destroy();
    void SetDpi(int dpi) { dpi_ = dpi > 0 ? dpi : 96; }

    // Shows `textUtf8` under the screen rectangle `anchor` (above it if there is no room below),
    // centred on it and kept inside the monitor that holds the anchor. Without activating anything.
    // Empty text hides the tooltip.
    void Show(const std::string& textUtf8, const RECT& anchor, bool dark);
    void Hide();
    bool Visible() const;

private:
    static bool EnsureClass();  // registers the window class once per process
    static LRESULT CALLBACK Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    void Paint(HWND hwnd);
    int Scale(int v) const;

    HWND hwnd_ = nullptr;
    int dpi_ = 96;
    bool dark_ = false;
    std::wstring text_;
};
