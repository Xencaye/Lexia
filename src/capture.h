#pragma once
// Screen capture: a frozen copy of one monitor.
#include <windows.h>

#include <cstdint>

#include <opencv2/core.hpp>

// One normal copy and one dimmed copy of a monitor.
struct Shot {
    int ox = 0, oy = 0;  // top-left of the monitor on the virtual desktop
    int w = 0, h = 0;
    HDC origDc = nullptr, dimDc = nullptr;
    HBITMAP origBmp = nullptr, dimBmp = nullptr;
    HGDIOBJ origOld = nullptr, dimOld = nullptr;
    uint32_t* origBits = nullptr;
    uint32_t* dimBits = nullptr;

    Shot() = default;
    Shot(const Shot&) = delete;
    Shot& operator=(const Shot&) = delete;
    ~Shot();

    // `mon` is the monitor's rectangle in virtual-desktop coordinates.
    bool Capture(const RECT& mon);

    // Full-colour copy for OCR (colour helps the neural detector on busy backgrounds).
    cv::Mat ToBgr() const;
};

// The monitor the user last interacted with = the one showing the foreground window.
// (Falls back to the monitor under the mouse cursor, then to the primary monitor.)
RECT ActiveMonitorRect();
