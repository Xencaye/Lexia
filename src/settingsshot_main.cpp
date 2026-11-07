// lexia_settingsshot: a developer tool (not packaged). Draws a page of the native Settings window with the
// same widget tree and the same Direct2D canvas as the window, off-screen, and writes a PNG, so the look can
// be compared with wireframe/html-reference/ without a window or a mouse. It carries the asset resources
// (icons, engravings, the font) like lexia_barshot, so it needs no ui\ folder.
//
//   lexia_settingsshot --page gallery|home|voices|settings|report|help|about [--on] [--hover <widget>]
//                      [--voices none|changed|user] [--sheet <name>] [--theme light|dark] [--dpi N] out.png
//
// The picture is 800 x 600 DIPs (the window's client area), so 1200 x 900 pixels at 144 dpi. On success
// it prints one line: the file, its size and the number of distinct colours (the smoke test checks that
// the picture is not one plain colour). Exit code 0 = written, 1 = failed, 2 = bad command line.
// Pages are in settingsshot_pages.*.
#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <set>
#include <vector>
#include <string>

#include "common.h"
#include "log.h"
#include "native_ui/d2d_canvas.h"
#include "native_ui/palette.h"
#include "native_ui/screen.h"
#include "settingsshot_pages.h"

namespace {

constexpr float kWindowW = 800, kWindowH = 600;  // the Settings window's client area in DIPs

int Usage() {
    std::fprintf(stderr,
                 "usage: lexia_settingsshot --page gallery|home|voices|settings|report|help|about [--on] "
                 "[--hover <widget>] [--voices none|changed|user] [--sheet <name>] [--theme light|dark] [--dpi N] out.png\n");
    return 2;
}

// How many different colours the picture has (premultiplied BGRA values); capped by the number of pixels.
size_t DistinctColours(const std::vector<uint32_t>& px) {
    std::set<uint32_t> seen;
    for (uint32_t p : px) {
        seen.insert(p);
        if (seen.size() >= 4096) break;  // far more than "not plain"; no need to count on
    }
    return seen.size();
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    std::wstring page = L"gallery", out;
    bool dark = false;
    int dpi = 96;
    shot::ShotOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::wstring a = argv[i];
        auto next = [&]() -> std::wstring { return i + 1 < argc ? std::wstring(argv[++i]) : std::wstring(); };
        if (a == L"--page") page = next();
        else if (a == L"--theme") dark = next() == L"dark";
        else if (a == L"--on") options.on = true;
        else if (a == L"--hover") options.hover = WideToUtf8(next());
        else if (a == L"--voices") options.voices = WideToUtf8(next());
        else if (a == L"--sheet") options.sheet = WideToUtf8(next());
        else if (a == L"--dpi") dpi = _wtoi(next().c_str());
        else if (a.rfind(L"--", 0) == 0) return Usage();
        else out = a;
    }
    if (out.empty() || dpi < 48 || dpi > 480) return Usage();

    // The log goes to the console of the caller too; the tool is run from a terminal or from ctest.
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    int rc = 1;
    {
        const float scale = static_cast<float>(dpi) / 96.0f;
        const int widthPx = static_cast<int>(kWindowW * scale + 0.5f), heightPx = static_cast<int>(kWindowH * scale + 0.5f);
        nui::D2dCanvas canvas;
        nui::Screen screen;
        screen.SetSize({kWindowW, kWindowH});
        // No transitions: every forced state shows at its end value.
        screen.SetReducedMotion(true);
        const nui::Palette& palette = nui::PaletteFor(dark);

        if (!canvas.CreateOffscreen(widthPx, heightPx, static_cast<float>(dpi))) {
            std::fprintf(stderr, "The Direct2D canvas could not be made.\n");
        } else if (!shot::BuildShotPage(WideToUtf8(page), screen, canvas, dark, options)) {
            std::fprintf(stderr, "There is no page named %s.\n", WideToUtf8(page).c_str());
            rc = 2;
        } else {
            screen.Tick(0.0);
            std::vector<uint32_t> pixels;
            if (canvas.BeginFrame()) {
                canvas.Clear(palette.paper);
                screen.Paint(canvas, palette);
                canvas.EndFrame();
            }
            if (!canvas.ReadPixels(pixels)) {
                std::fprintf(stderr, "The picture could not be drawn.\n");
            } else if (!canvas.SavePng(out)) {
                std::fprintf(stderr, "The picture could not be written.\n");
            } else {
                std::printf("settingsshot: wrote %s %dx%d colours=%zu\n", WideToUtf8(out).c_str(), widthPx, heightPx,
                            DistinctColours(pixels));
                rc = 0;
            }
        }
    }
    if (SUCCEEDED(com)) CoUninitialize();
    return rc;
}
