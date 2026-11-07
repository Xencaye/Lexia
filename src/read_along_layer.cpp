#include "read_along_layer.h"

#include <algorithm>
#include <vector>

#include "control_bar_layout.h"  // ScaleForDpi
#include "log.h"

namespace {

const wchar_t kLayerClass[] = L"LexiaReadAlongLayer";
const UINT_PTR kTimerId = 1;
// The same pace as the Overlay's read-along bar (overlay.cpp): well under the length of a spoken word, and
// only a changed bar is drawn.
const UINT kPollMs = 30;

}  // namespace

struct ReadAlongLayer::Impl {
    ReadAlongLayerConfig cfg;
    HWND hwnd = nullptr;
    HDC memDc = nullptr;
    HBITMAP dib = nullptr;
    HGDIOBJ dibOld = nullptr;
    uint32_t* bits = nullptr;  // w * h premultiplied BGRA pixels, top-down, all 0 = transparent
    int w = 0, h = 0;
    int thickness = 3, gap = 1;       // px, scaled for the monitor's DPI (as the Overlay's bars)
    uint32_t barPixel = 0xFF000000u;  // the colour as an opaque premultiplied pixel
    std::vector<Rect> drawn;          // the bars that are in the picture now (clipped, layer coordinates)
    bool ended = false;

    static LRESULT CALLBACK Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    bool Make(ReadAlongLayerConfig config);
    void Poll();
    void Draw(const std::vector<Rect>& bars);
    void Present(const RECT* dirty);
    void End();
    void Release();
};

// The window procedure. It never takes focus and never takes a click: see the header. `self` is null
// while the window is being made and after Release took it away.
LRESULT CALLBACK ReadAlongLayer::Impl::Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    Impl* self = reinterpret_cast<Impl*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;  // even if a click got here, it would not activate the layer (ADR-0001)
    case WM_NCHITTEST:
        return HTTRANSPARENT;  // the mouse belongs to whatever is under the layer
    case WM_ERASEBKGND:
        return 1;  // a layered window is drawn by UpdateLayeredWindow only
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);  // validates the area; the picture is not drawn here
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_TIMER:
        if (self && wp == kTimerId) {
            self->Poll();
            return 0;
        }
        break;
    case WM_DISPLAYCHANGE:
        // The monitor's size or place may have changed, so the layer no longer fits it: end it (the next
        // Session draws a new one on the right area). The App deletes it after the posted message.
        if (self) self->End();
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// Makes the window, the picture and the timer; false with everything undone if any step fails.
bool ReadAlongLayer::Impl::Make(ReadAlongLayerConfig config) {
    cfg = std::move(config);
    w = cfg.monitor.right - cfg.monitor.left;
    h = cfg.monitor.bottom - cfg.monitor.top;
    if (w <= 0 || h <= 0) return false;

    HINSTANCE inst = GetModuleHandleW(nullptr);
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.hInstance = inst;
        wc.lpfnWndProc = Proc;
        wc.lpszClassName = kLayerClass;  // no cursor, no brush: it paints nothing but the bars
        if (!RegisterClassExW(&wc)) {
            LogError("[readalong] Could not register the window class.");
            return false;
        }
        registered = true;
    }

    // Layered + transparent + no-activate + topmost, never in the taskbar or Alt+Tab (tool window).
    hwnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
                           kLayerClass, cfg.title.c_str(), WS_POPUP, cfg.monitor.left, cfg.monitor.top, w, h,
                           nullptr, nullptr, inst, nullptr);
    if (!hwnd) {
        LogError("[readalong] Could not create the window (", GetLastError(), ").");
        return false;
    }
    const int dpi = std::max(96, static_cast<int>(GetDpiForWindow(hwnd)));
    thickness = ScaleForDpi(3, dpi);
    gap = ScaleForDpi(1, dpi);
    barPixel = 0xFF000000u | (static_cast<uint32_t>(GetRValue(cfg.color)) << 16) |
               (static_cast<uint32_t>(GetGValue(cfg.color)) << 8) | static_cast<uint32_t>(GetBValue(cfg.color));

    HDC screen = GetDC(nullptr);
    memDc = CreateCompatibleDC(screen);
    ReleaseDC(nullptr, screen);
    if (memDc) {
        BITMAPINFO bi = {};
        bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
        bi.bmiHeader.biWidth = w;
        bi.bmiHeader.biHeight = -h;  // top-down: row 0 is the top of the screen
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        void* raw = nullptr;
        dib = CreateDIBSection(memDc, &bi, DIB_RGB_COLORS, &raw, nullptr, 0);  // zero-filled: transparent
        bits = static_cast<uint32_t*>(raw);
        if (dib && bits) dibOld = SelectObject(memDc, dib);
    }
    if (!dibOld) {
        LogError("[readalong] Could not make the picture.");
        Release();
        return false;
    }
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    // The first picture is all transparent; the window is shown without activation only after that.
    Present(nullptr);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    // A failed SetTimer would leave a layer that never ends: do not keep it.
    if (SetTimer(hwnd, kTimerId, kPollMs, nullptr) == 0) {
        LogError("[readalong] Could not start the timer.");
        Release();
        return false;
    }
    Poll();  // the first bars now, not 30 ms later
    return true;
}

// One tick: ends the layer when none of its Readings is left, else moves the bars to the heard word.
void ReadAlongLayer::Impl::Poll() {
    if (ended) return;
    if (!cfg.track.AnyActive(cfg.isActive)) {
        End();
        return;
    }
    const std::optional<HeardWord> heard = cfg.heardWord ? cfg.heardWord() : std::nullopt;
    std::vector<Rect> bars;
    for (const Rect& bar : cfg.track.Bars(heard, thickness, gap)) {
        // The layer's coordinates are the Shot's, and a word at the edge may reach past it.
        const Rect clipped = ClipToArea(bar, w, h);
        if (clipped.right > clipped.left && clipped.bottom > clipped.top) bars.push_back(clipped);
    }
    if (SameRects(bars, drawn)) return;  // a word lasts much longer than a tick
    Draw(bars);
}

// Replaces the bars in the picture: the old ones are made transparent again, the new ones drawn, and only
// the changed area (the old and the new bars together) is handed to the system.
void ReadAlongLayer::Impl::Draw(const std::vector<Rect>& bars) {
    if (!bits) return;
    RECT dirty = {w, h, 0, 0};  // grows to hold every rectangle touched
    auto touch = [&](const Rect& r) {
        dirty.left = std::min<LONG>(dirty.left, r.left);
        dirty.top = std::min<LONG>(dirty.top, r.top);
        dirty.right = std::max<LONG>(dirty.right, r.right);
        dirty.bottom = std::max<LONG>(dirty.bottom, r.bottom);
    };
    auto fill = [&](const Rect& r, uint32_t pixel) {
        for (int y = r.top; y < r.bottom; ++y) {
            std::fill(bits + static_cast<size_t>(y) * w + r.left, bits + static_cast<size_t>(y) * w + r.right, pixel);
        }
    };
    for (const Rect& r : drawn) {  // already clipped to the layer when they were drawn
        fill(r, 0);
        touch(r);
    }
    for (const Rect& r : bars) {
        fill(r, barPixel);
        touch(r);
    }
    drawn = bars;
    if (dirty.right > dirty.left && dirty.bottom > dirty.top) Present(&dirty);
}

// Shows the picture (the whole of it, or only the `dirty` area) with per-pixel alpha.
void ReadAlongLayer::Impl::Present(const RECT* dirty) {
    POINT origin = {cfg.monitor.left, cfg.monitor.top};
    SIZE size = {w, h};
    POINT src = {0, 0};
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UPDATELAYEREDWINDOWINFO info = {};
    info.cbSize = sizeof(info);
    info.pptDst = &origin;
    info.psize = &size;
    info.hdcSrc = memDc;
    info.pptSrc = &src;
    info.pblend = &blend;
    info.dwFlags = ULW_ALPHA;
    info.prcDirty = dirty;
    UpdateLayeredWindowIndirect(hwnd, &info);
}

// The Readings are done (or the display changed): nothing more is drawn. The window is hidden here and
// destroyed later by the owner (never inside this procedure).
void ReadAlongLayer::Impl::End() {
    if (ended) return;
    ended = true;
    KillTimer(hwnd, kTimerId);
    ShowWindow(hwnd, SW_HIDE);
    std::function<void()> notify = std::move(cfg.onEnded);
    cfg.onEnded = nullptr;
    if (notify) notify();
}

// Takes the window, the picture and the timer away. Safe when only some of them were made.
void ReadAlongLayer::Impl::Release() {
    ended = true;
    if (hwnd) {
        KillTimer(hwnd, kTimerId);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);  // a message from now on finds no Impl
        ShowWindow(hwnd, SW_HIDE);
        DestroyWindow(hwnd);
        hwnd = nullptr;
    }
    if (memDc && dibOld) SelectObject(memDc, dibOld);
    dibOld = nullptr;
    if (dib) DeleteObject(dib);
    dib = nullptr;
    bits = nullptr;
    if (memDc) DeleteDC(memDc);
    memDc = nullptr;
}

ReadAlongLayer::ReadAlongLayer() : impl_(std::make_unique<Impl>()) {}

ReadAlongLayer::~ReadAlongLayer() { Destroy(); }

bool ReadAlongLayer::Create(ReadAlongLayerConfig cfg) {
    Destroy();
    impl_->ended = false;
    impl_->drawn.clear();
    return impl_->Make(std::move(cfg));
}

void ReadAlongLayer::Destroy() {
    if (impl_) impl_->Release();
}

bool ReadAlongLayer::Ended() const { return !impl_ || impl_->ended; }

HWND ReadAlongLayer::Handle() const { return impl_ ? impl_->hwnd : nullptr; }
