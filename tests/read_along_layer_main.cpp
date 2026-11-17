// A smoke test of the read-along layer (G2, src/read_along_layer.*) on a real window: it makes the layer over
// a small area, checks that it can never take the focus or a click (the extended styles, WM_NCHITTEST,
// WM_MOUSEACTIVATE, not the foreground window), that its timer asks for the heard word, and that it hides
// itself and calls `onEnded` once when no Reading is left. Run by ctest as `read_along_layer`.
#include <windows.h>

#include <cstdio>
#include <functional>
#include <optional>
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

// What a layer under test reports: how often it asked for the heard word, how often it said it had
// ended, and whether its Reading is still going on (the test turns `active` off to end it).
struct Probe {
    int polls = 0;
    int ended = 0;
    bool active = true;
};

// A layer config over `monitor` whose track holds Reading 5 with one Block ("one two three" on a Line at
// 10,10 - 140,30, as in the first part of main) and whose callbacks count into `probe`. `heard` is what
// heardWord answers; the default is the word "two" of Reading 5.
ReadAlongLayerConfig MakeConfig(Probe& probe, RECT monitor,
                                std::function<std::optional<HeardWord>(int poll)> heard = nullptr) {
    ReadAlongLayerConfig cfg;
    cfg.monitor = monitor;
    cfg.title = L"Lexia read-along (test)";
    TextBlock block;
    block.text = "one two three";
    TextLine line;
    line.box = Rect{10, 10, 140, 30};
    line.text = block.text;
    block.lines.push_back(line);
    block.box = line.box;
    cfg.track.Add(5, {block});
    cfg.heardWord = [&probe, heard]() -> std::optional<HeardWord> {
        const int poll = probe.polls++;
        if (heard) return heard(poll);
        return HeardWord{5, 0, TextRange{4, 7}};
    };
    cfg.isActive = [&probe](uint64_t id) { return probe.active && id == 5; };
    cfg.onEnded = [&probe]() { ++probe.ended; };
    return cfg;
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

    // ---- More cases. Each one has its own layer and its own counters, so one cannot hide another. ----

    // 1. The window covers exactly the monitor's area (a non-zero origin, as on a second monitor).
    {
        Probe probe;
        const RECT area = {40, 50, 440, 350};
        ReadAlongLayer l;
        Check(l.Create(MakeConfig(probe, area)), "[area] Create succeeds");
        RECT got = {};
        Check(l.Handle() != nullptr && GetWindowRect(l.Handle(), &got) != FALSE, "[area] the window has a rectangle");
        Check(got.left == area.left && got.top == area.top && got.right == area.right && got.bottom == area.bottom,
              "[area] the window covers exactly the monitor's area");
    }

    // 2. A display change ends the layer (it no longer fits the monitor), also while its Reading goes on.
    {
        Probe probe;
        ReadAlongLayer l;
        Check(l.Create(MakeConfig(probe, RECT{0, 0, 400, 300})), "[display] Create succeeds");
        const HWND h = l.Handle();
        Check(h != nullptr, "[display] the layer has a window");
        if (h) {
            Check(!l.Ended(), "[display] it runs before the message");
            SendMessageW(h, WM_DISPLAYCHANGE, 32, MAKELPARAM(400, 300));
            Check(l.Ended(), "[display] WM_DISPLAYCHANGE ends the layer");
            Check(IsWindowVisible(h) == FALSE, "[display] the window is hidden");
            Check(probe.ended == 1, "[display] onEnded was called once");
            const int pollsAtEnd = probe.polls;
            Pump(150, [] { return false; });
            Check(probe.polls == pollsAtEnd, "[display] the timer asks for the heard word no more");
            Check(probe.ended == 1, "[display] onEnded is not called again");
        }
    }

    // 3. No Reading is active when the layer is made: it ends inside Create, before it asked for a word.
    {
        Probe probe;
        probe.active = false;
        ReadAlongLayer l;
        Check(l.Create(MakeConfig(probe, RECT{0, 0, 400, 300})), "[idle] Create succeeds");
        Check(probe.ended == 1, "[idle] onEnded was called once, inside Create");
        Check(l.Ended(), "[idle] Ended() is true");
        Check(l.Handle() == nullptr || IsWindowVisible(l.Handle()) == FALSE, "[idle] nothing is shown");
        Check(probe.polls == 0, "[idle] the heard word was never asked");
        Pump(100, [] { return false; });
        Check(probe.ended == 1 && probe.polls == 0, "[idle] it stays quiet");
    }

    // 4. With no isActive function (nobody to ask) the layer is finished at once, and without heardWord
    //    it never has a word to draw.
    {
        int endedCount = 0;
        ReadAlongLayerConfig layerCfg;
        layerCfg.monitor = RECT{0, 0, 400, 300};
        TextBlock blk;
        blk.text = "one two";
        TextLine ln;
        ln.box = Rect{10, 10, 70, 30};
        ln.text = blk.text;
        blk.lines.push_back(ln);
        blk.box = ln.box;
        layerCfg.track.Add(5, {blk});
        layerCfg.onEnded = [&]() { ++endedCount; };
        ReadAlongLayer l;
        Check(l.Create(std::move(layerCfg)), "[no callbacks] Create succeeds");
        Check(l.Ended(), "[no callbacks] it ends at once");
        Check(endedCount == 1, "[no callbacks] onEnded was called once");
    }

    // 5. Create twice on one object: the first window goes without telling its owner, the second runs.
    {
        Probe first, second;
        ReadAlongLayer l;
        Check(l.Create(MakeConfig(first, RECT{0, 0, 400, 300})), "[twice] the first Create succeeds");
        const HWND h1 = l.Handle();
        Check(l.Create(MakeConfig(second, RECT{0, 0, 400, 300})), "[twice] the second Create succeeds");
        const HWND h2 = l.Handle();
        Check(h1 != nullptr && h2 != nullptr, "[twice] both made a window");
        if (h1 != h2) Check(IsWindow(h1) == FALSE, "[twice] the first window is gone");
        Check(first.ended == 0, "[twice] the first onEnded was not called");
        Check(!l.Ended(), "[twice] the second layer runs");
        const int firstPolls = first.polls;
        const int secondPolls = second.polls;
        Pump(300, [&] { return second.polls >= secondPolls + 3; });
        Check(second.polls >= secondPolls + 3, "[twice] the second layer polls");
        Check(first.polls == firstPolls, "[twice] the first layer's timer is gone");
        Check(first.ended == 0 && second.ended == 0, "[twice] neither has ended");
    }

    // 6. Words at the edge: one partly outside the area, one wholly outside (right, below), one of a
    //    Reading the track does not know and one of a Block it does not have. No crash, and the layer stays
    //    while its Reading is active.
    {
        Probe probe;
        Probe unused;
        ReadAlongLayerConfig layerCfg = MakeConfig(unused, RECT{0, 0, 400, 300});
        auto edge = [](Rect box) {
            TextBlock b;
            b.text = "aaaaaaaaaa";
            TextLine ln;
            ln.box = box;
            ln.text = b.text;
            b.lines.push_back(ln);
            b.box = box;
            return b;
        };
        // Block 0 reaches past the right and bottom edges; Block 1 is wholly right of the area, Block 2 wholly below.
        layerCfg.track.Add(5, {edge(Rect{350, 280, 450, 298}), edge(Rect{500, 10, 600, 30}), edge(Rect{10, 310, 100, 330})});
        layerCfg.heardWord = [&probe]() -> std::optional<HeardWord> {
            const int poll = probe.polls++;
            switch (poll % 5) {
            case 0: return HeardWord{5, 0, TextRange{0, 10}};  // partly outside: clipped
            case 1: return HeardWord{5, 1, TextRange{0, 10}};  // wholly outside
            case 2: return HeardWord{5, 2, TextRange{0, 10}};  // wholly outside
            case 3: return HeardWord{99, 0, TextRange{0, 10}}; // a Reading the track does not know
            default: return HeardWord{5, 7, TextRange{0, 10}}; // a Block it does not have
            }
        };
        layerCfg.isActive = [&probe](uint64_t id) { return probe.active && id == 5; };
        layerCfg.onEnded = [&probe]() { ++probe.ended; };
        ReadAlongLayer l;
        Check(l.Create(std::move(layerCfg)), "[edge] Create succeeds");
        Pump(1500, [&] { return probe.polls >= 10; });
        Check(probe.polls >= 10, "[edge] it polled through every kind of word");
        Check(!l.Ended() && probe.ended == 0, "[edge] it stays while its Reading is active");
        Check(l.Handle() != nullptr && IsWindowVisible(l.Handle()) != FALSE, "[edge] it is still shown");
    }

    // 7. After Destroy the timer is gone: the heard word is not asked again, and the layer is Ended.
    {
        Probe probe;
        ReadAlongLayer l;
        Check(l.Create(MakeConfig(probe, RECT{0, 0, 400, 300})), "[destroy] Create succeeds");
        Pump(100, [] { return false; });
        Check(probe.polls >= 1, "[destroy] it polled while it ran");
        l.Destroy();
        Check(l.Ended(), "[destroy] Ended() is true after Destroy");
        Check(l.Handle() == nullptr, "[destroy] the window is gone");
        const int pollsAtDestroy = probe.polls;
        Pump(150, [] { return false; });
        Check(probe.polls == pollsAtDestroy, "[destroy] the heard word is not asked again");
        Check(probe.ended == 0, "[destroy] Destroy does not call onEnded");
    }

    if (failures == 0) std::printf("OK read_along_layer\n");
    return failures == 0 ? 0 : 1;
}
