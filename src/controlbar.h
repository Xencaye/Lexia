#pragma once
// The Control bar: the buttons that float above the Overlay during a Session (D4, redrawn in D8.4).
// Its look is the spec in ui/dev/controlbar.html (made from wireframe/control_bar_*.png); where its
// parts are is control_bar_layout.h; the icons are the SVG files of ui/icons (svg_icon.h).
//
// It is its own window (owned by the Overlay window, so always above it), layered with per-pixel
// alpha so its soft shadow needs no rectangle. It never takes focus (ADR-0001): WS_EX_NOACTIVATE,
// WM_MOUSEACTIVATE -> MA_NOACTIVATE, no SetForegroundWindow / SetFocus anywhere. A click or drag
// that starts on the bar is the bar's, so the Overlay never sees it as a Pick; outside the bar
// (and in its shadow) the window is transparent to the mouse. There is no Close button: the user
// leaves the Overlay with a right-click, or the Read key.
//
// Everything runs on the UI thread, in the Session's message loop.
#include <windows.h>

#include <functional>
#include <memory>
#include <string>

#include "control_bar_layout.h"

struct SpeechState {
    bool active = false;  // the Reading queue holds something (speaking or paused)
    bool paused = false;
};

struct ControlBarConfig {
    HWND owner = nullptr;  // the Overlay window
    RECT monitor = {};     // the Session's monitor, in virtual-desktop pixels
    BarSize size = BarSize::Large;
    int savedX = -1, savedY = -1;  // settings.json controlBar.x / .y: offsets from `monitor`, -1 = none
    std::string theme = "system";  // "system", "light" or "dark"
    std::string interfaceLanguage = "en";
    std::string pauseKey, speechKey;  // as shown in a tooltip; "" = the action has no key
    int dpi = 0;                      // 0 = the DPI of `owner`'s monitor (96 if there is no owner)
    bool leftHanded = false;          // the Pick button (mouse_role.h) uses the bar; the Leave button is swallowed

    // All called on the UI thread.
    std::function<SpeechState()> speechState;  // polled while the bar is up
    std::function<void()> onPause;             // Pause or Resume, whichever the bar shows
    std::function<void()> onSkip;
    std::function<void()> onStop;
    std::function<void()> onHelp;                          // Help: end the Session, then open the Help page
    std::function<void(BarSize, int, int)> onChanged;      // the size or the place changed: size, x, y (offsets)
    std::function<void()> onPointerEntered;                // the mouse came onto the bar from outside
};

class ControlBar {
public:
    ControlBar();
    ~ControlBar();
    ControlBar(const ControlBar&) = delete;
    ControlBar& operator=(const ControlBar&) = delete;

    // Makes and shows the bar without activating it. False (logged) if a window could not be made;
    // the Session then goes on without a bar.
    bool Create(const ControlBarConfig& config);
    void SetAnalysing(bool analysing);  // Pause, Skip and Stop are greyed while the Analysis runs
    void Destroy();

    // Developer tool (lexia_barshot): the picture the bar draws now, with `hover` drawn as hovered,
    // written to a PNG (premultiplied pixels converted by GDI+). False if it could not be written.
    bool SaveSnapshot(const wchar_t* pngPath, BarButton hover = BarButton::None);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
