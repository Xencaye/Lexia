// lexia_barshot: a developer tool (not packaged). Makes a Control bar window, puts it in the state the
// command line names and writes the picture the bar draws, shadow margin included, to a PNG. The bar
// is drawn by the same code as in a Session, so the picture is what the Session shows (except that no
// Overlay is under it). Compare it with wireframe/control_bar_*.png.
//
//   lexia_barshot --size large|small --state idle|speaking|paused|analysing
//                 [--hover pause|skip|stop|help|size] [--theme light|dark] [--dpi 96] out.png
// The picture is the bar plus a margin of 32 px (at 96 dpi) on each side, for its shadow.
#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <string>

#include "controlbar.h"

// Reads the options, makes the bar on a pretend 1920 x 1080 monitor, and writes its picture.
int wmain(int argc, wchar_t** argv) {
    ControlBarConfig cfg;
    cfg.monitor = RECT{0, 0, 1920, 1080};
    cfg.theme = "light";
    std::wstring out;
    std::wstring state = L"speaking", hover;
    for (int i = 1; i < argc; ++i) {
        const std::wstring a = argv[i];
        auto next = [&]() -> std::wstring { return i + 1 < argc ? std::wstring(argv[++i]) : std::wstring(); };
        if (a == L"--size") {
            cfg.size = next() == L"small" ? BarSize::Small : BarSize::Large;
        } else if (a == L"--state") {
            state = next();
        } else if (a == L"--hover") {
            hover = next();
        } else if (a == L"--theme") {
            cfg.theme = next() == L"dark" ? "dark" : "light";
        } else if (a == L"--dpi") {
            cfg.dpi = _wtoi(next().c_str());
        } else {
            out = a;
        }
    }
    if (out.empty()) {
        std::fprintf(stderr, "usage: lexia_barshot --size large|small --state idle|speaking|paused|analysing "
                             "[--hover pause|skip|stop|help|size] [--theme light|dark] [--dpi N] out.png\n");
        return 2;
    }

    const bool speaking = state == L"speaking", paused = state == L"paused";
    cfg.speechState = [speaking, paused]() { return SpeechState{speaking || paused, paused}; };

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    int rc = 1;
    {
        ControlBar bar;
        if (!bar.Create(cfg)) {
            std::fprintf(stderr, "The bar could not be made.\n");
        } else {
            bar.SetAnalysing(state == L"analysing");
            BarButton h = BarButton::None;
            if (hover == L"pause") h = BarButton::Pause;
            else if (hover == L"skip") h = BarButton::Skip;
            else if (hover == L"stop") h = BarButton::Stop;
            else if (hover == L"help") h = BarButton::Help;
            else if (hover == L"size") h = BarButton::Size;
            if (bar.SaveSnapshot(out.c_str(), h)) {
                rc = 0;
            } else {
                std::fprintf(stderr, "The picture could not be written.\n");
            }
        }
    }
    CoUninitialize();
    return rc;
}
