#pragma once
// The word mark layer (G2, "keep the word mark after I leave"). With that setting on, a Session that
// ends while a Reading of it is still being read leaves this behind: a window over the monitor that
// draws only the underline bar under the heard word (G1, read_along.h) and nothing else. It is
// transparent to the mouse and never active, so the game or page under it works as if it were not
// there. It ends by itself when none of its Readings is in the Reading queue any more; the App also
// ends it on Stop, Off, a new Session and Quit.
//
// Windows: a top-level popup with WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE |
// WS_EX_TOOLWINDOW | WS_EX_TOPMOST, shown with SW_SHOWNOACTIVATE. WM_MOUSEACTIVATE answers
// MA_NOACTIVATE and WM_NCHITTEST answers HTTRANSPARENT, so a click goes through even if the layered
// pixel under it were opaque (ADR-0001: nothing of Lexia may take the focus from a game). It takes no
// input of any kind and uses no file or network. Its picture is a 32-bit premultiplied DIB, shown
// with UpdateLayeredWindow (per-pixel alpha: everything but the bars is fully transparent).
#include <windows.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "read_along.h"

struct ReadAlongLayerConfig {
    RECT monitor = {};                 // the screen area it covers: the Shot's monitor
    COLORREF color = RGB(255, 200, 0); // the bars' colour (the Overlay's hover colour, as in the Session)
    std::wstring title;                // the window's name (strings WordMarkName); never shown
    ReadAlongTrack track;              // the Session's Readings and their Blocks (Shot coordinates)
    // The word heard now (Speaker::HeardWordNow) and whether a Reading is still in the Reading queue
    // (Speaker::HasReading). Both are called on the UI thread, on the layer's 30 ms timer.
    std::function<std::optional<HeardWord>()> heardWord;
    std::function<bool(uint64_t)> isActive;
    // Called once, on the UI thread, when the layer has ended by itself (its Readings are done, or the
    // display changed). The window is hidden by then. It runs inside the layer's window procedure, so it
    // must not delete the layer: the App posts itself a message and deletes it from there.
    std::function<void()> onEnded;
};

class ReadAlongLayer {
public:
    ReadAlongLayer();
    ~ReadAlongLayer();
    ReadAlongLayer(const ReadAlongLayer&) = delete;
    ReadAlongLayer& operator=(const ReadAlongLayer&) = delete;

    // Makes the window and shows the first bars at once. False (logged) if the window or its picture
    // could not be made; nothing is left on the screen then.
    bool Create(ReadAlongLayerConfig cfg);
    // Removes the window. Safe to call twice and from the destructor. Not from `onEnded`.
    void Destroy();
    // True once the layer has finished (its Readings ended) or was destroyed: it draws nothing more.
    bool Ended() const;
    HWND Handle() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
