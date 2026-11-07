// A smoke test of the read-along layer (G2, src/read_along_layer.*) on a real window: it makes the layer over
// a small area, checks that it can never take the focus or a click (the extended styles, WM_NCHITTEST,
// WM_MOUSEACTIVATE, not the foreground window), that its timer asks for the heard word, and that it hides
// itself and calls `onEnded` once when no Reading is left. Run by ctest as `read_along_layer`.
#include <windows.h>

#include <cstdio>
#include <string>
#include <vector>

#include "read_along_layer.h"

namespace {

int failures = 0;

void Check(bool ok, const std::string& what) {
    if (!ok) {
        ++failures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

// Handles window messages (the layer's timer) for `ms` milliseconds, or until `done()` is true.
template <typename Done>
void Pump(int ms, Done done) {
    const ULONGLONG until = GetTickCount64() + static_cast<ULONGLONG>(ms);
    while (GetTickCount64() < until && !done()) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        Sleep(5);
    }
}

}  // namespace

int main() {
    ReadAlongLayerConfig cfg;
    cfg.monitor = RECT{0, 0, 400, 300};
    cfg.title = L"Lexia read-along (test)";
    TextBlock block;
    block.text = "one two three";
    TextLine line;
    line.box = Rect{10, 10, 140, 30};
    line.text = block.text;
    block.lines.push_back(line);
    block.box = line.box;
    cfg.track.Add(5, {block});

    int polls = 0;
    bool active = true;
    int ended = 0;
    cfg.heardWord = [&]() -> std::optional<HeardWord> {
        ++polls;
        return HeardWord{5, 0, TextRange{4, 7}};
    };
    cfg.isActive = [&](uint64_t id) { return active && id == 5; };
    cfg.onEnded = [&]() { ++ended; };

    ReadAlongLayer layer;
    Check(layer.Create(std::move(cfg)), "Create succeeds");
    const HWND hwnd = layer.Handle();
    Check(hwnd != nullptr && IsWindow(hwnd), "the layer has a window");
    if (!hwnd) return 1;

    // It can never take the focus or the mouse: the styles, and the answers to the two messages.
    const LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    for (LONG_PTR style : {static_cast<LONG_PTR>(WS_EX_LAYERED), static_cast<LONG_PTR>(WS_EX_TRANSPARENT),
                           static_cast<LONG_PTR>(WS_EX_NOACTIVATE), static_cast<LONG_PTR>(WS_EX_TOOLWINDOW),
                           static_cast<LONG_PTR>(WS_EX_TOPMOST)}) {
        Check((ex & style) == style, "extended style 0x" + std::to_string(style));
    }
    Check((GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_CHILD) == 0, "it is a top-level popup");
    Check(SendMessageW(hwnd, WM_NCHITTEST, 0, MAKELPARAM(20, 20)) == HTTRANSPARENT, "WM_NCHITTEST is HTTRANSPARENT");
    Check(SendMessageW(hwnd, WM_MOUSEACTIVATE, 0, MAKELPARAM(HTCLIENT, WM_LBUTTONDOWN)) == MA_NOACTIVATE,
          "WM_MOUSEACTIVATE is MA_NOACTIVATE");
    Check(IsWindowVisible(hwnd) != FALSE, "it is shown");
    Check(GetForegroundWindow() != hwnd, "it is not the foreground window");

    // The timer asks for the heard word, again and again; the first poll came with Create.
    Check(polls >= 1, "the first bars are asked for in Create");
    const int before = polls;
    Pump(300, [&] { return polls >= before + 3; });
    Check(polls >= before + 3, "the 30 ms timer polls the heard word");
    Check(!layer.Ended() && ended == 0, "it stays while a Reading is active");
    Check(GetForegroundWindow() != hwnd, "still not the foreground window");

    // The Reading ends: the layer hides itself and tells its owner once, but is not deleted by itself.
    active = false;
    Pump(1000, [&] { return ended > 0; });
    Check(ended == 1, "onEnded was called once");
    Check(layer.Ended(), "Ended() is true");
    Check(IsWindowVisible(hwnd) == FALSE, "the window is hidden when it ended");
    Pump(150, [] { return false; });
    Check(ended == 1, "onEnded is not called again");

    layer.Destroy();
    Check(layer.Handle() == nullptr, "Destroy removes the window");
    Check(IsWindow(hwnd) == FALSE, "the window is gone");
    layer.Destroy();  // twice is fine

    // An empty area is refused (nothing to draw on).
    ReadAlongLayerConfig none;
    none.monitor = RECT{0, 0, 0, 0};
    ReadAlongLayer refused;
    Check(!refused.Create(std::move(none)), "an empty area is refused");

    if (failures == 0) std::printf("OK read_along_layer\n");
    return failures == 0 ? 0 : 1;
}
