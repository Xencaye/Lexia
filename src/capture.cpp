// The Shot: a frozen copy of one monitor, taken when the Overlay opens. Two 32-bit bitmaps are kept:
// the original (for OCR and for a Picked area, which is shown at full brightness) and a dimmed copy
// (the background of the Overlay).
#include "capture.h"

#include <opencv2/imgproc.hpp>

bool Shot::Capture(const RECT& mon) {
    ox = mon.left;
    oy = mon.top;
    w = mon.right - mon.left;
    h = mon.bottom - mon.top;
    if (w <= 0 || h <= 0) return false;

    // 32 bits per pixel (BGRA in memory), so a pixel is one uint32_t and OpenCV can wrap the bits.
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;  // negative = top-down: row 0 is the top of the screen
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    // A DIB section gives a memory DC to draw from and direct access to its pixels. The DCs and bitmaps
    // stay selected until the Shot is destroyed, because the Overlay paints from them.
    HDC screen = GetDC(nullptr);
    origDc = CreateCompatibleDC(screen);
    dimDc = CreateCompatibleDC(screen);
    origBmp = CreateDIBSection(screen, &bmi, DIB_RGB_COLORS,
                               reinterpret_cast<void**>(&origBits), nullptr, 0);
    dimBmp = CreateDIBSection(screen, &bmi, DIB_RGB_COLORS,
                              reinterpret_cast<void**>(&dimBits), nullptr, 0);
    if (!origBmp || !dimBmp || !origBits || !dimBits) {
        ReleaseDC(nullptr, screen);
        return false;  // the destructor frees whatever was made
    }
    origOld = SelectObject(origDc, origBmp);
    dimOld = SelectObject(dimDc, dimBmp);

    // Copy the monitor. CAPTUREBLT also takes layered (semi-transparent) windows, such as tooltips.
    BitBlt(origDc, 0, 0, w, h, screen, ox, oy, SRCCOPY | CAPTUREBLT);
    ReleaseDC(nullptr, screen);

    // The dimmed copy: each colour channel at half its value. The shift moves every byte one bit
    // down, and the mask clears the bit that each byte took from the byte above it (alpha becomes 0,
    // which BitBlt ignores).
    const size_t n = static_cast<size_t>(w) * h;
    for (size_t i = 0; i < n; ++i) {
        dimBits[i] = (origBits[i] >> 1) & 0x007F7F7Fu;
    }
    return true;
}

// The original pixels as a 3-channel BGR image, which is what the OCR models take.
cv::Mat Shot::ToBgr() const {
    cv::Mat bgra(h, w, CV_8UC4, origBits);  // wraps the DIB bits, no copy
    cv::Mat bgr;
    cv::cvtColor(bgra, bgr, cv::COLOR_BGRA2BGR);
    return bgr;
}

Shot::~Shot() {
    // Put the DCs' first bitmaps back before deleting ours: a bitmap that is selected into a DC
    // cannot be deleted.
    if (origDc && origOld) SelectObject(origDc, origOld);
    if (dimDc && dimOld) SelectObject(dimDc, dimOld);
    if (origBmp) DeleteObject(origBmp);
    if (dimBmp) DeleteObject(dimBmp);
    if (origDc) DeleteDC(origDc);
    if (dimDc) DeleteDC(dimDc);
}

// The Active monitor: the one that shows the foreground window (the game or page the user is
// looking at). Without a foreground window, the monitor under the mouse; as a last resort, the
// primary monitor's size at the origin.
RECT ActiveMonitorRect() {
    HMONITOR mon = nullptr;
    HWND fg = GetForegroundWindow();
    if (fg) mon = MonitorFromWindow(fg, MONITOR_DEFAULTTONEAREST);
    if (!mon) {
        POINT p = {};
        GetCursorPos(&p);
        mon = MonitorFromPoint(p, MONITOR_DEFAULTTOPRIMARY);
    }
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    if (mon && GetMonitorInfoW(mon, &mi)) return mi.rcMonitor;  // the whole monitor, taskbar included
    return RECT{0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
}
