#include "settings_window.h"

#include <windows.h>

#include <dwmapi.h>
#include <shellscalingapi.h>
#include <windowsx.h>  // GET_X_LPARAM

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cwchar>

#include "common.h"
#include "log.h"
#include "native_ui/d2d_canvas.h"
#include "native_ui/palette.h"
#include "native_ui/screen.h"
#include "native_ui/settings_ui.h"
#include "native_ui/uia_provider.h"
#include "tooltip_window.h"

namespace {

const wchar_t kClassName[] = L"LexiaSettingsWindow";
// The window is a fixed 800 x 600 (client area, device-independent pixels at 96 dpi): no resizing, no
// maximising.
const int kClientWidth = 800, kClientHeight = 600;
const DWORD kWindowStyle = WS_OVERLAPPEDWINDOW & ~static_cast<DWORD>(WS_THICKFRAME | WS_MAXIMIZEBOX);
const UINT_PTR kTimerFrame = 1;  // about 60 Hz while a transition or the tooltip delay runs
const UINT kFrameMs = 16;

// Device-independent pixels (96 dpi) -> pixels at `dpi`.
int Scale(int dip, UINT dpi) { return MulDiv(dip, static_cast<int>(dpi), 96); }

// Seconds on a clock that only goes forward (the Screen's transitions and tooltip delay count in it).
double NowSeconds() {
    using Clock = std::chrono::steady_clock;
    return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
}

// A virtual-key code -> the toolkit's key (Key::Other keeps the code for the key capture of K.4).
nui::Key KeyOf(WPARAM vk) {
    switch (vk) {
        case VK_TAB: return nui::Key::Tab;
        case VK_RETURN: return nui::Key::Enter;
        case VK_SPACE: return nui::Key::Space;
        case VK_ESCAPE: return nui::Key::Escape;
        case VK_LEFT: return nui::Key::Left;
        case VK_RIGHT: return nui::Key::Right;
        case VK_UP: return nui::Key::Up;
        case VK_DOWN: return nui::Key::Down;
        case VK_PRIOR: return nui::Key::PageUp;
        case VK_NEXT: return nui::Key::PageDown;
        case VK_HOME: return nui::Key::Home;
        case VK_END: return nui::Key::End;
        default: return nui::Key::Other;
    }
}

bool KeyIsDown(int vk) { return (GetKeyState(vk) & 0x8000) != 0; }


// The Voice library's answers reach the tree directly (no window message of their own), so each one is
// followed by a repaint request: the window does not learn from the tree that it changed outside an event.
class RepaintingSink : public VoiceEventSink {
public:
    RepaintingSink(VoiceEventSink& inner, std::function<void()> repaint) : inner_(inner), repaint_(std::move(repaint)) {}
    void CatalogueShown(std::vector<CatalogueEntry> voices) override { inner_.CatalogueShown(std::move(voices)); repaint_(); }
    void CatalogueFailed(VoiceProblem problem) override { inner_.CatalogueFailed(problem); repaint_(); }
    void DownloadProgress(const std::string& name, uint64_t done, uint64_t total) override { inner_.DownloadProgress(name, done, total); repaint_(); }
    void DownloadDone(const std::string& name) override { inner_.DownloadDone(name); repaint_(); }
    void DownloadFailed(const std::string& name, const ProblemFacts& facts) override { inner_.DownloadFailed(name, facts); repaint_(); }
    void VoiceAdded(const VoiceAddedFacts& added) override { inner_.VoiceAdded(added); repaint_(); }
    void VoiceRefused(const ProblemFacts& facts) override { inner_.VoiceRefused(facts); repaint_(); }
    void VoiceDeleteFailed(const std::string& voice, VoiceProblem problem) override { inner_.VoiceDeleteFailed(voice, problem); repaint_(); }
    void PresetSaved(const std::string& fileName) override { inner_.PresetSaved(fileName); repaint_(); }
    void PresetLoaded(const std::string& fileName) override { inner_.PresetLoaded(fileName); repaint_(); }
    void PresetFailed(PresetProblem problem) override { inner_.PresetFailed(problem); repaint_(); }

private:
    VoiceEventSink& inner_;
    std::function<void()> repaint_;
};
}  // namespace

// Everything the window hosts: the widget tree, its canvas, the tooltip.
struct SettingsWindow::Impl {
    nui::Screen screen;
    std::unique_ptr<nui::SettingsUi> ui;
    std::unique_ptr<nui::D2dCanvas> canvas;
    TooltipWindow tip;
    // UI Automation (K.5): declared after `screen`, which it reads, so it is destroyed before the tree.
    std::unique_ptr<UiaHost> uia;
    UINT dpi = 96;
    bool dark = false;
    bool trackingMouse = false;  // TrackMouseEvent(TME_LEAVE) is armed
    bool leftDown = false;       // we hold the capture for a press
    nui::PointF lastPos;
    bool timerOn = false;
    int lostFrames = 0;          // frames in a row that were lost (device loss); stops a retry loop
    std::string tipText;         // what the tooltip window shows now ("" = hidden)
    std::unique_ptr<RepaintingSink> events;  // the Voice library's sink, with a repaint after each answer
    // K.4: a key chip waited for a key when an Alt / F10 key went down, and Windows would take the rest of
    // that press (the character, the key-up) for the system menu. Set while those messages are eaten.
    bool swallowSysKeys = false;

    // Does the focused widget wait for a key (a key chip in capture)? Then it gets every key, even the ones
    // Windows keeps for itself. The window reads only its own key messages: no hook, no other window's keys.
    bool Capturing() const {
        const nui::Widget* f = screen.Focused();
        return f && f->CapturesAllKeys();
    }

    float ToDip(int px) const { return static_cast<float>(px) * 96.0f / static_cast<float>(dpi); }
    int ToPx(float dip) const { return static_cast<int>(std::lround(dip * static_cast<float>(dpi) / 96.0f)); }
};

SettingsWindow::SettingsWindow(Options options) : options_(std::move(options)), impl_(std::make_unique<Impl>()) {
    impl_->dark = options_.dark;
    impl_->ui = std::make_unique<nui::SettingsUi>(impl_->screen, options_.interfaceLanguage);
    impl_->ui->SetDark(options_.dark);
    impl_->ui->ShowPage(options_.startPage);
    // A user action leaves the tree after the event that made it is over (Screen::Defer), so the owner's
    // answer (a new state) never meets a widget that is in the middle of its own callback.
    impl_->screen.SetMessageSink([this](const PageMessage& m) {
        impl_->screen.Defer([this, m] {
            if (options_.onCommand) options_.onCommand(m);
        });
    });
}

SettingsWindow::~SettingsWindow() {
    if (hwnd_) {
        SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0);  // no callback into a half-deleted object
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

// The window class's procedure: finds the SettingsWindow (stored at WM_NCCREATE, the first message)
// and hands the message to it.
LRESULT CALLBACK SettingsWindow::WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        auto* self = static_cast<SettingsWindow*>(cs->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    auto* self = reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!self) return DefWindowProcW(hwnd, msg, wp, lp);
    return self->OnMessage(msg, wp, lp);
}

LRESULT SettingsWindow::OnMessage(UINT msg, WPARAM wp, LPARAM lp) {
    Impl& im = *impl_;

    // After any event: ask for a repaint if the tree wants one, keep the frame timer going while something
    // moves, and show or hide the tooltip the tree asks for.
    auto sync = [this, &im]() {
        if (!hwnd_) return;
        if (im.screen.NeedsRepaint()) InvalidateRect(hwnd_, nullptr, FALSE);
        if (im.screen.NeedsTick() && !im.timerOn) {
            im.timerOn = SetTimer(hwnd_, kTimerFrame, kFrameMs, nullptr) != 0;
        }
        const std::optional<nui::TooltipRequest> tip = im.screen.Tooltip();
        if (!tip) {
            if (!im.tipText.empty()) {
                im.tip.Hide();
                im.tipText.clear();
            }
        } else if (tip->text != im.tipText) {
            // The anchor is in client DIPs: to client pixels, then to the screen.
            POINT tl = {im.ToPx(tip->anchor.x), im.ToPx(tip->anchor.y)};
            POINT br = {im.ToPx(tip->anchor.Right()), im.ToPx(tip->anchor.Bottom())};
            ClientToScreen(hwnd_, &tl);
            ClientToScreen(hwnd_, &br);
            RECT anchor = {tl.x, tl.y, br.x, br.y};
            im.tip.Show(tip->text, anchor, im.dark);
            im.tipText = tip->text;
        }
    };
    // Sends a mouse event to the tree with the clock moved on first.
    // Tells UI Automation what the last event changed. After events only, never inside one; a plain pointer
    // move changes nothing a screen reader reads, unless a list is open (the highlighted row has the focus).
    auto notify = [&im]() {
        if (im.uia) im.uia->Update();
    };
    auto mouse = [&](nui::MouseKind kind, nui::MouseButton button, int xPx, int yPx, float wheel = 0) {
        im.screen.Tick(NowSeconds());
        nui::MouseEvent ev;
        ev.kind = kind;
        ev.button = button;
        ev.pos = {im.ToDip(xPx), im.ToDip(yPx)};
        ev.wheelDelta = wheel;
        im.lastPos = ev.pos;
        const bool used = im.screen.HandleMouse(ev);
        sync();
        if (kind != nui::MouseKind::Move || im.screen.Popup()) notify();
        return used;
    };

    // Sends a key to the tree: the toolkit's key, the virtual-key code and the modifiers as they are now. The
    // key itself is never logged (a captured key is the user's choice, but a log of keys is not Lexia's).
    auto deliverKey = [&](WPARAM vk, bool repeat) {
        im.screen.Tick(NowSeconds());
        nui::KeyEvent ev;
        ev.key = KeyOf(vk);
        ev.code = static_cast<int>(vk);
        ev.ctrl = KeyIsDown(VK_CONTROL);
        ev.alt = KeyIsDown(VK_MENU);
        ev.shift = KeyIsDown(VK_SHIFT);
        ev.win = KeyIsDown(VK_LWIN) || KeyIsDown(VK_RWIN);
        ev.repeat = repeat;
        const bool used = im.screen.HandleKey(ev);
        sync();
        notify();
        return used;
    };

    switch (msg) {
    case WM_ERASEBKGND:
        return 1;  // Direct2D clears the whole client area every frame
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd_, &ps);  // validates the window; the pixels come from Direct2D
        EndPaint(hwnd_, &ps);
        if (!im.canvas) return 0;
        im.screen.Tick(NowSeconds());
        if (im.canvas->BeginFrame()) {
            im.canvas->Clear(nui::PaletteFor(im.ui->Dark()).paper);
            im.screen.Paint(*im.canvas, nui::PaletteFor(im.ui->Dark()));
            if (im.canvas->EndFrame()) {
                im.lostFrames = 0;
            } else if (++im.lostFrames < 5) {
                InvalidateRect(hwnd_, nullptr, FALSE);  // a lost device: the next frame builds it again
            }
        }
        sync();
        return 0;
    }
    case WM_SIZE:
        if (im.canvas && wp != SIZE_MINIMIZED) {
            const int w = LOWORD(lp), h = HIWORD(lp);
            if (w > 0 && h > 0) im.canvas->Resize(w, h);
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        return 0;
    case WM_TIMER:
        if (wp == kTimerFrame) {
            im.screen.Tick(NowSeconds());
            if (!im.screen.NeedsTick()) {
                KillTimer(hwnd_, kTimerFrame);
                im.timerOn = false;
            }
            sync();
        }
        return 0;
    case WM_MOUSEMOVE:
        if (!im.trackingMouse) {
            TRACKMOUSEEVENT t = {sizeof(t), TME_LEAVE, hwnd_, 0};
            im.trackingMouse = TrackMouseEvent(&t) != FALSE;
        }
        mouse(nui::MouseKind::Move, nui::MouseButton::None, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_MOUSELEAVE:
        im.trackingMouse = false;
        if (!im.leftDown) mouse(nui::MouseKind::Leave, nui::MouseButton::None, 0, 0);
        return 0;
    case WM_LBUTTONDOWN:
        SetCapture(hwnd_);  // a drag of a slider goes on outside the window
        im.leftDown = true;
        mouse(nui::MouseKind::Down, nui::MouseButton::Left, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_LBUTTONUP:
        if (im.leftDown) {
            im.leftDown = false;
            ReleaseCapture();  // sends WM_CAPTURECHANGED, which finds leftDown false
        }
        mouse(nui::MouseKind::Up, nui::MouseButton::Left, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_CAPTURECHANGED:
        // The capture was taken from us in the middle of a press (another window came up): end the press as a
        // release where the pointer last was, so no slider stays in a drag.
        if (im.leftDown) {
            im.leftDown = false;
            im.screen.Tick(NowSeconds());
            nui::MouseEvent up;
            up.kind = nui::MouseKind::Up;
            up.button = nui::MouseButton::Left;
            up.pos = im.lastPos;
            im.screen.HandleMouse(up);
            sync();
            notify();
        }
        return 0;
    case WM_MOUSEWHEEL: {
        POINT p = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};  // screen coordinates
        ScreenToClient(hwnd_, &p);
        // A notch up (positive) scrolls the content down the page: the toolkit's positive = content moves up.
        const float notches = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wp)) / static_cast<float>(WHEEL_DELTA);
        mouse(nui::MouseKind::Wheel, nui::MouseButton::None, p.x, p.y, -notches * nui::kWheelNotchDips);
        return 0;
    }
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT) {
            SetCursor(LoadCursorW(nullptr, im.screen.Cursor() == nui::CursorHint::Hand ? IDC_HAND : IDC_ARROW));
            return TRUE;
        }
        return DefWindowProcW(hwnd_, msg, wp, lp);
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        const bool capturing = im.Capturing();
        const bool sys = msg == WM_SYSKEYDOWN;
        // Alt+F4 closes the window even while a chip waits: it is Windows' own key, and trapping it would
        // leave no way out but Esc. (It is not usable as a Key anyway.)
        const bool closeKey = sys && wp == VK_F4 && KeyIsDown(VK_MENU);
        if (capturing && sys && !closeKey) im.swallowSysKeys = true;
        const bool used = deliverKey(wp, (lp & (1 << 30)) != 0);  // bit 30: the key was already down
        // While a chip waits, every key is its own: F10 and Alt combinations (WM_SYSKEYDOWN) must not reach
        // DefWindowProc either, or Windows would open the system menu instead of leaving us the key.
        if (capturing && !closeKey) return 0;
        // A key the tree does not use goes on: Alt+F4 closes, Alt+Space opens the system menu.
        return used ? 0 : DefWindowProcW(hwnd_, msg, wp, lp);
    }
    case WM_KEYUP:
    case WM_SYSKEYUP:
        // PrintScreen sends no key-down to a window, only a key-up (with Alt: a system key-up). While a chip
        // waits, that one message is its key.
        if (wp == VK_SNAPSHOT && im.Capturing()) {
            if (msg == WM_SYSKEYUP) im.swallowSysKeys = true;
            deliverKey(wp, /*repeat=*/false);
            return 0;
        }
        if (im.swallowSysKeys && msg == WM_SYSKEYUP) {
            // The end of a press whose start we took (an Alt tap would open the system menu here). The flag
            // goes when Alt is up again.
            if (!KeyIsDown(VK_MENU)) im.swallowSysKeys = false;
            return 0;
        }
        return DefWindowProcW(hwnd_, msg, wp, lp);
    case WM_SYSCHAR:
        if (im.swallowSysKeys || im.Capturing()) return 0;  // Alt+letter would beep or pick a menu item
        return DefWindowProcW(hwnd_, msg, wp, lp);
    case WM_SYSCOMMAND:
        // Alt alone, F10 or Alt+Space ask for the system menu (SC_KEYMENU): not while a key is awaited.
        if ((wp & 0xFFF0) == SC_KEYMENU && (im.swallowSysKeys || im.Capturing())) return 0;
        return DefWindowProcW(hwnd_, msg, wp, lp);
    case WM_ACTIVATE:
        // Another window took the activation: a chip that waits stops waiting (as a blur does in a browser).
        // Taking the focus away makes the chip tell the page, the page tells the host, and the host gives
        // Lexia's keys back.
        if (LOWORD(wp) == WA_INACTIVE) {
            im.swallowSysKeys = false;
            if (im.Capturing()) {
                im.screen.SetFocus(nullptr);
                sync();
            }
        }
        return DefWindowProcW(hwnd_, msg, wp, lp);
    case WM_GETOBJECT:
        // UI Automation asks for the window's provider (K.5). Anything else (MSAA) gets the default answer.
        if (im.uia) {
            if (const LRESULT r = im.uia->HandleGetObject(wp, lp)) return r;
        }
        return DefWindowProcW(hwnd_, msg, wp, lp);
    case UiaHost::kActionsMessage:
        // Invoke, Toggle, SetValue ... that a screen reader asked for: done here, like a click, not inside the
        // UI Automation call (a click may open a file dialog).
        if (im.uia) im.uia->RunPendingActions();
        sync();
        notify();
        return 0;
    case WM_SETFOCUS:
        if (im.uia) im.uia->RaiseFocus();  // the window has the keyboard: say which element has it
        return DefWindowProcW(hwnd_, msg, wp, lp);
    case WM_DPICHANGED: {
        im.dpi = LOWORD(wp);
        if (im.uia) im.uia->SetDpi(im.dpi);
        if (im.canvas) im.canvas->SetDpi(static_cast<float>(im.dpi));
        im.tip.SetDpi(static_cast<int>(im.dpi));
        const RECT* r = reinterpret_cast<const RECT*>(lp);
        SetWindowPos(hwnd_, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    }
    case WM_SETTINGCHANGE:
        if (lp && std::wcscmp(reinterpret_cast<const wchar_t*>(lp), L"ImmersiveColorSet") == 0 &&
            options_.onSystemThemeChanged) {
            options_.onSystemThemeChanged();
        }
        if (wp == SPI_SETCLIENTAREAANIMATION) {
            BOOL animate = TRUE;
            SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animate, 0);
            im.screen.SetReducedMotion(!animate);
            sync();
        }
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd_);
        return 0;
    case WM_DESTROY:
        // First: from here every UI Automation element answers "not available" (a screen reader may call late).
        if (im.uia) im.uia->Detach();
        if (im.timerOn) KillTimer(hwnd_, kTimerFrame);
        im.timerOn = false;
        im.tip.Destroy();
        // Nothing may draw or call the owner after this: the swap chain goes with its window.
        im.canvas.reset();
        return 0;
    case WM_NCDESTROY: {
        const LRESULT r = DefWindowProcW(hwnd_, msg, wp, lp);
        const HWND h = hwnd_;
        SetWindowLongPtrW(h, GWLP_USERDATA, 0);
        hwnd_ = nullptr;
        if (options_.onDestroyed) options_.onDestroyed();  // last: the owner may drop us afterwards
        return r;
    }
    default:
        return DefWindowProcW(hwnd_, msg, wp, lp);
    }
}

// Makes the window, centred on `monitor`, then attaches the Direct2D canvas and the tooltip.
bool SettingsWindow::Create(const RECT& monitor) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);  // already registered the second time: fine

    // The size is in device-independent pixels; the DPI is the one of the monitor it opens on.
    UINT dpiX = 96, dpiY = 96;
    GetDpiForMonitor(MonitorFromRect(&monitor, MONITOR_DEFAULTTONEAREST), MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
    RECT outer = {0, 0, Scale(kClientWidth, dpiX), Scale(kClientHeight, dpiX)};
    AdjustWindowRectExForDpi(&outer, kWindowStyle, FALSE, 0, dpiX);
    int w = outer.right - outer.left;
    int h = outer.bottom - outer.top;
    const int monW = monitor.right - monitor.left;
    const int monH = monitor.bottom - monitor.top;
    w = std::min(w, monW);
    h = std::min(h, monH);
    const int x = monitor.left + (monW - w) / 2;
    const int y = monitor.top + (monH - h) / 2;

    impl_->dpi = dpiX;
    hwnd_ = nullptr;
    CreateWindowExW(0, kClassName, options_.title.c_str(), kWindowStyle, x, y, w, h, nullptr, nullptr,
                    wc.hInstance, this);
    if (!hwnd_) {
        LogError("[window] Could not create the Settings window (error ", GetLastError(), ").");
        return false;
    }
    // The UI Automation host needs the window and the tree; WM_GETOBJECT before this point gets the default.
    impl_->uia = std::make_unique<UiaHost>(hwnd_, impl_->screen, options_.interfaceLanguage);
    impl_->uia->SetDpi(impl_->dpi);
    SetDark(impl_->dark);

    // The canvas works in pixels of the client area (which is what the fixed size gives at this DPI).
    RECT client;
    GetClientRect(hwnd_, &client);
    impl_->canvas = std::make_unique<nui::D2dCanvas>();
    if (!impl_->canvas->AttachWindow(hwnd_, client.right - client.left, client.bottom - client.top,
                                     static_cast<float>(impl_->dpi))) {
        LogError("[window] The Settings window could not make its Direct2D canvas; it closes.");
        impl_->canvas.reset();
        // Not DestroyWindow here: the owner drops us when Create returns false, and the destructor does it.
        return false;
    }
    impl_->tip.Create(hwnd_, static_cast<int>(impl_->dpi));
    BOOL animate = TRUE;
    SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animate, 0);
    impl_->screen.SetReducedMotion(!animate);

    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
    // A new window comes to the front: the user just started Lexia, asked for the window from the tray,
    // or ran a second lexia.exe. Windows allows it in those cases (an elevated relaunch is allowed by
    // the Lexia that started it, see elevation.cpp).
    SetForegroundWindow(hwnd_);
    return true;
}

// Restores the window if it is minimised and brings it to the front.
void SettingsWindow::BringToFront() {
    if (!hwnd_) return;
    if (IsIconic(hwnd_)) ShowWindow(hwnd_, SW_RESTORE);
    ShowWindow(hwnd_, SW_SHOW);
    BringWindowToTop(hwnd_);
    SetForegroundWindow(hwnd_);
}

void SettingsWindow::ShowPage(nui::Page page) {
    impl_->ui->ShowPage(page);
    if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
    if (impl_->uia) impl_->uia->Update();  // the page is new: a structure change for a screen reader
}

void SettingsWindow::SetState(const UiState& state) {
    impl_->ui->SetState(state);
    if (impl_->ui->Dark() != impl_->dark) SetDark(impl_->ui->Dark());  // the title bar follows the state's theme
    if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
    if (impl_->uia) impl_->uia->Update();  // values, toggles and names that the state moved
}

// The dark or light title bar (DWM attribute 20, DWMWA_USE_IMMERSIVE_DARK_MODE, not in older SDK
// headers); the paper colour is the palette's, chosen when the tree paints.
void SettingsWindow::SetDark(bool dark) {
    impl_->dark = dark;
    impl_->ui->SetDark(dark);
    if (!hwnd_) return;
    const BOOL value = dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd_, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &value, sizeof(value));
    SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

VoiceEventSink* SettingsWindow::VoiceEvents() {
    if (!impl_->events) {
        impl_->events = std::make_unique<RepaintingSink>(impl_->ui->VoiceEvents(), [this] {
            if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
            if (impl_->uia) impl_->uia->Update();  // a catalogue, a progress step or a result: news for a screen reader
        });
    }
    return impl_->events.get();
}
