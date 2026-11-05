#include "app.h"

#include <windows.h>

#include <shellapi.h>
#include <windowsx.h>  // GET_X_LPARAM

#include <algorithm>
#include <cmath>
#include <chrono>
#include <string>

#include "autostart.h"
#include "capture.h"
#include "command_line.h"
#include "common.h"
#include "elevation.h"
#include "win32_theme.h"
#include "hotkeys.h"
#include "instance.h"
#include "key_edit.h"
#include "language.h"
#include "layout.h"
#include "log.h"
#include "overlay.h"
#include "recognize.h"
#include "version.h"
#include "strings/string_ids.h"
#include "strings/strings.h"
#include "tray_menu.h"

static const UINT kMsgSettingsChanged = WM_APP + 10;
static const UINT kMsgSpeechActivity = WM_APP + 11;  // the Reading queue became busy or idle
static const UINT kMsgSettingsWindowClosed = WM_APP + 12;  // the Settings window was destroyed
static const UINT kMsgTray = WM_APP + 13;  // the tray icon's events
static const UINT kMsgRunOnUi = WM_APP + 20;  // lParam: a heap std::function<void()> to run on the UI thread (the Voice tab's workers)
static const UINT_PTR kReloadTimer = 1;
static const UINT_PTR kSpeechKeyTimer = 2;
static const UINT kReloadDelayMs = 300;  // an editor may save in several steps

// A clock for the Speech key's tap window: milliseconds that only go forward (the wall clock can jump).
static long long NowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

App::~App() {
    voiceTab_.reset();  // waits for its workers; they post to window_
    settingsWindow_.reset();  // its callbacks reach this App
    watcher_.reset();  // its thread posts to window_
    speaker_.SetActivityNotify(nullptr);  // so does the speech thread
    if (window_) {
        // The workers are joined: closures still queued would never run and would leak.
        MSG pending;
        while (PeekMessageW(&pending, window_, kMsgRunOnUi, kMsgRunOnUi, PM_REMOVE)) {
            delete reinterpret_cast<std::function<void()>*>(pending.lParam);
        }
        DestroyWindow(window_);
    }
}

// The hidden App window's procedure: every message that other threads, timers, the tray, Explorer or a
// second Lexia send to the App arrives here and becomes one App call on the UI thread. `app` is null
// until CreateMessageWindow has stored it, and again while the App is destroyed.
LRESULT CALLBACK App::WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    App* app = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (app && msg == kMsgSpeechActivity) {
        app->SyncSpeechKeys();
        return 0;
    }
    if (msg == kMsgRunOnUi) {  // a worker thread asked for this to run here; the message owns it
        std::unique_ptr<std::function<void()>> job(reinterpret_cast<std::function<void()>*>(lp));
        if (app && job && *job) (*job)();
        return 0;
    }
    if (app && msg == kMsgSettingsWindowClosed) {
        app->OnSettingsWindowClosed();
        return 0;
    }
    if (app && msg == WM_TIMER && wp == kSpeechKeyTimer) {
        KillTimer(hwnd, kSpeechKeyTimer);
        app->OnSpeechKeyTimer();
        return 0;
    }
    if (app && msg == kMsgSettingsChanged) {
        SetTimer(hwnd, kReloadTimer, kReloadDelayMs, nullptr);  // restarts the wait on each change
        return 0;
    }
    if (app && msg == WM_TIMER && wp == kReloadTimer) {
        KillTimer(hwnd, kReloadTimer);
        app->OnSettingsFileChanged();
        return 0;
    }
    if (app && msg == kMsgTray) {
        app->OnTrayMessage(wp, lp);
        return 0;
    }
    if (app && msg == OpenSettingsMessage()) {  // a second launch of Lexia asked for the window
        app->OpenSettingsWindow();
        return 0;
    }
    static const UINT taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    if (app && msg == taskbarCreated) {
        app->OnTaskbarCreated();
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// Makes the hidden App window that receives the messages above (and the hotkeys' timers).
bool App::CreateMessageWindow() {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kAppWindowClass;
    RegisterClassExW(&wc);
    // Never shown, but top level (not message-only): only a top-level window gets the broadcast
    // "TaskbarCreated" that says the tray icon must be added again.
    window_ = CreateWindowExW(0, kAppWindowClass, L"Lexia", 0, 0, 0, 0, 0, nullptr, nullptr,
                              wc.hInstance, nullptr);
    if (!window_) return false;
    SetWindowLongPtrW(window_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    // An elevated Lexia would not hear a second launch ("open Settings") or Explorer ("the taskbar was made
    // again") from a program that is not elevated: UIPI drops registered messages unless they are let through.
    // Harmless when Lexia is not elevated.
    ChangeWindowMessageFilterEx(window_, OpenSettingsMessage(), MSGFLT_ALLOW, nullptr);
    ChangeWindowMessageFilterEx(window_, RegisterWindowMessageW(L"TaskbarCreated"), MSGFLT_ALLOW, nullptr);
    return true;
}

// Reads settings.json again after it changed on disk. Our own saves (the Language, On / Off)
// land here too and are recognised as no change.
void App::OnSettingsFileChanged() {
    Settings s;
    if (!ReloadSettings(s)) return;  // missing or not JSON: keep what is in force (logged)
    if (s == fileSettings_) return;

    // A Language or On / Off that Lexia changed itself but could not save is not in the file; an
    // unrelated edit must not put the old value back. Only a value the user changed is applied.
    Settings next = s;
    if (s.language == fileSettings_.language) next.language = settings_.language;
    if (s.enabled == fileSettings_.enabled) next.enabled = settings_.enabled;
    if (s.theme == fileSettings_.theme) next.theme = settings_.theme;
    if (s.runAtStartup == fileSettings_.runAtStartup) next.runAtStartup = settings_.runAtStartup;
    if (s.startMinimised == fileSettings_.startMinimised) next.startMinimised = settings_.startMinimised;
    if (s.runAsAdmin == fileSettings_.runAsAdmin) next.runAsAdmin = settings_.runAsAdmin;
    if (s.leftHanded == fileSettings_.leftHanded) next.leftHanded = settings_.leftHanded;
    if (s.speechVolume == fileSettings_.speechVolume) next.speechVolume = settings_.speechVolume;
    if (s.voices == fileSettings_.voices) next.voices = settings_.voices;
    if (s.activeVoices == fileSettings_.activeVoices) next.activeVoices = settings_.activeVoices;
    fileSettings_ = s;
    LogInfo("[settings] settings.json changed; applying it.");
    ApplySettings(next);
}

bool App::Init() {
    settings_ = LoadSettings();
    fileSettings_ = settings_;  // what the file says, before KeepToOffered
    for (const std::string& note : KeepToOffered(settings_)) LogWarn("[settings] ", note);

    // Run at startup: the registry is the truth, the setting only mirrors it (D6).
    const bool runKey = ReadRunAtStartup();
    if (runKey != settings_.runAtStartup) {
        LogInfo("[startup] The Run key says Run at startup is ", runKey ? "on" : "off",
                ", settings.json said otherwise; the Run key wins.");
        settings_.runAtStartup = runKey;
        if (SaveSettingValue("runAtStartup", runKey ? "true" : "false")) fileSettings_.runAtStartup = runKey;
    }
    // Write the value again at every start: it then names this lexia.exe (Lexia may have moved, or an
    // update installed it elsewhere) and has --minimised exactly when Start minimised is on (a value of
    // an older Lexia may say otherwise).
    if (runKey) WriteRunAtStartup(true, settings_.startMinimised);

    if (!speaker_.Init()) LogError("Speech is unavailable: Lexia starts, but will not read aloud.");
    speaker_.SetInterfaceLanguage(settings_.interfaceLanguage);
    speaker_.Configure(settings_.speechRate, settings_.speechVolume);

    VoiceTab::Host host;
    host.toPage = [this](const std::string& json) {
        if (settingsWindow_ && pageReady_) settingsWindow_->Post(json);  // a closed window drops it
    };
    host.pushState = [this] { PushState(); };
    host.owner = [this]() -> HWND { return settingsWindow_ ? settingsWindow_->Handle() : nullptr; };
    host.languageId = [this] { return AllLanguages()[langIdx_].id; };
    host.runOnUi = [this](std::function<void()> job) {
        if (!window_) return;
        auto* heap = new std::function<void()>(std::move(job));
        if (!PostMessageW(window_, kMsgRunOnUi, 0, reinterpret_cast<LPARAM>(heap))) delete heap;
    };
    voiceTab_ = std::make_unique<VoiceTab>(settings_, fileSettings_, speaker_, std::move(host));
    voiceTab_->Apply();

    // Start in the language from the settings (falling back to the default one).
    const std::vector<Language>& langs = AllLanguages();
    for (size_t i = 0; i < langs.size(); ++i) {
        if (langs[i].id == settings_.language) langIdx_ = i;
    }
    if (langIdx_ != 0 && !OcrEngine::ModelsPresent(langs[langIdx_])) {
        LogWarn("[settings] The models for ", langs[langIdx_].name,
                " are not installed; starting with ", langs[0].name, " instead.");
        langIdx_ = 0;
    }
    auto engine = std::make_shared<OcrEngine>();
    if (!engine->Init(langs[langIdx_])) {
        LogError("\nCould not load the OCR models from: ", OcrEngine::ModelsDir(), "\n",
                 "That folder needs det.onnx, ", langs[langIdx_].recModel, " and ",
                 langs[langIdx_].dictFile, " (see the setup notes).");
        return false;
    }
    ocr_ = std::move(engine);

    // Before RegisterKeys: whether the window exists decides when the Pause and Speech keys are taken.
    if (CreateMessageWindow()) {
        watcher_ = std::make_unique<SettingsWatcher>(SettingsPath(), window_, kMsgSettingsChanged);
        // The speech thread tells the UI thread when it starts or stops having something to say,
        // so the Pause and Speech keys are taken only then. Posting never blocks.
        speaker_.SetActivityNotify(
            [hwnd = window_] { PostMessageW(hwnd, kMsgSpeechActivity, 0, 0); });
    } else {
        LogWarn("[settings] Live reload is off (no window); edits to settings.json need a restart. "
                "The Pause and Speech keys are taken all the time, even while Lexia is silent.");
    }

    if (window_) UpdateTray();  // the tray icon (D2), added once the window can receive its events
    RegisterKeys();  // a key that is taken is reported and does nothing
    PrintHelp();

    // The tray icon is how Lexia is reached; the Settings window opens at start unless Lexia was started
    // with --minimised (the Run value has it when Start minimised is on, so a start at logon stays in
    // the tray). A manual start, and the first start after the installer, open the window.
    if (!StartedMinimised()) OpenSettingsWindow();
    return true;
}

// The UI thread's message loop, until Quit(). The global hotkeys (RegisterHotKey) arrive as WM_HOTKEY
// thread messages, with no window, so they are handled here; everything else goes to its window.
// After the loop: the clean shutdown (speech, keys, the Settings window, the tray icon).
int App::Run() {
    MSG msg;
    while (!quit_ && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message != WM_HOTKEY) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);  // the settings reload arrives as a window message
            continue;
        }
        // A key press queued before Off must not act now.
        if (!settings_.enabled) continue;
        if (capturingKey_) continue;  // the user is choosing a key: the press is theirs, not an action
        switch (msg.wParam) {
        case HK_PAUSE: TogglePause(); break;
        case HK_SPEECH: PressSpeechKey(); break;
        case HK_LANG: SwitchLanguage(); break;
        case HK_READ: OpenOverlay(); break;
        }
    }

    speaker_.Stop();
    UnregisterKeys();
    settingsWindow_.reset();  // the window and its WebView2 go before COM is shut down
    tray_.Remove();
    return 0;
}

void App::Quit() {
    quit_ = true;  // not only PostQuitMessage: a key press already queued must not open a Session
    PostQuitMessage(0);
}

void App::Stop() { speaker_.Stop(); }

void App::Skip() { speaker_.Skip(); }

// The Pause key (F7): Pause, or Resume when paused.
void App::TogglePause() {
    if (speaker_.Paused()) {
        speaker_.Resume();
    } else {
        speaker_.Pause();
    }
}

// The Speech key (F6): the Speaker counts the taps (tts/speech_key.h); the App only owns the timer
// that ends the tap window.
void App::PressSpeechKey() {
    if (!window_) {  // no timer to end the tap window with: just Skip
        speaker_.Skip();
        return;
    }
    ArmSpeechKeyTimer(speaker_.TapSpeechKey(NowMs()));
}

void App::OnSpeechKeyTimer() { ArmSpeechKeyTimer(speaker_.SettleSpeechKey(NowMs())); }

// ms > 0: wake up when the tap window ends; 0: nothing waits, so no timer.
void App::ArmSpeechKeyTimer(long long ms) {
    if (ms > 0) {
        SetTimer(window_, kSpeechKeyTimer, static_cast<UINT>(ms), nullptr);  // restarts a running one
    } else {
        KillTimer(window_, kSpeechKeyTimer);
    }
}

// On / Off (tray, Home page). Off frees every key, so the game gets F6 / F7 / F8 back, without ending
// Lexia. The state is saved, so it is the same after a restart.
void App::SetOn(bool on) {
    if (on == settings_.enabled) return;
    settings_.enabled = on;
    UnregisterKeys();
    RegisterKeys();  // retries a key that was taken before; Off takes none
    LogInfo(on ? "Lexia is On." : "Lexia is Off: no keys are taken until it is turned on.");
    if (SaveSettingValue("enabled", on ? "true" : "false")) fileSettings_.enabled = on;
    PushState();
}


// ------------------------------------------------------------- Settings window ---

// The state the page draws (ui/README.md). Voices and controls are the Voice tab's (D5).
UiState App::BuildUiState() const {
    UiState s;
    s.on = settings_.enabled;
    s.language = AllLanguages()[langIdx_].id;
    s.theme = settings_.theme;
    s.version = LEXIA_VERSION;
    s.runAtStartup = settings_.runAtStartup;
    s.startMinimised = settings_.startMinimised;
    s.dark = IsDarkTheme(settings_.theme, SystemUsesLightTheme());
    s.runAsAdmin = settings_.runAsAdmin;
    static const bool elevated = IsElevated();  // cannot change while Lexia runs
    s.elevated = elevated;
    s.leftHanded = settings_.leftHanded;
    s.masterVolume.value = settings_.speechVolume;
    auto key = [](const char* action, const Hotkey& k) {
        return UiKey{action, k.IsBound() ? k.text : std::string()};
    };
    s.keys = {key("read", settings_.read), key("pause", settings_.pause), key("speech", settings_.speech)};
    if (voiceTab_) voiceTab_->FillState(s);
    return s;
}

void App::PushState() {
    UpdateTray();  // the tray icon is the first to show On / Off, window or not
    if (!settingsWindow_ || !pageReady_) return;
    settingsWindow_->Post(StateMessage(BuildUiState()));
}

// Windows' light / dark mode changed (or the theme setting did): the window's frame follows.
void App::ApplyWindowTheme() {
    if (settingsWindow_) settingsWindow_->SetDark(IsDarkTheme(settings_.theme, SystemUsesLightTheme()));
}

void App::OpenSettingsWindow(std::string_view page) {
    std::wstring startPage;  // only lower-case letters: ?page=<name>; anything else is the Home page
    if (!page.empty() && std::all_of(page.begin(), page.end(), [](char c) { return c >= 'a' && c <= 'z'; })) {
        startPage.assign(page.begin(), page.end());
    }
    if (settingsWindow_ && IsWindow(settingsWindow_->Handle())) {
        if (!startPage.empty()) {
            pageReady_ = false;  // the page loads again and sends `ready`
            settingsWindow_->ShowPage(startPage);
        }
        settingsWindow_->BringToFront();
        return;
    }
    settingsWindow_.reset();
    pageReady_ = false;

    SettingsWindow::Options o;
    o.title = Utf8ToWide(strings::Text(settings_.interfaceLanguage, strings::id::kWindowTitle));
    o.noRuntimeText = Utf8ToWide(strings::Text(settings_.interfaceLanguage, strings::id::kWindowNoRuntime));
    o.uiDir = ExeDir() / "ui";
    o.startPage = startPage;
    o.dark = IsDarkTheme(settings_.theme, SystemUsesLightTheme());
    o.onMessage = [this](const std::string& json) { HandlePageMessage(json); };
    o.onSystemThemeChanged = [this] { ApplyWindowTheme(); };
    // Called inside the window's own procedure: the object is dropped later, by a posted message.
    o.onDestroyed = [this] {
        pageReady_ = false;
        if (window_) PostMessageW(window_, kMsgSettingsWindowClosed, 0, 0);
    };
    if (!std::filesystem::exists(o.uiDir / "index.html")) {
        LogError("[window] The Settings window's pages are missing: ", o.uiDir, ". Reinstall Lexia.");
        return;
    }
    settingsWindow_ = std::make_unique<SettingsWindow>(std::move(o));
    if (!settingsWindow_->Create(ActiveMonitorRect())) settingsWindow_.reset();
}

// The posted end of a closed Settings window (see onDestroyed above): drop the object.
void App::OnSettingsWindowClosed() {
    // A stale message (its window is gone and a new one is open) must not drop the new one.
    if (settingsWindow_ && !IsWindow(settingsWindow_->Handle())) {
        settingsWindow_.reset();
        pageReady_ = false;
        EndKeyCapture();  // nobody is left to answer: give the keys back
    }
}

// One message from the page (checked by ParsePageMessage) -> one action, then a new state for the page.
void App::HandlePageMessage(const std::string& json) {
    const PageMessage m = ParsePageMessage(json);
    if (voiceTab_ && voiceTab_->Handle(m)) return;  // the Voice tab's messages
    switch (m.type) {
    case PageMessageType::Ready:
        pageReady_ = true;
        EndKeyCapture();  // a reloaded page is not waiting for a key any more
        if (settingsWindow_) settingsWindow_->Post(StringsMessage(settings_.interfaceLanguage));
        PushState();  // strings first, then state: the page draws from both
        break;
    case PageMessageType::SetOn:
        SetOn(m.on);
        PushState();  // also when nothing changed: the page shows only what the state says
        break;
    case PageMessageType::SetTheme:
        if (m.theme != settings_.theme) {
            settings_.theme = m.theme;
            if (SaveSettingValue("theme", "\"" + m.theme + "\"")) fileSettings_.theme = m.theme;
        }
        ApplyWindowTheme();
        PushState();
        break;
    case PageMessageType::CaptureKey: CaptureKey(); break;
    case PageMessageType::ReadKeyCard: ReadKeyCard(m.action); break;
    case PageMessageType::CancelKeyCapture:
        EndKeyCapture();
        PushState();
        break;
    case PageMessageType::SetKey: SetKey(m.action, m.key); break;
    case PageMessageType::SetRunAtStartup: SetRunAtStartup(m.on); break;
    case PageMessageType::SetStartMinimised: SetStartMinimised(m.on); break;
    case PageMessageType::SetRunAsAdmin: SetRunAsAdmin(m.on); break;
    case PageMessageType::SetLeftHanded: SetBoolSetting("leftHanded", m.on, settings_.leftHanded, fileSettings_.leftHanded); break;  // read at the next Session
    case PageMessageType::SetMasterVolume: SetMasterVolume(static_cast<int>(std::lround(m.value))); break;
    case PageMessageType::OpenLink: OpenLink(m.link); break;
    case PageMessageType::OpenNotices: OpenNotices(); break;
    case PageMessageType::Unknown:
        LogWarn("[window] A message from the Settings window was ignored: \"", m.typeText, "\".");
        break;
    default:
        LogDebug("[window] Not handled yet: ", m.typeText);
        break;
    }
}

// "10.0.26100", for the bug report link. RtlGetVersion is called through GetProcAddress because it is
// not in the SDK's import libraries; it tells the truth where GetVersionEx can be made to lie
// (compatibility modes, a missing manifest entry).
static std::string WindowsVersionText() {
    using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);
    OSVERSIONINFOW info = {};
    info.dwOSVersionInfoSize = sizeof(info);
    if (HMODULE ntdll = GetModuleHandleW(L"ntdll.dll")) {
        const FARPROC proc = GetProcAddress(ntdll, "RtlGetVersion");
        if (proc) {
            auto fn = reinterpret_cast<RtlGetVersionFn>(reinterpret_cast<void*>(proc));
            if (fn(&info) == 0) {
                return std::to_string(info.dwMajorVersion) + "." + std::to_string(info.dwMinorVersion) + "." +
                       std::to_string(info.dwBuildNumber);
            }
        }
    }
    return "unknown";
}

// Opens one of the fixed links in the default program (the browser, or the mail program). The one place
// Lexia starts either, and only when the user clicked a link (ADR-0005); the page names the link, the
// URL is the host's. The GitHub link is a new issue with only the two versions in it.
void App::OpenLink(LinkId id) {
    const std::wstring url = Utf8ToWide(LinkUrl(id, LEXIA_VERSION, WindowsVersionText()));
    const HWND owner = settingsWindow_ ? settingsWindow_->Handle() : nullptr;
    if (reinterpret_cast<INT_PTR>(ShellExecuteW(owner, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) <= 32) {
        LogWarn("[window] The link \"", LinkName(id), "\" could not be opened.");
    }
}

// The About page's "third-party notices": the package's THIRD-PARTY-NOTICES.txt in the user's text editor.
void App::OpenNotices() {
    const std::filesystem::path file = ExeDir() / "THIRD-PARTY-NOTICES.txt";
    if (!std::filesystem::exists(file)) {
        LogWarn("[window] THIRD-PARTY-NOTICES.txt is not next to lexia.exe: ", file);
        return;
    }
    const HWND owner = settingsWindow_ ? settingsWindow_->Handle() : nullptr;
    if (reinterpret_cast<INT_PTR>(ShellExecuteW(owner, L"open", file.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) <= 32) {
        LogWarn("[window] The third-party notices could not be opened.");
    }
}

// The Control bar changed size or place: keep it, for this run and in settings.json (only the
// values that changed). Our own save is not "an edit": fileSettings_ follows it.
void App::SaveControlBar(BarSize size, int x, int y) {
    if (size != settings_.controlBar.size) {
        settings_.controlBar.size = size;
        if (SaveSettingValue("controlBar.size", size == BarSize::Small ? "\"small\"" : "\"large\"")) {
            fileSettings_.controlBar.size = size;
        }
    }
    if (x != settings_.controlBar.x) {
        settings_.controlBar.x = x;
        if (SaveSettingValue("controlBar.x", std::to_string(x))) fileSettings_.controlBar.x = x;
    }
    if (y != settings_.controlBar.y) {
        settings_.controlBar.y = y;
        if (SaveSettingValue("controlBar.y", std::to_string(y))) fileSettings_.controlBar.y = y;
    }
}

// ------------------------------------------------------------ Settings tab (D6) ---

// A key chip was selected: while the page waits for a key, the Lexia keys must not act or hold the
// key the user is about to press (F8 would open the Overlay instead of being chosen).
void App::CaptureKey() {
    if (capturingKey_) return;
    capturingKey_ = true;
    UnregisterKeys();
    LogDebug("[window] Waiting for a key; Lexia's keys are released until it comes.");
}

// The capture ended without a key (cancelled, or the window closed): take Lexia's keys again.
void App::EndKeyCapture() {
    if (!capturingKey_) return;
    capturingKey_ = false;
    RegisterKeys();  // a key another program holds is reported, as at start-up
}

// The key card's play button: speak what the key does, so a person who cannot read the card well yet (Lexia
// may still be Off) hears it. The words are the card's own, from the string table (window.keys.<action>Desc,
// one Block per line), in the Interface language; the page sends only which key. It is a Reading of its
// own: it takes the place of what was being spoken, so a second press starts it again.
void App::ReadKeyCard(const std::string& action) {
    const std::string text = strings::Text(settings_.interfaceLanguage, "window.keys." + action + "Desc");
    std::vector<std::string> blocks;
    size_t start = 0;
    while (start <= text.size()) {
        const size_t end = text.find('\n', start);
        const std::string line = Trim(text.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (!line.empty()) blocks.push_back(line);
        if (end == std::string::npos) break;
        start = end + 1;
    }
    if (!blocks.empty()) speaker_.SpeakReading(blocks, /*append=*/false);
}

// The page heard a key. Whatever the answer, the capture is over and the page gets a new `state`
// (it shows the key of that state, so a refused key puts the old one back).
void App::SetKey(const std::string& action, const std::string& key) {
    capturingKey_ = false;
    const KeyEditResult r = CheckKeyEdit(settings_, action, key);
    if (r.outcome != KeyEditOutcome::Accepted) {
        LogWarn("[settings] The key \"", key, "\" for ", action, " was refused: ", KeyEditOutcomeText(r.outcome),
                r.usedBy.empty() ? std::string() : " (" + r.usedBy + ")", ".");
        RegisterKeys();  // the capture released them
        PushState();
        return;
    }
    Settings next = settings_;
    Hotkey& dst = action == "read" ? next.read : action == "pause" ? next.pause : next.speech;
    dst = r.key;
    if (SaveSettingValue(HotkeySettingName(action), "\"" + r.key.text + "\"")) {
        Hotkey& mirror = action == "read" ? fileSettings_.read : action == "pause" ? fileSettings_.pause
                                                                                  : fileSettings_.speech;
        mirror = r.key;
    }
    LogInfo("[settings] The key for ", action, " is now ", r.key.text, ".");
    ApplySettings(next);  // re-registers the keys that changed, and pushes the state
    RegisterKeys();       // the capture released them all: a same key (no change) comes back too
}

// The Run at startup switch: write the Run value, then read it back, so the page shows what the
// registry really holds.
void App::SetRunAtStartup(bool on) {
    if (WriteRunAtStartup(on, settings_.startMinimised)) LogInfo("[startup] Run at startup is ", on ? "on" : "off", ".");
    const bool actual = ReadRunAtStartup();  // the registry is the truth, also if the write failed
    settings_.runAtStartup = actual;
    if (SaveSettingValue("runAtStartup", actual ? "true" : "false")) fileSettings_.runAtStartup = actual;
    PushState();
}

// The Start minimised switch: save it, and while Run at startup is on write the Run value again, so the
// next logon starts Lexia in the tray (on) or with its window (off).
void App::SetStartMinimised(bool on) {
    settings_.startMinimised = on;
    if (SaveSettingValue("startMinimised", on ? "true" : "false")) fileSettings_.startMinimised = on;
    if (settings_.runAtStartup && WriteRunAtStartup(true, on)) {
        LogInfo("[startup] A start at logon ", on ? "stays in the tray." : "opens the Settings window.");
    }
    PushState();
}

// A boolean setting that is read when it is used: remember it, show it in the state.
void App::SetBoolSetting(const char* key, bool on, bool& inForce, bool& inFile) {
    inForce = on;
    if (SaveSettingValue(key, on ? "true" : "false")) inFile = on;
    PushState();
}

// Run as admin (ADR-0008). Off or on, the setting is saved first. Turned on while Lexia is not elevated, it
// starts an elevated Lexia (the UAC prompt) and ends this one; the new one opens its Settings window. If the
// person says no, or it fails, this Lexia goes on, the setting stays on and the state says elevated = false.
// Turned off, it applies at the next start.
void App::SetRunAsAdmin(bool on) {
    SetBoolSetting("runAsAdmin", on, settings_.runAsAdmin, fileSettings_.runAsAdmin);  // also pushes the state
    if (!on || IsElevated()) return;
    LogInfo("[admin] Run as admin is on and Lexia is not elevated: starting it again as administrator.");
    switch (RelaunchElevated(Utf8ToWide(RelaunchArgs(/*minimised=*/false)))) {
    case RelaunchResult::Started:
        Quit();  // a clean end: the new Lexia waits for the single-instance mutex until this one lets go
        break;
    case RelaunchResult::Cancelled:
        LogInfo("[admin] Not elevated: Run as admin stays on, and takes effect at the next start.");  // the state already says so
        break;
    case RelaunchResult::Failed:
        LogWarn("[admin] Lexia could not start itself as administrator; it goes on as it is.");
        break;
    }
}

// The master volume (0..100): saved as speech.volume and given to the Speaker at once.
void App::SetMasterVolume(int volume) {
    volume = std::min(std::max(volume, 0), 100);
    if (volume != settings_.speechVolume) {
        settings_.speechVolume = volume;
        if (SaveSettingValue("speech.volume", std::to_string(volume))) fileSettings_.speechVolume = volume;
        speaker_.Configure(settings_.speechRate, settings_.speechVolume);
    }
    PushState();  // also when nothing changed: the page draws only the state
}

// The current OCR engine, for a job. A job keeps its shared_ptr, so a Language switch during the job
// does not delete the engine under it.
std::shared_ptr<OcrEngine> App::Engine() {
    std::lock_guard<std::mutex> lock(ocrMutex_);
    return ocr_;
}

// The Language key (unbound in 0.5): the next Language in the table that is offered and has its models,
// in a circle. It is saved and announced in its own name; with none, the "no other Language"
// Announcement.
void App::SwitchLanguage() {
    const std::vector<Language>& langs = AllLanguages();
    for (size_t k = 1; k < langs.size(); ++k) {
        const size_t cand = (langIdx_ + k) % langs.size();
        if (!langs[cand].offered || !OcrEngine::ModelsPresent(langs[cand])) continue;
        if (UseLanguage(cand)) {
            LogInfo("\nLanguage: ", langs[langIdx_].name);
            settings_.language = langs[langIdx_].id;
            if (SaveSettingValue("language", "\"" + langs[langIdx_].id + "\"")) {
                fileSettings_.language = langs[langIdx_].id;
            }
            speaker_.AnnounceLanguage(langs[langIdx_]);  // its own name, in its own Voice
            PushState();
            return;
        }
    }
    LogWarn("\nNo other language available (check the models folder).");
    speaker_.Announce(strings::id::kAnnounceNoOtherLanguage);
}

// Loads the models for Language `idx` into a new engine and swaps it in only if that worked, so
// a failure leaves the current Language fully usable.
bool App::UseLanguage(size_t idx) {
    auto fresh = std::make_shared<OcrEngine>();
    if (!fresh->Init(AllLanguages()[idx])) return false;
    {
        std::lock_guard<std::mutex> lock(ocrMutex_);
        ocr_ = std::move(fresh);
    }
    langIdx_ = idx;
    ++languageEpoch_;
    return true;
}

static bool SameKey(const Hotkey& a, const Hotkey& b) { return a.mods == b.mods && a.vk == b.vk; }

// Puts a whole new Settings in force (a settings.json edit, or a key change from the page): speech,
// the Voice tab, the Language (only if its models load), the keys (only if they changed), the theme.
// What a Session reads (colours, Control bar) is read when the next one opens.
void App::ApplySettings(const Settings& wanted) {
    Settings s = wanted;
    for (const std::string& note : KeepToOffered(s)) LogWarn("[settings] ", note);
    const Settings old = settings_;
    settings_ = s;
    // Edited in settings.json: make the Run key say so (on / off, and --minimised or not).
    if (s.runAtStartup != old.runAtStartup || (s.runAtStartup && s.startMinimised != old.startMinimised)) {
        WriteRunAtStartup(s.runAtStartup, s.startMinimised);
        settings_.runAtStartup = ReadRunAtStartup();
    }
    speaker_.SetInterfaceLanguage(s.interfaceLanguage);
    speaker_.Configure(s.speechRate, s.speechVolume);
    if (voiceTab_) voiceTab_->SettingsApplied();  // the Voice controls and the Voice choice

    // The colours, key names and Interface language are read from settings_ when the next Session
    // opens.
    if (s.interfaceLanguage != old.interfaceLanguage) {
        LogInfo("[settings] Interface language: ", s.interfaceLanguage);
    }
    const std::vector<Language>& langs = AllLanguages();
    if (s.language != langs[langIdx_].id) {
        bool switched = false;
        for (size_t i = 0; i < langs.size() && !switched; ++i) {
            if (langs[i].id != s.language) continue;
            if (!OcrEngine::ModelsPresent(langs[i])) {
                LogWarn("[settings] The models for ", langs[i].name, " are not installed; keeping ",
                        langs[langIdx_].name, ".");
            } else if (UseLanguage(i)) {
                switched = true;
            } else {
                LogWarn("[settings] The models for ", langs[i].name, " could not be loaded; keeping ",
                        langs[langIdx_].name, ".");
            }
        }
        if (!switched) settings_.language = langs[langIdx_].id;
    }

    const bool keysChanged = !SameKey(old.read, s.read) || !SameKey(old.pause, s.pause) ||
                             !SameKey(old.speech, s.speech) ||
                             !SameKey(old.switchLanguage, s.switchLanguage);
    if (old.enabled != s.enabled || keysChanged) {
        UnregisterKeys();
        RegisterKeys();
    }

    ApplyWindowTheme();
    PushState();
}

// Registers what is bound. A key another program already holds is reported and that action does
// nothing, instead of Lexia giving up; the configured key is kept, so the next RegisterKeys (On
// again, or a changed key) tries it again. Returns true if every wanted key was registered.
bool App::RegisterKeys() {
    bool ok = true;
    for (int id : {HK_READ, HK_PAUSE, HK_SPEECH, HK_LANG}) ok = TryRegister(id) && ok;
    return ok;
}

// The action's name for the log (developer text, not shown in the UI).
static const char* KeyName(int id) {
    switch (id) {
    case HK_READ: return "Read";
    case HK_PAUSE: return "Pause";
    case HK_SPEECH: return "Speech";
    default: return "Language";
    }
}

// The configured key of hotkey id `id` (HK_* in hotkeys.h).
const Hotkey& App::KeyFor(int id) const {
    switch (id) {
    case HK_READ: return settings_.read;
    case HK_PAUSE: return settings_.pause;
    case HK_SPEECH: return settings_.speech;
    default: return settings_.switchLanguage;
    }
}

// Registers key `id` if it is bound and wanted right now (hotkeys.h KeyWanted: the Pause and
// Speech keys only while Lexia is speaking or paused, so a game keeps them otherwise). A key
// another program holds is reported once, and the next try (speech starting again) is quiet.
bool App::TryRegister(int id) {
    const Hotkey& k = KeyFor(id);
    if (!k.IsBound()) return true;  // clashed in settings.json (already noted); keep running
    if (capturingKey_) return true;  // the Settings window waits for a key (D6)
    if (!KeyWanted(id, settings_.enabled, Speaking())) return true;
    if (std::find(registeredKeys_.begin(), registeredKeys_.end(), id) != registeredKeys_.end()) {
        return true;
    }
    const auto taken = std::find(takenKeys_.begin(), takenKeys_.end(), id);
    if (!RegisterHotKey(nullptr, id, k.mods | MOD_NOREPEAT, k.vk)) {
        if (taken == takenKeys_.end()) {
            LogError("Could not register ", k.text, ", the key for ", KeyName(id),
                     ": another program is using it. That action does nothing until you pick "
                     "another key in settings.json.");
            takenKeys_.push_back(id);
        }
        return false;
    }
    if (taken != takenKeys_.end()) takenKeys_.erase(taken);
    registeredKeys_.push_back(id);
    return true;
}

// Gives key `id` back to Windows (and so to the game), if Lexia holds it.
void App::UnregisterKey(int id) {
    const auto it = std::find(registeredKeys_.begin(), registeredKeys_.end(), id);
    if (it == registeredKeys_.end()) return;
    UnregisterHotKey(nullptr, id);
    registeredKeys_.erase(it);
}

// KeyWanted's `speaking`. Without the window no message says when the Reading queue turns busy or
// idle, so the Pause and Speech keys are then held all the time; else they would never work.
bool App::Speaking() const { return !window_ || speaker_.Active(); }

// The Pause and Speech keys follow the Reading queue: taken while it holds something, given back
// when it empties (or Lexia goes Off). Safe to call at any time; it only does what is missing.
void App::SyncSpeechKeys() {
    for (int id : {HK_PAUSE, HK_SPEECH}) {
        if (KeyWanted(id, settings_.enabled, Speaking())) {
            TryRegister(id);
        } else {
            UnregisterKey(id);
        }
    }
}

void App::UnregisterKeys() {
    for (int id : registeredKeys_) UnregisterHotKey(nullptr, id);
    registeredKeys_.clear();
    takenKeys_.clear();
}

void App::PrintHelp() const {
    // An action without a key (it clashed in settings.json) is listed as such, not skipped
    // silently, so the user can see why it does nothing.
    auto keyLine = [this](int id, const Hotkey& k, const char* what) {
        const bool taken = std::find(takenKeys_.begin(), takenKeys_.end(), id) != takenKeys_.end();
        return "  " + k.Label() + (taken ? " (taken by another program)" : "") + "  " + what + "\n";
    };
    const std::vector<Language>& langs = AllLanguages();
    if (!settings_.enabled) {
        LogInfo("Lexia is Off (\"enabled\": false in ", SettingsPath(),
                "): no key works until it is turned on.");
    }
    std::string help = "Lexia is running.  Language: " + langs[langIdx_].name + "\n";
    help += keyLine(HK_READ, settings_.read, "open the overlay: click or drag to read, as many times as you like");
    if (settings_.read.IsBound()) {
        help += "      (" + settings_.read.text + " again or right-click leaves the overlay)\n";
    }
    help += keyLine(HK_PAUSE, settings_.pause, "pause / resume reading (only while Lexia is speaking)");
    help += keyLine(HK_SPEECH, settings_.speech, "skip a Block (tap 1 or 2 times: that many Skips); tap 3 times to stop (only while speaking)");
    help += keyLine(HK_LANG, settings_.switchLanguage, "switch Language");
    LogInfo(help, "  Settings: ", SettingsPath());
    for (const Language& l : langs) {
        if (!OcrEngine::ModelsPresent(l)) {
            LogInfo("  (", l.name, " models not installed yet; see the setup notes.)");
        }
    }
}


// The Read key (F8): one Session. Freeze the Active monitor in a Shot, then RunOverlay shows it and
// returns only when the Session ends; meanwhile each Pick calls onSelect (on this thread) and is
// queued for speech.
void App::OpenOverlay() {
    if (!settings_.enabled) return;
    speaker_.SessionStarted();  // speech goes on if it is playing; a paused Reading is dropped

    const RECT mon = ActiveMonitorRect();
    Shot shot;
    if (!shot.Capture(mon)) {
        LogError("Screen capture failed.");
        speaker_.Announce(strings::id::kAnnounceCaptureFailed);
        return;
    }
    const cv::Mat bgr = shot.ToBgr();
    LogInfo("\n[overlay on monitor at ", shot.ox, ",", shot.oy, "  ", shot.w, "x", shot.h, "]");

    OverlayStyle style;
    style.hoverColor = settings_.hoverColor;
    style.pickedColor = settings_.pickedColor;
    style.pauseKey = settings_.pause.IsBound() ? settings_.pause.text : std::string();  // "" = no key: the tooltip shows the word
    style.speechKey = settings_.speech.IsBound() ? settings_.speech.text : std::string();
    style.interfaceLanguage = settings_.interfaceLanguage;
    style.theme = settings_.theme;
    style.barSize = settings_.controlBar.size;
    style.barX = settings_.controlBar.x;
    style.barY = settings_.controlBar.y;
    style.leftHanded = settings_.leftHanded;  // read once per Session: a change applies at the next one

    // Runs on a JobQueue thread; RunOverlay waits for the jobs before `bgr` goes away. The
    // engine is fetched per call, so the Analysis re-run after a Language switch uses the new one.
    // The layout knobs are copied: a settings reload during the Session must not race the jobs.
    auto recognize = [&, layout = settings_.layout](Rect area, bool isDrag) {
        const std::shared_ptr<OcrEngine> engine = Engine();
        return RecognizeBlocks(*engine, bgr, area, isDrag, layout);
    };

    OverlayCallbacks cb;
    // Called on this thread for every click/drag while the overlay is open.
    cb.onSelect = [&](const Selection& sel, const Recognition& picked) -> bool {
        const std::string text = BlocksToText(picked.blocks);

        LogInfo("\n[", sel.kind == SelKind::Drag ? "area" : "block", ", OCR ", picked.ms, " ms]");
        // The recognised text is for the person at the console only: the log gets
        // attached to bug reports and must never contain it (ADR-0005).
        ConsolePrint(text);

        if (Trim(text).empty()) {  // only spaces, tabs or line ends: nothing to say (R11)
            LogInfo("(no text found there)");
            speaker_.Announce(strings::id::kAnnouncePickNoText);
            return false;
        }
        // One Reading of the Pick's Blocks, so Skip goes to the next Block and each Block gets
        // its own main Voice; queued after earlier Picks.
        std::vector<std::string> blocks;
        for (const TextBlock& b : picked.blocks) blocks.push_back(b.text);
        speaker_.SpeakReading(blocks, /*append=*/true);
        return true;
    };
    cb.onPause = [&]() { TogglePause(); };
    cb.onSkip = [&]() { Skip(); };
    cb.onStop = [&]() { Stop(); };
    cb.speechState = [&]() { return SpeechState{speaker_.Active(), speaker_.Paused()}; };
    cb.onBarChanged = [&](BarSize size, int x, int y) { SaveControlBar(size, x, y); };
    cb.onSpeechKey = [&]() { PressSpeechKey(); };
    cb.onSwitchLanguage = [&]() { SwitchLanguage(); };
    cb.onHelp = [&]() { OpenSettingsWindow("help"); };  // RunOverlay calls it after the Overlay is gone
    cb.languageEpoch = &languageEpoch_;

    RunOverlay(shot, recognize, jobs_, style, cb);
}


// ------------------------------------------------------------------- tray (D2) ---

// The tray icon shows On / Off; it is added the first time (the icon needs the App window).
void App::UpdateTray() {
    if (!window_) return;
    const std::wstring tip =
        Utf8ToWide(strings::Text(settings_.interfaceLanguage, TrayTooltipId(settings_.enabled)));
    if (tray_.Added()) {
        tray_.Update(settings_.enabled, tip);
    } else {
        tray_.Add(window_, kMsgTray, settings_.enabled, tip);
    }
}

void App::OnTaskbarCreated() {
    // Explorer restarted and forgot every icon.
    if (!window_) return;
    tray_.Add(window_, kMsgTray, settings_.enabled,
              Utf8ToWide(strings::Text(settings_.interfaceLanguage, TrayTooltipId(settings_.enabled))));
}

// The tray icon's events (NOTIFYICON_VERSION_4): the event is in LOWORD(lp), and for the menu the
// point in screen coordinates is in wp.
void App::OnTrayMessage(WPARAM wp, LPARAM lp) {
    switch (LOWORD(lp)) {
    case WM_CONTEXTMENU:
        ShowTrayMenu(POINT{GET_X_LPARAM(wp), GET_Y_LPARAM(wp)});
        break;
    case WM_LBUTTONDBLCLK:
    case NIN_KEYSELECT:
        OpenSettingsWindow();  // double-click, or Enter on the focused icon; one click does nothing
        break;
    }
}

void App::ShowTrayMenu(POINT at) {
    const std::vector<TrayMenuItem> items = TrayMenuItems(settings_.enabled);
    std::vector<std::wstring> labels;
    for (const TrayMenuItem& item : items) {
        labels.push_back(Utf8ToWide(strings::Text(settings_.interfaceLanguage, item.textId)));
    }
    switch (TrayIcon::ShowMenu(window_, at, items, labels)) {
    case TrayCommand::On: SetOn(true); break;
    case TrayCommand::Off: SetOn(false); break;
    case TrayCommand::Open: OpenSettingsWindow(); break;
    case TrayCommand::Quit: Quit(); break;
    case TrayCommand::None: break;
    }
}
