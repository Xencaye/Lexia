#include "controlbar.h"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <filesystem>
#include <vector>

// GDI+ takes min / max from the global namespace.
using std::max;
using std::min;
#include <gdiplus.h>

#include "common.h"
#include "log.h"
#include "mouse_role.h"
#include "svg_icon.h"
#include "win32_theme.h"
#include "strings/string_ids.h"
#include "strings/strings.h"

namespace fs = std::filesystem;
namespace gp = Gdiplus;

namespace {

// ------------------------------------------------------------------ colours ---
// The Control bar's colours: the palette of the design (wireframe/palette_and_buttons.png and the
// control_bar_*.png), as 0xRRGGBB. A tile has a state colour; the icon on it is "glyph"; the Help and
// size icons have no tile and take the accent colours.

struct Palette {
    uint32_t paper;                          // the bar
    uint32_t tile, tileHot, tilePressed;     // Pause, Skip, Stop
    uint32_t tileOff;                        // ... while they do not work (greyed)
    uint32_t glyph, glyphOff;                // the icon on a tile
    uint32_t word, wordOff;                  // the word beside a tile (Large)
    uint32_t dots;                           // the grip and the divider
    uint32_t accent, accentHot, accentPressed;  // the Help and size icons
    uint32_t tipBg, tipText;                 // the tooltip
};

// 0xRRGGBB (the palette's form) and an alpha -> a GDI+ colour.
gp::Color Rgb(uint32_t rgb, BYTE a = 255) {
    return gp::Color(a, static_cast<BYTE>(rgb >> 16), static_cast<BYTE>(rgb >> 8), static_cast<BYTE>(rgb));
}

// The colours of the bar in the light or the dark theme.
Palette PaletteFor(bool dark) {
    Palette p;
    if (dark) {
        // TODO(owner): final dark palette. This is the temporary one of the same roles (D8 spec).
        p.paper = 0x2A2420;
        p.tile = 0x8F7A6B;
        p.tileHot = 0xA28D7D;
        p.tilePressed = 0xB5A090;
        p.tileOff = 0x3E352F;
        p.glyph = 0x2A2420;
        p.glyphOff = 0x2A2420;
        p.word = 0xE8DED3;
        p.wordOff = 0x574C44;
        p.dots = 0x574C44;
        p.accent = 0xC9A79E;
        p.accentHot = 0xE0C4BC;
        p.accentPressed = 0xEED8D2;
        p.tipBg = 0xE8DED3;
        p.tipText = 0x2A2420;
    } else {
        p.paper = 0xF5EFE8;
        p.tile = 0xBEA991;
        p.tileHot = 0xAB9883;       // a darker tone of the tile
        p.tilePressed = 0x988774;   // darker again
        p.tileOff = 0xD9CFC4;       // the design's "line" colour
        p.glyph = 0xF5EFE8;
        p.glyphOff = 0xF5EFE8;
        p.word = 0x6D6154;          // the design's ink
        p.wordOff = 0xD9CFC4;
        p.dots = 0xD9CFC4;
        p.accent = 0x6E504A;
        p.accentHot = 0x4F3733;
        p.accentPressed = 0x3A2825;
        p.tipBg = 0x6D6154;
        p.tipText = 0xF5EFE8;
    }
    return p;
}

// "system" follows Windows' app mode (the same setting the Settings window's title bar follows).
bool UseDark(const std::string& theme) {
    if (theme == "dark") return true;
    if (theme == "light") return false;
    return !SystemUsesLightTheme();
}

// ------------------------------------------------------------ GDI+ lifetime ---

void EnsureGdiplus() {
    struct Starter {
        ULONG_PTR token = 0;
        Starter() {
            gp::GdiplusStartupInput in;
            gp::GdiplusStartup(&token, &in, nullptr);
        }
        ~Starter() {
            if (token) gp::GdiplusShutdown(token);
        }
    };
    static Starter starter;
}

// ------------------------------------------------------------------- pixels ---
// A premultiplied BGRA picture (what a layered window wants). GDI+ draws into it through a Bitmap
// that wraps the same memory; the shadow is made here, because GDI+ cannot blur; the icons are
// blended in directly.

struct Pixmap {
    int w = 0, h = 0;
    std::vector<uint32_t> px;

    void Resize(int width, int height) {
        w = width;
        h = height;
        px.assign(static_cast<size_t>(w) * static_cast<size_t>(h), 0);
    }
    void Clear() { std::fill(px.begin(), px.end(), 0u); }
};

// One box blur pass along a line of n values `stride` apart; edges repeat.
void BoxLine(const uint8_t* in, uint8_t* out, int n, size_t stride, int r) {
    const int win = 2 * r + 1;
    auto at = [&](int i) { return static_cast<int>(in[static_cast<size_t>(std::clamp(i, 0, n - 1)) * stride]); };
    int sum = 0;
    for (int i = -r; i <= r; ++i) sum += at(i);
    for (int i = 0; i < n; ++i) {
        out[static_cast<size_t>(i) * stride] = static_cast<uint8_t>((sum + win / 2) / win);
        sum += at(i + r + 1) - at(i - r);
    }
}

// A gaussian blur of sigma `sigma` px, as three box passes each way.
void Blur(std::vector<uint8_t>& a, int w, int h, double sigma) {
    if (sigma < 0.5 || w <= 0 || h <= 0) return;
    const int r = std::max(1, static_cast<int>(std::lround((std::sqrt(4.0 * sigma * sigma + 1.0) - 1.0) / 2.0)));
    std::vector<uint8_t> tmp(a.size());
    for (int pass = 0; pass < 3; ++pass) {
        for (int y = 0; y < h; ++y) {
            BoxLine(a.data() + static_cast<size_t>(y) * static_cast<size_t>(w), tmp.data() + static_cast<size_t>(y) * static_cast<size_t>(w), w, 1, r);
        }
        for (int x = 0; x < w; ++x) {
            BoxLine(tmp.data() + x, a.data() + x, h, static_cast<size_t>(w), r);
        }
    }
}

// `colour` at coverage `cover` (0..255) over the pixel, premultiplied.
void Over(uint32_t& dst, const gp::Color& colour, int cover) {
    const int sa = colour.GetA() * cover / 255;
    if (sa <= 0) return;
    const uint32_t d = dst;
    const int inv = 255 - sa;
    auto ch = [&](int c, int shift) {
        const int s = c * sa / 255;
        const int dv = static_cast<int>((d >> shift) & 0xFF);
        return std::min(255, s + dv * inv / 255);
    };
    const int a = std::min(255, sa + static_cast<int>(d >> 24) * inv / 255);
    dst = (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(ch(colour.GetR(), 16)) << 16) |
          (static_cast<uint32_t>(ch(colour.GetG(), 8)) << 8) | static_cast<uint32_t>(ch(colour.GetB(), 0));
}

// A premultiplied picture over the Pixmap with its top left corner at (x, y).
void BlendPremultiplied(Pixmap& dst, int x, int y, const IconPixels& icon) {
    for (int j = 0; j < icon.size; ++j) {
        const int py = y + j;
        if (py < 0 || py >= dst.h) continue;
        for (int i = 0; i < icon.size; ++i) {
            const int px = x + i;
            if (px < 0 || px >= dst.w) continue;
            const uint32_t s = icon.px[static_cast<size_t>(j) * static_cast<size_t>(icon.size) + static_cast<size_t>(i)];
            const int sa = static_cast<int>(s >> 24);
            if (sa == 0) continue;
            uint32_t& d = dst.px[static_cast<size_t>(py) * static_cast<size_t>(dst.w) + static_cast<size_t>(px)];
            const int inv = 255 - sa;
            auto ch = [&](int shift) {
                const int sv = static_cast<int>((s >> shift) & 0xFF);
                const int dv = static_cast<int>((d >> shift) & 0xFF);
                return static_cast<uint32_t>(std::min(255, sv + dv * inv / 255));
            };
            d = (static_cast<uint32_t>(std::min(255, sa + static_cast<int>(d >> 24) * inv / 255)) << 24) | (ch(16) << 16) |
                (ch(8) << 8) | ch(0);
        }
    }
}

// A rectangle with rounded corners as a GDI+ path, for filling and for the shadow. The radius is at
// most half the shorter side; 0 gives a plain rectangle.
std::unique_ptr<gp::GraphicsPath> RoundedPath(const gp::RectF& r, float radius) {
    auto path = std::make_unique<gp::GraphicsPath>();
    const float rad = std::min(radius, std::min(r.Width, r.Height) / 2.0f);
    const float d = rad * 2.0f;
    if (rad <= 0.0f) {
        path->AddRectangle(r);
    } else {
        path->AddArc(r.X, r.Y, d, d, 180, 90);
        path->AddArc(r.X + r.Width - d, r.Y, d, d, 270, 90);
        path->AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0, 90);
        path->AddArc(r.X, r.Y + r.Height - d, d, d, 90, 90);
        path->CloseFigure();
    }
    return path;
}

// The shape's coverage in a region [rx, rx+rw) x [ry, ry+rh) of the picture, 0..255.
std::vector<uint8_t> ShapeMask(const gp::RectF& shape, float radius, int rx, int ry, int rw, int rh) {
    Pixmap tmp;
    tmp.Resize(rw, rh);
    {
        gp::Bitmap bmp(rw, rh, rw * 4, PixelFormat32bppPARGB, reinterpret_cast<BYTE*>(tmp.px.data()));
        gp::Graphics g(&bmp);
        g.SetSmoothingMode(gp::SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(gp::PixelOffsetModeHalf);
        gp::SolidBrush white(gp::Color(255, 255, 255, 255));
        const auto path = RoundedPath(
            gp::RectF(shape.X - static_cast<float>(rx), shape.Y - static_cast<float>(ry), shape.Width, shape.Height), radius);
        g.FillPath(&white, path.get());
        g.Flush(gp::FlushIntentionSync);
    }
    std::vector<uint8_t> m(tmp.px.size());
    for (size_t i = 0; i < m.size(); ++i) m[i] = static_cast<uint8_t>(tmp.px[i] >> 24);
    return m;
}

// A blurred copy of the shape, moved by (dx, dy), under whatever is there (CSS box-shadow: dx dy blur colour).
void DropShadow(Pixmap& dst, const gp::RectF& shape, float radius, double dx, double dy, double blur,
                const gp::Color& colour) {
    const double sigma = blur / 2.0;
    const int pad = static_cast<int>(std::ceil(sigma * 3.0)) + 2;
    const int rx = static_cast<int>(std::floor(shape.X + dx)) - pad;
    const int ry = static_cast<int>(std::floor(shape.Y + dy)) - pad;
    const int rw = static_cast<int>(std::ceil(shape.Width)) + 2 * pad + 2;
    const int rh = static_cast<int>(std::ceil(shape.Height)) + 2 * pad + 2;
    gp::RectF moved(shape.X + static_cast<float>(dx), shape.Y + static_cast<float>(dy), shape.Width, shape.Height);
    std::vector<uint8_t> m = ShapeMask(moved, radius, rx, ry, rw, rh);
    Blur(m, rw, rh, sigma);
    for (int y = 0; y < rh; ++y) {
        const int py = ry + y;
        if (py < 0 || py >= dst.h) continue;
        for (int x = 0; x < rw; ++x) {
            const int px = rx + x;
            if (px < 0 || px >= dst.w) continue;
            const uint8_t cover = m[static_cast<size_t>(y) * static_cast<size_t>(rw) + static_cast<size_t>(x)];
            if (cover) Over(dst.px[static_cast<size_t>(py) * static_cast<size_t>(dst.w) + static_cast<size_t>(px)], colour, cover);
        }
    }
}

// A layout Rect (bar coordinates) moved by (ox, oy) into the window picture, which has the shadow
// margin around the bar.
gp::RectF ToRectF(const Rect& r, int ox, int oy) {
    return gp::RectF(static_cast<float>(r.left + ox), static_cast<float>(r.top + oy),
                     static_cast<float>(r.right - r.left), static_cast<float>(r.bottom - r.top));
}

// -------------------------------------------------------------------- icons ---

// The SVG file (without its folder and extension) a button shows now.
const char* IconName(BarButton b, bool paused, BarSize size) {
    switch (b) {
    case BarButton::Pause: return paused ? "play-03" : "pause-03";
    case BarButton::Skip: return "fast-forward";
    case BarButton::Stop: return "stop-01";
    case BarButton::Help: return "help-circle-contained";
    case BarButton::Size: return size == BarSize::Large ? "minimise-01" : "maximise-02";
    default: return "";
    }
}

// <exe>\ui\icons in a package; a dev build (build\Release\lexia.exe) has no ui\ beside it, so look
// in the folders above too, up to the source tree's ui\icons. Empty if there is none.
fs::path FindIconsDir() {
    wchar_t buf[MAX_PATH * 2] = {};
    GetModuleFileNameW(nullptr, buf, static_cast<DWORD>(std::size(buf)));
    fs::path dir = fs::path(buf).parent_path();
    for (int up = 0; up < 6 && !dir.empty(); ++up) {
        std::error_code ec;
        const fs::path candidate = dir / L"ui" / L"icons";
        if (fs::is_directory(candidate, ec)) return candidate;
        const fs::path parent = dir.parent_path();
        if (parent == dir) break;
        dir = parent;
    }
    return {};
}

// A tooltip that is only a word starts with a capital (the labels on the bar are lower case).
std::wstring Capitalised(std::wstring s) {
    if (!s.empty()) s[0] = static_cast<wchar_t>(std::towupper(s[0]));
    return s;
}

// The GDI+ image encoder for a MIME type ("image/png"), for SaveSnapshot.
bool EncoderFor(const wchar_t* mime, CLSID* clsid) {
    UINT count = 0, bytes = 0;
    gp::GetImageEncodersSize(&count, &bytes);
    if (bytes == 0) return false;
    std::vector<BYTE> buf(bytes);
    auto* info = reinterpret_cast<gp::ImageCodecInfo*>(buf.data());
    gp::GetImageEncoders(count, bytes, info);
    for (UINT i = 0; i < count; ++i) {
        if (std::wcscmp(info[i].MimeType, mime) == 0) {
            *clsid = info[i].Clsid;
            return true;
        }
    }
    return false;
}

}  // namespace

// ============================================================================ the bar ===

struct ControlBar::Impl {
    ControlBarConfig cfg;
    HWND hwnd = nullptr, tip = nullptr;
    Palette pal = {};
    bool dark = false;
    int dpi = 96;
    int margin = 0;  // room round the bar for its shadow, in px

    BarState state;
    BarLayout layout;
    Point origin;  // the bar's top left corner on the screen (not the window's)
    bool relayout = true;

    // Pointer
    BarButton hot = BarButton::None;
    bool inside = false;
    BarButton pressed = BarButton::None;
    bool dragging = false;
    bool dragMoved = false;
    POINT grab = {};  // cursor minus bar origin when a drag began
    BarButton tipFor = BarButton::None;

    // Drawing
    Pixmap work, shadows;
    int shadowW = -1, shadowH = -1;
    HDC memDc = nullptr;
    HBITMAP dib = nullptr;
    HGDIOBJ dibOld = nullptr;
    uint32_t* dibBits = nullptr;
    int winW = 0, winH = 0;
    std::unique_ptr<gp::FontFamily> family;
    SvgIconCache svgIcons;
    fs::path iconsDir;

    // Words, in the Interface language
    std::wstring label[3], tipText[3], resume, resumeTip, helpTip, sizeTipSmall, sizeTipLarge;
    // Large only: the measured widths of the two words the Pause button can show. The button is as
    // wide as the word it shows now, so the gap after it is the same 18 px as after the others.
    int pauseWordW = 0, resumeWordW = 0;

    static const UINT_PTR kPollTimer = 1;
    static const UINT_PTR kTipTimer = 2;

    static LRESULT CALLBACK BarProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    static LRESULT CALLBACK TipProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    bool Create(const ControlBarConfig& config);
    void Destroy();
    int Scale(int v) const { return ScaleForDpi(v, dpi); }
    // The Pause button's word width for the word it shows now (Resume while paused).
    void SetPauseWordWidth() {
        state.labelWidth[static_cast<size_t>(BarButton::Pause)] = state.paused ? resumeWordW : pauseWordW;
    }
    std::wstring W(const char* id) const { return Utf8ToWide(strings::Text(cfg.interfaceLanguage, id)); }
    std::wstring Tip(const char* tipId, const std::string& key, const std::wstring& word) const {
        if (key.empty()) return Capitalised(word);
        return Utf8ToWide(strings::Format(cfg.interfaceLanguage, tipId, {{"key", key}}));
    }
    void LoadWords();
    float MeasureWidth(const std::wstring& text, float px) const;
    void Layout();         // layout, window size, cached shadow
    void Paint();          // the picture, then UpdateLayeredWindow
    void Present();
    void PaintBarBody(gp::Graphics& g);
    void PaintItems(gp::Graphics& g);
    void PaintTileButton(gp::Graphics& g, const BarItem& it);
    void PaintIconButton(gp::Graphics& g, const BarItem& it);
    void PaintDivider(gp::Graphics& g, const BarItem& it);
    bool DrawIcon(gp::Graphics& g, BarButton b, const Rect& box, uint32_t rgb);
    void DrawWord(gp::Graphics& g, const std::wstring& text, const gp::RectF& box, uint32_t rgb, bool centred);
    POINT ScreenOfWindowOrigin() const { return POINT{origin.x - margin, origin.y - margin}; }
    void MoveWindowToOrigin();
    Point ClientToBar(int x, int y) const { return Point{x - margin, y - margin}; }
    void OnMouseMove(int x, int y);
    void OnLButtonDown(int x, int y);
    void OnLButtonUp(int x, int y);
    void OnLeave();
    void ArmTip();
    void ShowTip();
    void HideTip();
    std::wstring TipTextFor(BarButton b) const;
    const std::wstring& WordFor(BarButton b) const;
    void ToggleSize();
    void Changed();
};

// The bar window's procedure. The bar never takes focus (ADR-0001), and outside the bar's own shape
// (the shadow) it lets the mouse through to the Overlay below, so the shadow never eats a Pick.
LRESULT CALLBACK ControlBar::Impl::BarProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    Impl* self = reinterpret_cast<Impl*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!self) return DefWindowProcW(hwnd, msg, wp, lp);
    switch (msg) {
    case WM_MOUSEACTIVATE:  // ADR-0001: a click on the bar must not take focus from the game
        return MA_NOACTIVATE;
    case WM_NCHITTEST: {
        POINT p = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        ScreenToClient(hwnd, &p);
        // The shadow area is part of the window but not of the bar: clicks there go to the Overlay.
        if (HitTest(self->layout, self->ClientToBar(p.x, p.y)).kind == BarItemKind::None) return HTTRANSPARENT;
        return HTCLIENT;
    }
    case WM_SETCURSOR: {
        if (LOWORD(lp) != HTCLIENT) break;
        POINT p;
        GetCursorPos(&p);
        ScreenToClient(hwnd, &p);
        const BarHit hit = HitTest(self->layout, self->ClientToBar(p.x, p.y));
        LPCWSTR cur = IDC_ARROW;
        if (self->dragging) {
            cur = IDC_SIZEALL;
        } else if (hit.kind == BarItemKind::Button) {
            cur = hit.enabled ? IDC_HAND : IDC_ARROW;
        } else if (hit.kind == BarItemKind::Grip || hit.kind == BarItemKind::Background) {
            cur = IDC_SIZEALL;
        }
        SetCursor(LoadCursorW(nullptr, cur));
        return TRUE;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);  // a layered window is drawn by UpdateLayeredWindow, not here
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_MOUSEMOVE:
        self->OnMouseMove(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN: {
        // The Pick button presses the bar's buttons and drags it; the Leave button is the Overlay's
        // "leave", not the bar's: swallow it (mouse_role.h; left-handed swaps the two).
        const MouseButton b = msg == WM_LBUTTONDOWN ? MouseButton::Left : MouseButton::Right;
        if (RoleOf(b, self->cfg.leftHanded) == MouseRole::Pick) self->OnLButtonDown(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    }
    case WM_LBUTTONUP:
    case WM_RBUTTONUP: {
        const MouseButton b = msg == WM_LBUTTONUP ? MouseButton::Left : MouseButton::Right;
        if (RoleOf(b, self->cfg.leftHanded) == MouseRole::Pick) self->OnLButtonUp(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    }
    case WM_CAPTURECHANGED:
        if (self->dragging || self->pressed != BarButton::None) {
            self->dragging = false;
            self->pressed = BarButton::None;
            self->Paint();
        }
        return 0;
    case WM_MOUSELEAVE:
        self->OnLeave();
        return 0;
    case WM_TIMER:
        if (wp == kPollTimer) {
            const SpeechState sp = self->cfg.speechState ? self->cfg.speechState() : SpeechState{};
            const bool speaking = sp.active && !sp.paused;
            if (speaking != self->state.speaking || sp.paused != self->state.paused) {
                self->state.speaking = speaking;
                self->state.paused = sp.paused;
                self->Paint();
            }
        } else if (wp == kTipTimer) {
            KillTimer(hwnd, kTipTimer);
            self->ShowTip();
        }
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// The tooltip window's procedure: it takes no focus and no mouse, and paints its text.
LRESULT CALLBACK ControlBar::Impl::TipProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    Impl* self = reinterpret_cast<Impl*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_NCHITTEST:
        return HTTRANSPARENT;  // a tooltip is never under the mouse
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        if (self) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            const std::wstring text = self->TipTextFor(self->tipFor);
            const uint32_t bgc = self->pal.tipBg, fgc = self->pal.tipText;
            HBRUSH bg = CreateSolidBrush(RGB((bgc >> 16) & 0xFF, (bgc >> 8) & 0xFF, bgc & 0xFF));
            FillRect(dc, &rc, bg);
            DeleteObject(bg);
            HFONT font = CreateFontW(-self->Scale(15), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                     OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                     DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            const HGDIOBJ old = SelectObject(dc, font);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB((fgc >> 16) & 0xFF, (fgc >> 8) & 0xFF, fgc & 0xFF));
            DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            SelectObject(dc, old);
            DeleteObject(font);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// The bar's words and tooltips in the Interface language (strings/), read once per bar.
void ControlBar::Impl::LoadWords() {
    using namespace strings::id;
    label[static_cast<int>(BarButton::Pause)] = W(kControlBarPause);
    label[static_cast<int>(BarButton::Skip)] = W(kControlBarSkip);
    label[static_cast<int>(BarButton::Stop)] = W(kControlBarStop);
    resume = W(kControlBarResume);
    helpTip = W(kControlBarHelpTip);
    sizeTipSmall = W(kControlBarSizeSmall);
    sizeTipLarge = W(kControlBarSizeLarge);
    const std::string stopKey = cfg.speechKey.empty() ? std::string() : cfg.speechKey + " " + cfg.speechKey;  // tapped twice
    tipText[static_cast<int>(BarButton::Pause)] = Tip(kControlBarPauseTip, cfg.pauseKey, label[static_cast<int>(BarButton::Pause)]);
    resumeTip = Tip(kControlBarResumeTip, cfg.pauseKey, resume);
    tipText[static_cast<int>(BarButton::Skip)] = Tip(kControlBarSkipTip, cfg.speechKey, label[static_cast<int>(BarButton::Skip)]);
    tipText[static_cast<int>(BarButton::Stop)] = Tip(kControlBarStopTip, stopKey, label[static_cast<int>(BarButton::Stop)]);
}

// A button's tooltip now: Pause says Resume while paused; Size names the size it switches to.
std::wstring ControlBar::Impl::TipTextFor(BarButton b) const {
    switch (b) {
    case BarButton::Pause: return state.paused ? resumeTip : tipText[static_cast<int>(b)];
    case BarButton::Skip:
    case BarButton::Stop: return tipText[static_cast<int>(b)];
    case BarButton::Help: return helpTip;
    case BarButton::Size: return cfg.size == BarSize::Large ? sizeTipSmall : sizeTipLarge;
    default: return std::wstring();
    }
}

// The word on a Pause, Skip or Stop button now (Resume while paused).
const std::wstring& ControlBar::Impl::WordFor(BarButton b) const {
    if (b == BarButton::Pause && state.paused) return resume;
    return label[static_cast<size_t>(b)];
}

// The width in pixels of `text` in the bar's bold font at `px` pixels, so the Large layout makes room
// for words of any Interface language.
float ControlBar::Impl::MeasureWidth(const std::wstring& text, float px) const {
    gp::Bitmap bmp(1, 1, PixelFormat32bppPARGB);
    gp::Graphics g(&bmp);
    g.SetTextRenderingHint(gp::TextRenderingHintAntiAlias);
    gp::Font font(family.get(), px, gp::FontStyleBold, gp::UnitPixel);
    gp::RectF out;
    g.MeasureString(text.c_str(), static_cast<INT>(text.size()), &font, gp::PointF(0, 0),
                    gp::StringFormat::GenericTypographic(), &out);
    return out.Width;
}

// Makes the bar's window and its tooltip for one Session, lays it out for the monitor's DPI and puts
// it where it was last time (or the default place). False if a window cannot be made.
bool ControlBar::Impl::Create(const ControlBarConfig& config) {
    cfg = config;
    EnsureGdiplus();
    dark = UseDark(cfg.theme);
    pal = PaletteFor(dark);
    dpi = cfg.dpi > 0 ? cfg.dpi : (cfg.owner ? static_cast<int>(GetDpiForWindow(cfg.owner)) : 96);
    if (dpi <= 0) dpi = 96;
    margin = Scale(32);  // room for the soft shadow to fade

    // The design's words are bold and round; Atkinson Hyperlegible Next is a web font here (woff2),
    // GDI+ cannot load it, so Segoe UI Bold stands in.
    family = std::make_unique<gp::FontFamily>(L"Segoe UI");
    iconsDir = FindIconsDir();
    if (iconsDir.empty()) LogInfo("[controlbar] No ui\\icons folder found; the buttons show words only.");
    LoadWords();
    state.size = cfg.size;

    HINSTANCE inst = GetModuleHandleW(nullptr);
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.hInstance = inst;
        wc.lpfnWndProc = BarProc;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"LexiaControlBar";
        WNDCLASSEXW tc = wc;
        tc.lpfnWndProc = TipProc;
        tc.lpszClassName = L"LexiaControlBarTip";
        if (!RegisterClassExW(&wc) || !RegisterClassExW(&tc)) {
            LogError("[controlbar] Could not register the window classes.");
            return false;
        }
        registered = true;
    }

    // Never active, always above the Overlay (its owner), never in the taskbar or Alt+Tab.
    hwnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
                           L"LexiaControlBar", W(strings::id::kControlBarName).c_str(), WS_POPUP, 0, 0, 1, 1, cfg.owner, nullptr, inst, nullptr);
    if (!hwnd) {
        LogError("[controlbar] Could not create the window (", GetLastError(), ").");
        return false;
    }
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    tip = CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_TOOLWINDOW, L"LexiaControlBarTip", L"", WS_POPUP,
                          0, 0, 1, 1, hwnd, nullptr, inst, nullptr);
    if (tip) SetWindowLongPtrW(tip, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    HDC screen = GetDC(nullptr);
    memDc = CreateCompatibleDC(screen);
    ReleaseDC(nullptr, screen);

    const SpeechState sp = cfg.speechState ? cfg.speechState() : SpeechState{};
    state.speaking = sp.active && !sp.paused;
    state.paused = sp.paused;

    Layout();
    const Rect mon = {cfg.monitor.left, cfg.monitor.top, cfg.monitor.right, cfg.monitor.bottom};
    origin = InitialBarOrigin(cfg.savedX, cfg.savedY, mon, layout.width, layout.height, dpi);
    MoveWindowToOrigin();
    Paint();
    // Shown without activation: a game under the Overlay must stay the foreground window.
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    SetTimer(hwnd, kPollTimer, 83, nullptr);
    return true;
}

// Ends the bar: its timers, windows and picture buffers. Safe to call twice.
void ControlBar::Impl::Destroy() {
    if (tip) {
        DestroyWindow(tip);
        tip = nullptr;
    }
    if (hwnd) {
        KillTimer(hwnd, kPollTimer);
        KillTimer(hwnd, kTipTimer);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        DestroyWindow(hwnd);
        hwnd = nullptr;
    }
    if (memDc) {
        if (dibOld) SelectObject(memDc, dibOld);
        dibOld = nullptr;
        DeleteDC(memDc);
        memDc = nullptr;
    }
    if (dib) {
        DeleteObject(dib);
        dib = nullptr;
    }
}

// Moves the window so the bar (not the shadow margin) is at `origin`, without a repaint.
void ControlBar::Impl::MoveWindowToOrigin() {
    const POINT p = ScreenOfWindowOrigin();
    SetWindowPos(hwnd, nullptr, p.x, p.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

// The layout for the current state, and the window's picture buffers to match.
void ControlBar::Impl::Layout() {
    state.size = cfg.size;
    const bool large = cfg.size == BarSize::Large;
    const float wordPx = static_cast<float>(Scale(18));
    if (large) {
        auto px = [](float w) { return static_cast<int>(std::ceil(w)) + 1; };  // whole pixels, never cut off
        pauseWordW = px(MeasureWidth(label[0], wordPx));
        resumeWordW = px(MeasureWidth(resume, wordPx));
        SetPauseWordWidth();
        state.labelWidth[static_cast<size_t>(BarButton::Skip)] = px(MeasureWidth(label[1], wordPx));
        state.labelWidth[static_cast<size_t>(BarButton::Stop)] = px(MeasureWidth(label[2], wordPx));
    }
    relayout = false;
    layout = LayoutBar(state, dpi);
    winW = layout.width + 2 * margin;
    winH = layout.height + 2 * margin;

    work.Resize(winW, winH);
    shadows.Resize(winW, winH);
    shadowW = shadowH = -1;  // the cached shadow is for another size

    if (memDc) {
        if (dibOld) SelectObject(memDc, dibOld);
        if (dib) DeleteObject(dib);
        BITMAPINFO bi = {};
        bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
        bi.bmiHeader.biWidth = winW;
        bi.bmiHeader.biHeight = -winH;  // top-down
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        dib = CreateDIBSection(memDc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
        dibBits = static_cast<uint32_t*>(bits);
        dibOld = dib ? SelectObject(memDc, dib) : nullptr;
    }
}

// The icon of button `b` in `box`, in colour `rgb`, blended into the picture. False if there is no
// icon (no folder, no file, no Direct2D SVG): the caller shows a word instead.
bool ControlBar::Impl::DrawIcon(gp::Graphics& g, BarButton b, const Rect& box, uint32_t rgb) {
    if (iconsDir.empty()) return false;
    const fs::path file = iconsDir / (std::string(IconName(b, state.paused, cfg.size)) + ".svg");
    const IconPixels* icon = svgIcons.Get(file, box.right - box.left, rgb);
    if (!icon) return false;
    g.Flush(gp::FlushIntentionSync);  // GDI+ has finished with the picture; now we write into it
    BlendPremultiplied(work, box.left + margin, box.top + margin, *icon);
    return true;
}

// One word (a label, or the sign that stands in for a missing icon) in the bar's bold font, centred
// vertically in `box`.
void ControlBar::Impl::DrawWord(gp::Graphics& g, const std::wstring& text, const gp::RectF& box, uint32_t rgb,
                                bool centred) {
    gp::Font font(family.get(), static_cast<float>(Scale(18)), gp::FontStyleBold, gp::UnitPixel);
    gp::StringFormat sf(gp::StringFormat::GenericTypographic());
    sf.SetLineAlignment(gp::StringAlignmentCenter);
    sf.SetAlignment(centred ? gp::StringAlignmentCenter : gp::StringAlignmentNear);
    sf.SetFormatFlags(gp::StringFormatFlagsNoWrap | gp::StringFormatFlagsNoClip);
    gp::SolidBrush brush(Rgb(rgb));
    g.DrawString(text.c_str(), static_cast<INT>(text.size()), &font, box, &sf, &brush);
}

// The bar: a soft shadow, then the paper.
void ControlBar::Impl::PaintBarBody(gp::Graphics& g) {
    const gp::RectF body(static_cast<float>(margin), static_cast<float>(margin), static_cast<float>(layout.width),
                         static_cast<float>(layout.height));
    const float rad = static_cast<float>(Scale(10));
    const float s = static_cast<float>(dpi) / 96.0f;
    if (shadowW != winW || shadowH != winH) {
        shadows.Clear();
        DropShadow(shadows, body, rad, 0, 4 * s, 14 * s, gp::Color(static_cast<BYTE>(0.30 * 255), 0, 0, 0));
        DropShadow(shadows, body, rad, 0, 1 * s, 3 * s, gp::Color(static_cast<BYTE>(0.22 * 255), 0, 0, 0));
        shadowW = winW;
        shadowH = winH;
    }
    g.Flush(gp::FlushIntentionSync);
    std::memcpy(work.px.data(), shadows.px.data(), work.px.size() * sizeof(uint32_t));
    const auto path = RoundedPath(body, rad);
    gp::SolidBrush brush(Rgb(pal.paper));
    g.FillPath(&brush, path.get());
}

// Pause, Skip, Stop: a rounded tile with a light icon, and in Large a bold word beside it.
void ControlBar::Impl::PaintTileButton(gp::Graphics& g, const BarItem& it) {
    const bool large = cfg.size == BarSize::Large;
    const bool isPressed = it.enabled && pressed == it.button && hot == it.button && inside;
    const bool isHot = it.enabled && hot == it.button && inside && !isPressed;
    uint32_t tileColour = pal.tile;
    if (!it.enabled) tileColour = pal.tileOff;
    else if (isPressed) tileColour = pal.tilePressed;
    else if (isHot) tileColour = pal.tileHot;

    const gp::RectF r = ToRectF(it.tile, margin, margin);
    const auto path = RoundedPath(r, static_cast<float>(Scale(5)));
    gp::SolidBrush fill(Rgb(tileColour));
    g.FillPath(&fill, path.get());

    const int iconPx = Scale(24);
    const int dx = (it.tile.right - it.tile.left - iconPx) / 2, dy = (it.tile.bottom - it.tile.top - iconPx) / 2;
    const Rect box = {it.tile.left + dx, it.tile.top + dy, it.tile.left + dx + iconPx, it.tile.top + dy + iconPx};
    const bool haveIcon = DrawIcon(g, it.button, box, it.enabled ? pal.glyph : pal.glyphOff);

    const std::wstring& word = WordFor(it.button);
    const uint32_t wordColour = it.enabled ? pal.word : pal.wordOff;
    if (large) {
        DrawWord(g, word, ToRectF(it.word, margin, margin), wordColour, false);
    } else if (!haveIcon && !word.empty()) {
        DrawWord(g, std::wstring(1, static_cast<wchar_t>(std::towupper(word[0]))), r, it.enabled ? pal.glyph : pal.glyphOff, true);
    }
}

// Help and the size icon: just the icon, in the accent colour; a darker tone on hover and press.
void ControlBar::Impl::PaintIconButton(gp::Graphics& g, const BarItem& it) {
    const bool isPressed = it.enabled && pressed == it.button && hot == it.button && inside;
    const bool isHot = it.enabled && hot == it.button && inside && !isPressed;
    const uint32_t colour = isPressed ? pal.accentPressed : isHot ? pal.accentHot : pal.accent;
    if (DrawIcon(g, it.button, it.tile, colour)) return;
    // No icon: a plain sign instead, so the button is still there.
    const std::wstring sign = it.button == BarButton::Help ? L"?" : cfg.size == BarSize::Large ? L"-" : L"+";
    DrawWord(g, sign, ToRectF(it.tile, margin, margin), colour, true);
}

// A dotted 1 px line.
void ControlBar::Impl::PaintDivider(gp::Graphics& g, const BarItem& it) {
    // 1 px dots 3.5 px apart, smoothed where they fall between pixels (as in the design's picture).
    const float s = static_cast<float>(dpi) / 96.0f;
    const gp::RectF r = ToRectF(it.tile, margin, margin);
    gp::SolidBrush brush(Rgb(pal.dots));
    for (float y = r.Y; y + s <= r.Y + r.Height + 0.01f; y += 3.5f * s) {
        g.FillRectangle(&brush, r.X, y, r.Width, s);
    }
}

// Draws every item of the layout (grip, tiles, words, divider, Help and Size icons) on the bar.
void ControlBar::Impl::PaintItems(gp::Graphics& g) {
    const float s = static_cast<float>(dpi) / 96.0f;
    for (const BarItem& it : layout.items) {
        switch (it.kind) {
        case BarItemKind::Grip: {
            // 2 x 3 dots (rounded squares) of 4 px, 8 px apart across and 9 px down.
            const gp::RectF r = ToRectF(it.tile, margin, margin);
            gp::SolidBrush dot(Rgb(pal.dots));
            for (int row = 0; row < 3; ++row) {
                for (int col = 0; col < 2; ++col) {
                    const auto path = RoundedPath(gp::RectF(r.X + static_cast<float>(col) * 8.0f * s,
                                                            r.Y + static_cast<float>(row) * 9.0f * s, 4.0f * s, 4.0f * s),
                                                  1.2f * s);
                    g.FillPath(&dot, path.get());
                }
            }
            break;
        }
        case BarItemKind::Divider:
            PaintDivider(g, it);
            break;
        case BarItemKind::Button:
            if (it.button == BarButton::Help || it.button == BarButton::Size) {
                PaintIconButton(g, it);
            } else {
                PaintTileButton(g, it);
            }
            break;
        default:
            break;
        }
        g.Flush(gp::FlushIntentionSync);
    }
}

// Draws the whole bar into its buffer (shadow, body, items) and shows it with UpdateLayeredWindow:
// a layered window has no WM_PAINT, its picture is replaced whole. Lays the bar out again first
// when its size changed (the size switch, or Pause <-> Resume).
void ControlBar::Impl::Paint() {
    if (!hwnd) return;
    if (cfg.size == BarSize::Large) SetPauseWordWidth();  // Pause <-> Resume changes the bar's width
    BarLayout fresh = LayoutBar(state, dpi);
    if (fresh.width != layout.width || fresh.height != layout.height || relayout) {
        const int oldW = layout.width;
        const bool sizeSwitch = relayout;
        Layout();
        const Rect mon = {cfg.monitor.left, cfg.monitor.top, cfg.monitor.right, cfg.monitor.bottom};
        // The size switch keeps the bar's centre. A new word on Pause keeps the left edge, so the
        // Pause button stays under the mouse that just clicked it; the buttons after it move.
        if (oldW > 0) {
            origin = sizeSwitch ? OriginKeepingCentre(origin, oldW, layout.width, layout.height, mon)
                                : ClampBarOrigin(origin, layout.width, layout.height, mon);
        }
        MoveWindowToOrigin();
    } else {
        layout = std::move(fresh);  // same size, but which buttons work may differ
    }
    if (!dibBits) return;

    work.Clear();
    {
        gp::Bitmap bmp(winW, winH, winW * 4, PixelFormat32bppPARGB, reinterpret_cast<BYTE*>(work.px.data()));
        gp::Graphics g(&bmp);
        g.SetSmoothingMode(gp::SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(gp::PixelOffsetModeHalf);
        g.SetInterpolationMode(gp::InterpolationModeHighQualityBicubic);
        g.SetTextRenderingHint(gp::TextRenderingHintAntiAlias);
        PaintBarBody(g);  // copies the cached shadow into `work`, then fills the paper
        PaintItems(g);
        g.Flush(gp::FlushIntentionSync);
    }
    Present();
}

// The finished picture to the screen.
void ControlBar::Impl::Present() {
    std::memcpy(dibBits, work.px.data(), work.px.size() * sizeof(uint32_t));
    const POINT dst = ScreenOfWindowOrigin();
    SIZE size = {winW, winH};
    POINT src = {0, 0};
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UpdateLayeredWindow(hwnd, nullptr, const_cast<POINT*>(&dst), &size, memDc, &src, 0, &blend, ULW_ALPHA);
}


// ------------------------------------------------------------------- mouse ---

void ControlBar::Impl::OnMouseMove(int x, int y) {
    if (!inside) {
        inside = true;
        TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hwnd, 0};
        TrackMouseEvent(&tme);
        if (cfg.onPointerEntered) cfg.onPointerEntered();
    }
    if (dragging) {
        POINT c;
        GetCursorPos(&c);
        const Rect mon = {cfg.monitor.left, cfg.monitor.top, cfg.monitor.right, cfg.monitor.bottom};
        const Point next = ClampBarOrigin({c.x - grab.x, c.y - grab.y}, layout.width, layout.height, mon);
        if (next.x != origin.x || next.y != origin.y) {
            origin = next;
            dragMoved = true;
            MoveWindowToOrigin();
        }
        return;
    }
    const BarHit hit = HitTest(layout, ClientToBar(x, y));
    const BarButton nowHot = hit.kind == BarItemKind::Button ? hit.button : BarButton::None;
    if (nowHot != hot) {
        hot = nowHot;
        HideTip();
        ArmTip();
        Paint();
    }
}

// A press on a button arms it (it acts on release, on the same button); a press anywhere else on the
// bar starts a drag of the bar.
void ControlBar::Impl::OnLButtonDown(int x, int y) {
    HideTip();
    KillTimer(hwnd, kTipTimer);
    const BarHit hit = HitTest(layout, ClientToBar(x, y));
    if (hit.kind == BarItemKind::None) return;
    SetCapture(hwnd);  // keeps the drag going off the bar; capturing does not activate the window
    if (hit.kind == BarItemKind::Button) {
        if (hit.enabled) {
            pressed = hit.button;
            Paint();
        }
        return;
    }
    POINT c;
    GetCursorPos(&c);
    grab = POINT{c.x - origin.x, c.y - origin.y};
    dragging = true;
    dragMoved = false;
}

// The release: the end of a drag (the new place is saved), or the click of the armed button if the
// mouse is still on it.
void ControlBar::Impl::OnLButtonUp(int x, int y) {
    const BarButton was = pressed;
    const bool wasDragging = dragging;
    const bool moved = dragMoved;
    pressed = BarButton::None;
    dragging = false;
    dragMoved = false;
    if (GetCapture() == hwnd) ReleaseCapture();
    if (wasDragging) {
        if (moved) Changed();
        return;
    }
    if (was == BarButton::None) return;
    const BarHit hit = HitTest(layout, ClientToBar(x, y));
    Paint();
    // The action runs on release over the same button.
    if (hit.kind != BarItemKind::Button || hit.button != was || !hit.enabled) return;
    switch (was) {
    case BarButton::Pause: if (cfg.onPause) cfg.onPause(); break;
    case BarButton::Skip: if (cfg.onSkip) cfg.onSkip(); break;
    case BarButton::Stop: if (cfg.onStop) cfg.onStop(); break;
    case BarButton::Help: if (cfg.onHelp) cfg.onHelp(); break;
    case BarButton::Size: ToggleSize(); break;
    default: break;
    }
}

// The mouse left the bar: no hot button, no tooltip.
void ControlBar::Impl::OnLeave() {
    inside = false;
    hot = BarButton::None;
    KillTimer(hwnd, kTipTimer);
    HideTip();
    Paint();
}

// The Size button: Large <-> Small, saved through onChanged.
void ControlBar::Impl::ToggleSize() {
    cfg.size = cfg.size == BarSize::Large ? BarSize::Small : BarSize::Large;
    relayout = true;  // Paint() re-lays the bar out and keeps its centre
    state.size = cfg.size;
    hot = BarButton::None;  // the button under the mouse is another one now
    Paint();
    Changed();
}

// The size or the place changed: tell the app, which saves them. Offsets from the monitor's corner.
void ControlBar::Impl::Changed() {
    if (cfg.onChanged) {
        cfg.onChanged(cfg.size, std::max(0, origin.x - static_cast<int>(cfg.monitor.left)),
                      std::max(0, origin.y - static_cast<int>(cfg.monitor.top)));
    }
}

// ---------------------------------------------------------------- tooltips ---

void ControlBar::Impl::ArmTip() {
    if (hot == BarButton::None) return;
    SetTimer(hwnd, kTipTimer, 450, nullptr);  // the native tooltip's delay
}

// Shows the hot button's tooltip under the bar (above it if there is no room below), without
// activating it.
void ControlBar::Impl::ShowTip() {
    if (!tip || !inside || dragging || pressed != BarButton::None || hot == BarButton::None) return;
    tipFor = hot;
    const std::wstring text = TipTextFor(tipFor);
    if (text.empty()) return;

    HDC dc = GetDC(tip);
    HFONT font = CreateFontW(-Scale(15), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    const HGDIOBJ old = SelectObject(dc, font);
    SIZE ts = {};
    GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &ts);
    SelectObject(dc, old);
    DeleteObject(font);
    ReleaseDC(tip, dc);

    const int w = ts.cx + 2 * Scale(12), h = ts.cy + 2 * Scale(6);
    Rect target;
    for (const BarItem& it : layout.items) {
        if (it.kind == BarItemKind::Button && it.button == hot) target = it.rect;
    }
    int x = origin.x + (target.left + target.right) / 2 - w / 2;
    int y = origin.y + layout.height + Scale(8);
    if (y + h > cfg.monitor.bottom) y = origin.y - Scale(8) - h;  // no room below: above
    x = std::max(static_cast<int>(cfg.monitor.left), std::min(x, static_cast<int>(cfg.monitor.right) - w));
    SetWindowPos(tip, HWND_TOPMOST, x, y, w, h, SWP_NOACTIVATE);
    SetWindowRgn(tip, CreateRoundRectRgn(0, 0, w + 1, h + 1, Scale(10) * 2, Scale(10) * 2), TRUE);
    InvalidateRect(tip, nullptr, FALSE);
    ShowWindow(tip, SW_SHOWNOACTIVATE);
}

// Hides the tooltip, if it shows.
void ControlBar::Impl::HideTip() {
    if (tip) ShowWindow(tip, SW_HIDE);
}

// ================================================================== the public face ===

ControlBar::ControlBar() : impl_(std::make_unique<Impl>()) {}
ControlBar::~ControlBar() { Destroy(); }

bool ControlBar::Create(const ControlBarConfig& config) {
    if (impl_->Create(config)) return true;
    impl_->Destroy();
    return false;
}

void ControlBar::SetAnalysing(bool analysing) {
    if (!impl_->hwnd || impl_->state.analysing == analysing) return;
    impl_->state.analysing = analysing;
    impl_->Paint();
}

void ControlBar::Destroy() { impl_->Destroy(); }

// For lexia_barshot: paints the bar with `hover` as the hot button and writes the picture, shadow
// margin included, to a PNG file.
bool ControlBar::SaveSnapshot(const wchar_t* pngPath, BarButton hover) {
    Impl& s = *impl_;
    if (!s.hwnd) return false;
    s.inside = hover != BarButton::None;
    s.hot = hover;
    s.Paint();
    s.hot = BarButton::None;
    s.inside = false;
    CLSID png;
    if (!EncoderFor(L"image/png", &png)) return false;
    gp::Bitmap bmp(s.winW, s.winH, s.winW * 4, PixelFormat32bppPARGB, reinterpret_cast<BYTE*>(s.work.px.data()));
    return bmp.Save(pngPath, &png, nullptr) == gp::Ok;
}
