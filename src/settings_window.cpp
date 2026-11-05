#include "settings_window.h"

#include <windows.h>

#include <dwmapi.h>
#include <shellscalingapi.h>
#include <wrl/client.h>
#include <wrl/event.h>
#include <wrl/implements.h>

#include <WebView2.h>

#include <algorithm>
#include <cwchar>
#include <functional>
#include <string>
#include <vector>

#include "common.h"
#include "log.h"

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

namespace {

const wchar_t kClassName[] = L"LexiaSettingsWindow";
const wchar_t kHost[] = L"lexia.invalid";
const wchar_t kHomeUrl[] = L"https://lexia.invalid/index.html";
// The window is a fixed 800 x 600 (client area, device-independent pixels at 96 dpi): no resizing, no
// maximising. The page scrolls when the user zooms in.
const int kClientWidth = 800, kClientHeight = 600;
const DWORD kWindowStyle = WS_OVERLAPPEDWINDOW & ~static_cast<DWORD>(WS_THICKFRAME | WS_MAXIMIZEBOX);

// index.html, or index.html?page=<page> (a page name is lower-case letters only, checked by the caller).
std::wstring PageUrl(const std::wstring& page) { return page.empty() ? std::wstring(kHomeUrl) : std::wstring(kHomeUrl) + L"?page=" + page; }

const COLORREF kLightSurface = RGB(0xF5, 0xEF, 0xE8);  // paper in ui/css/tokens.css
const COLORREF kDarkSurface = RGB(0x2A, 0x24, 0x20);  // TODO(owner): final dark palette

// An HRESULT as "0x80070002", the form it is looked up by, for the log.
std::string Hex(HRESULT hr) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "0x%08lX", static_cast<unsigned long>(hr));
    return buf;
}

// Web addresses compare without letter case in the scheme and host.
bool StartsWithNoCase(const std::wstring& s, const wchar_t* prefix) {
    const size_t n = std::wcslen(prefix);
    return s.size() >= n && _wcsnicmp(s.c_str(), prefix, n) == 0;
}

// The only addresses the page may load: its own virtual host, and things that are not the network
// (ADR-0005: no request leaves the PC).
bool AllowedUri(const std::wstring& uri) {
    return StartsWithNoCase(uri, L"https://lexia.invalid/") || StartsWithNoCase(uri, L"data:") ||
           StartsWithNoCase(uri, L"blob:") || StartsWithNoCase(uri, L"about:blank");
}

// A message is accepted only from a page of Lexia's own virtual host.
bool FromOurHost(const std::wstring& uri) {
    return StartsWithNoCase(uri, L"https://lexia.invalid/") || _wcsicmp(uri.c_str(), L"https://lexia.invalid") == 0;
}

// WebView2 hands strings over as CoTaskMem memory that the caller frees: copy it, then free it.
std::wstring TakeString(LPWSTR s) {
    std::wstring out = s ? s : L"";
    CoTaskMemFree(s);
    return out;
}

// Device-independent pixels (96 dpi) -> pixels at `dpi`.
int Scale(int dip, UINT dpi) { return MulDiv(dip, static_cast<int>(dpi), 96); }

}  // namespace


struct SettingsWindow::Impl {
    ComPtr<ICoreWebView2Environment> env;
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> webview;
    std::vector<std::function<void(ICoreWebView2*)>> removers;  // one per add_... event handler

    // Takes the event handlers off before the controller is closed.
    void RemoveHandlers() {
        if (webview) {
            for (auto& remove : removers) remove(webview.Get());
        }
        removers.clear();
    }
};

SettingsWindow::SettingsWindow(Options options)
    : options_(std::move(options)), dark_(options_.dark), impl_(std::make_unique<Impl>()),
      alive_(std::make_shared<bool>(true)) {}

SettingsWindow::~SettingsWindow() {
    *alive_ = false;
    if (hwnd_) {
        SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0);  // no callback into a half-deleted object
        impl_->RemoveHandlers();
        if (impl_->controller) impl_->controller->Close();
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

// Paints the paper colour behind the page while WebView2 starts (no white flash), keeps WebView2 the
// size of the window, and follows DPI and theme changes.
LRESULT SettingsWindow::OnMessage(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_ERASEBKGND: {
        RECT rc;
        GetClientRect(hwnd_, &rc);
        HBRUSH brush = CreateSolidBrush(dark_ ? kDarkSurface : kLightSurface);
        FillRect(reinterpret_cast<HDC>(wp), &rc, brush);
        DeleteObject(brush);
        return 1;
    }
    case WM_SIZE:
        if (impl_->controller) {
            RECT rc;
            GetClientRect(hwnd_, &rc);
            impl_->controller->put_Bounds(rc);
        }
        return 0;
    case WM_MOVE:
        if (impl_->controller) impl_->controller->NotifyParentWindowPositionChanged();
        return 0;
    case WM_SETFOCUS:
        if (impl_->controller) impl_->controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
        return 0;
    case WM_DPICHANGED: {
        const RECT* r = reinterpret_cast<const RECT*>(lp);
        SetWindowPos(hwnd_, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    case WM_SETTINGCHANGE:
        if (lp && std::wcscmp(reinterpret_cast<const wchar_t*>(lp), L"ImmersiveColorSet") == 0 &&
            options_.onSystemThemeChanged) {
            options_.onSystemThemeChanged();
        }
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd_);
        return 0;
    case WM_DESTROY:
        impl_->RemoveHandlers();
        if (impl_->controller) impl_->controller->Close();
        impl_->webview.Reset();
        impl_->controller.Reset();
        impl_->env.Reset();
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

// Makes the window, centred on `monitor`, then starts WebView2 (asynchronous: the environment, then
// the controller, then the page). The page's handlers, the request filter of ADR-0005 and the
// settings (no context menu, no DevTools in Release) are set up as each part arrives.
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

    hwnd_ = nullptr;
    CreateWindowExW(0, kClassName, options_.title.c_str(), kWindowStyle, x, y, w, h, nullptr, nullptr,
                    wc.hInstance, this);
    if (!hwnd_) {
        LogError("[window] Could not create the Settings window (error ", GetLastError(), ").");
        return false;
    }
    SetDark(dark_);
    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
    // A new window comes to the front: the user just started Lexia, asked for the window from the tray,
    // or ran a second lexia.exe. Windows allows it in those cases (an elevated relaunch is allowed by
    // the Lexia that started it, see elevation.cpp).
    SetForegroundWindow(hwnd_);

    std::error_code ec;
    const std::filesystem::path userData = LexiaDataDir() / "WebView2";
    std::filesystem::create_directories(userData, ec);

    const std::shared_ptr<bool> alive = alive_;
    auto onEnv = [this, alive](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
        if (!*alive) return S_OK;
        if (FAILED(result) || !env) {
            LogError("[window] WebView2 could not start (", Hex(result), "); the Settings window closes.");
            MessageBoxW(hwnd_, options_.noRuntimeText.c_str(), options_.title.c_str(), MB_OK | MB_ICONERROR);
            if (*alive && IsWindow(hwnd_)) DestroyWindow(hwnd_);  // the box pumps messages: the owner may have dropped us
            return S_OK;
        }
        impl_->env = env;
        auto onController = [this, alive](HRESULT hr, ICoreWebView2Controller* controller) -> HRESULT {
            if (!*alive) return S_OK;
            if (FAILED(hr) || !controller) {
                LogError("[window] WebView2 could not make its view (", Hex(hr), "); the Settings window closes.");
                DestroyWindow(hwnd_);
                return S_OK;
            }
            impl_->controller = controller;
            controller->get_CoreWebView2(&impl_->webview);
            ICoreWebView2* wv = impl_->webview.Get();

            // Behave like a program window, not a browser (ui/ is our own page).
            ComPtr<ICoreWebView2Settings> st;
            wv->get_Settings(&st);
            st->put_IsStatusBarEnabled(FALSE);
            st->put_AreDefaultContextMenusEnabled(FALSE);
#ifdef NDEBUG
            st->put_AreDevToolsEnabled(FALSE);
#endif
            // Zoom (Ctrl +, Ctrl -, Ctrl wheel) stays on: the user may need 200 %.
            ComPtr<ICoreWebView2Settings4> st4;
            if (SUCCEEDED(st.As(&st4))) {
                st4->put_IsGeneralAutofillEnabled(FALSE);
                st4->put_IsPasswordAutosaveEnabled(FALSE);
            }

            ComPtr<ICoreWebView2Controller2> c2;
            if (SUCCEEDED(impl_->controller.As(&c2))) {
                const COLORREF bg = dark_ ? kDarkSurface : kLightSurface;
                c2->put_DefaultBackgroundColor({255, GetRValue(bg), GetGValue(bg), GetBValue(bg)});
            }

            // The pages come from <exe>\ui, as https://lexia.invalid/; nothing else may load.
            ComPtr<ICoreWebView2_3> wv3;
            if (SUCCEEDED(impl_->webview.As(&wv3))) {
                wv3->SetVirtualHostNameToFolderMapping(kHost, options_.uiDir.c_str(),
                                                       COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS);
            }
            wv->AddWebResourceRequestedFilter(L"*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);

            EventRegistrationToken token;
            wv->add_WebResourceRequested(
                Callback<ICoreWebView2WebResourceRequestedEventHandler>(
                    [this, alive](ICoreWebView2*, ICoreWebView2WebResourceRequestedEventArgs* args) -> HRESULT {
                        if (!*alive) return S_OK;
                        ComPtr<ICoreWebView2WebResourceRequest> req;
                        args->get_Request(&req);
                        LPWSTR raw = nullptr;
                        if (req) req->get_Uri(&raw);
                        const std::wstring uri = TakeString(raw);
                        if (AllowedUri(uri)) return S_OK;
                        LogWarn("[window] A request outside Lexia was refused: ", WideToUtf8(uri.substr(0, 60).c_str()));
                        ComPtr<ICoreWebView2WebResourceResponse> resp;
                        impl_->env->CreateWebResourceResponse(nullptr, 403, L"Forbidden", L"", &resp);
                        args->put_Response(resp.Get());
                        return S_OK;
                    })
                    .Get(),
                &token);
            impl_->removers.push_back([token](ICoreWebView2* w) { w->remove_WebResourceRequested(token); });
            wv->add_NavigationStarting(
                Callback<ICoreWebView2NavigationStartingEventHandler>(
                    [alive](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                        if (!*alive) return S_OK;
                        LPWSTR raw = nullptr;
                        args->get_Uri(&raw);
                        if (!AllowedUri(TakeString(raw))) args->put_Cancel(TRUE);
                        return S_OK;
                    })
                    .Get(),
                &token);
            impl_->removers.push_back([token](ICoreWebView2* w) { w->remove_NavigationStarting(token); });
            wv->add_NewWindowRequested(
                Callback<ICoreWebView2NewWindowRequestedEventHandler>(
                    [](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT {
                        args->put_Handled(TRUE);  // no second window, whatever the page asks
                        return S_OK;
                    })
                    .Get(),
                &token);
            impl_->removers.push_back([token](ICoreWebView2* w) { w->remove_NewWindowRequested(token); });
            wv->add_PermissionRequested(
                Callback<ICoreWebView2PermissionRequestedEventHandler>(
                    [](ICoreWebView2*, ICoreWebView2PermissionRequestedEventArgs* args) -> HRESULT {
                        args->put_State(COREWEBVIEW2_PERMISSION_STATE_DENY);
                        return S_OK;
                    })
                    .Get(),
                &token);
            impl_->removers.push_back([token](ICoreWebView2* w) { w->remove_PermissionRequested(token); });
            wv->add_ProcessFailed(
                Callback<ICoreWebView2ProcessFailedEventHandler>(
                    [alive](ICoreWebView2*, ICoreWebView2ProcessFailedEventArgs* args) -> HRESULT {
                        COREWEBVIEW2_PROCESS_FAILED_KIND kind = COREWEBVIEW2_PROCESS_FAILED_KIND_BROWSER_PROCESS_EXITED;
                        args->get_ProcessFailedKind(&kind);
                        if (*alive) LogWarn("[window] A WebView2 process failed (kind ", static_cast<int>(kind), ").");
                        return S_OK;
                    })
                    .Get(),
                &token);
            impl_->removers.push_back([token](ICoreWebView2* w) { w->remove_ProcessFailed(token); });
            wv->add_WebMessageReceived(
                Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                    [this, alive](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                        if (!*alive) return S_OK;
                        LPWSTR src = nullptr;
                        args->get_Source(&src);
                        if (!FromOurHost(TakeString(src))) return S_OK;
                        LPWSTR json = nullptr;
                        if (FAILED(args->get_WebMessageAsJson(&json))) return S_OK;
                        const std::string text = WideToUtf8(TakeString(json).c_str());
                        if (options_.onMessage) options_.onMessage(text);
                        return S_OK;
                    })
                    .Get(),
                &token);
            impl_->removers.push_back([token](ICoreWebView2* w) { w->remove_WebMessageReceived(token); });

            RECT rc;
            GetClientRect(hwnd_, &rc);
            impl_->controller->put_Bounds(rc);
            impl_->controller->put_IsVisible(TRUE);
            wv->Navigate(PageUrl(options_.startPage).c_str());
            impl_->controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
            return S_OK;
        };
        env->CreateCoreWebView2Controller(hwnd_, Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(onController).Get());
        return S_OK;
    };

    const HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, userData.c_str(), nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(onEnv).Get());
    if (FAILED(hr)) {
        LogError("[window] The WebView2 Runtime is not available (", Hex(hr), ").");
        MessageBoxW(hwnd_, options_.noRuntimeText.c_str(), options_.title.c_str(), MB_OK | MB_ICONERROR);
        PostMessageW(hwnd_, WM_CLOSE, 0, 0);  // not DestroyWindow: the owner drops us when Create returns false
        return false;
    }
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

// Loads index.html again on another page (?page=<page>).
void SettingsWindow::ShowPage(const std::wstring& page) {
    options_.startPage = page;
    if (impl_ && impl_->webview) impl_->webview->Navigate(PageUrl(page).c_str());
}

// The dark or light title bar (DWM attribute 20, DWMWA_USE_IMMERSIVE_DARK_MODE, not in older SDK
// headers) and the background colour behind the page.
void SettingsWindow::SetDark(bool dark) {
    dark_ = dark;
    if (!hwnd_) return;
    const BOOL value = dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd_, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &value, sizeof(value));
    SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    if (impl_ && impl_->controller) {
        ComPtr<ICoreWebView2Controller2> c2;
        if (SUCCEEDED(impl_->controller.As(&c2))) {
            const COLORREF bg = dark ? kDarkSurface : kLightSurface;
            c2->put_DefaultBackgroundColor({255, GetRValue(bg), GetGValue(bg), GetBValue(bg)});
        }
    }
    InvalidateRect(hwnd_, nullptr, TRUE);
}

// Sends one message (JSON text) to the page; nothing before WebView2 is ready.
void SettingsWindow::Post(const std::string& json) {
    if (!impl_ || !impl_->webview) return;
    impl_->webview->PostWebMessageAsJson(Utf8ToWide(json).c_str());
}
