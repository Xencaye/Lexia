#pragma once
// The application: the state and the actions that main() used to keep as locals. A hotkey calls
// these methods today; the tray menu and the Settings window will call the same ones.
// Not thread-safe: everything runs on the thread that calls Run(). Only the OCR engine is also
// used by JobQueue threads, and that goes through Engine().
#include <windows.h>

#include <cstddef>
#include <memory>
#include <mutex>
#include <string_view>
#include <vector>

#include "jobqueue.h"
#include "ocr.h"
#include "read_along.h"
#include "read_along_layer.h"
#include "settings.h"
#include "settings_watch.h"
#include "settings_window.h"
#include "tray.h"
#include "ui_host.h"
#include "voice_tab.h"
#include "speech.h"

class App {
public:
    App() = default;
    ~App();
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    // Loads the settings, the speech engine and the OCR models, and registers the keys (unless
    // Off). False = Lexia can't run; the reason has been logged.
    bool Init();

    // Waits for hotkeys until Quit(). Returns the process exit code.
    int Run();

    // The actions.
    void OpenOverlay();       // starts a Session on the Active monitor; returns when it ends
    void Stop();              // Stop speaking
    void Skip();              // Skip the Block being read (the Control bar's Skip; the Speech key taps go through PressSpeechKey)
    void TogglePause();       // the Pause key: pauses the Reading queue, or resumes it
    void PressSpeechKey();    // the Speech key: a tap is Skip, a second tap in time is Stop
    void SaveControlBar(BarSize size, int x, int y);  // remembers the Control bar's size and place
    void SwitchLanguage();    // to the next Language whose models are installed; saved
    void ApplySettings(const Settings& s);  // new keys, speech, colours and Language take effect
    void SetOn(bool on);      // On / Off; remembered in settings.json
    void Quit();              // makes Run() return (the tray menu calls it)
    // Opens the Settings window, or brings the open one to the front. `page` (home|voices|settings|report|help|
    // about; "" = Home) is the page it shows (an unknown name is logged and shows Home); an open window switches to it.
    void OpenSettingsWindow(std::string_view page = {});
    void PushState();           // gives the Settings window the whole state (SetState); nothing if it is not open

    void OnSettingsFileChanged();  // the debounced reload; public for the window procedure
    void SyncSpeechKeys();         // the Pause and Speech keys follow whether Lexia is speaking
    void OnSpeechKeyTimer();       // the Speech key's tap window ran out
    void OnSettingsWindowClosed();  // the window is gone: drop it (a posted message, not in its own callback)
    void OnWordMarkEnded();         // the word mark layer ended by itself: delete it (a posted message, not in its own callback)
    void OnTrayMessage(WPARAM wp, LPARAM lp);  // an event of the tray icon
    void OnTaskbarCreated();                   // Explorer restarted: the tray icon is put back

private:
    // The word mark layer (G2, readAlong): a click-through window that goes on drawing the bar under the
    // heard word after a Session ends, until the Session's Readings are done. At most one.
    void StartWordMarkLayer(const RECT& mon, ReadAlongTrack track);
    void EndWordMarkLayer(bool flush);  // removes it; flush = wait until the screen is drawn without it (before a Shot)
    bool RegisterKeys();
    bool TryRegister(int id);  // registers key `id` if it is wanted now; false if another program has it
    void UnregisterKey(int id);
    void UnregisterKeys();
    bool Speaking() const;  // the Reading queue is busy, or there is no window to say when it is
    const Hotkey& KeyFor(int id) const;
    void ArmSpeechKeyTimer(long long ms);  // 0 = no Skip is waiting
    bool UseLanguage(size_t idx);
    std::shared_ptr<OcrEngine> Engine();
    void PrintHelp() const;
    bool CreateMessageWindow();
    UiState BuildUiState() const;
    void HandlePageMessage(const PageMessage& m);  // a user action of the Settings window (typed, ui_host.h)
    void ApplyWindowTheme();  // the Settings window's paper follows the theme setting
    void UpdateTray();        // the tray icon's On / Off look and tooltip follow the state
    void ShowTrayMenu(POINT at);
    void OpenNotices();
    void OpenLink(LinkId id);  // the page asked for one of the fixed links (ShellExecute)
    void CaptureKey();     // the page waits for a key: Lexia's keys are released meanwhile
    void ReadKeyCard(const std::string& cardId);  // speaks what that Key or switch does (the card's play button)
    void EndKeyCapture();  // the wait is over (cancelled, the page reloaded, or the window went)
    void SetKey(const std::string& action, const std::string& key);  // checks, saves, applies; always PushState
    void SetRunAtStartup(bool on);  // the Run key is the truth; the setting follows it
    void SetStartMinimised(bool on);  // saves it; rewrites the Run value while Run at startup is on
    void SetBoolSetting(const char* key, bool on, bool& inForce, bool& inFile);  // remembers a switch, shows it in the state
    void SetRunAsAdmin(bool on);  // saves it; turned on while not elevated -> relaunch elevated (ADR-0008)
    void SetMasterVolume(int volume);
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    Settings settings_;      // what is in force: keys that could not be registered are unbound
    Settings fileSettings_;  // what settings.json said last, to tell a real edit from a re-save
    Speaker speaker_;
    std::unique_ptr<ReadAlongLayer> wordMarkLayer_;  // after speaker_: its callbacks ask the Speaker, so it is destroyed first

    // The engine is replaced as a whole when the Language changes (a failed load leaves the
    // working one alone). A job takes its own shared_ptr when it starts, so the engine it is
    // using stays alive, and unchanged, until it is done.
    std::mutex ocrMutex_;
    std::shared_ptr<OcrEngine> ocr_;
    JobQueue jobs_;  // after ocr_: its threads use the engine, so they stop first

    size_t langIdx_ = 0;                // into AllLanguages()
    unsigned languageEpoch_ = 0;        // +1 whenever the Language changes; a Session watches it
    std::vector<int> registeredKeys_;   // HotkeyIds currently registered
    std::vector<int> takenKeys_;        // HotkeyIds RegisterHotKey refused (another program has them)
                                        // and already reported, so a retry stays quiet
    bool quit_ = false;
    bool capturingKey_ = false;         // the Settings window waits for a key: none of ours is registered

    // Hidden window that the settings watcher notifies. It lives on the UI thread, so its
    // messages are dispatched by Run() and by a Session's loop alike.
    HWND window_ = nullptr;
    std::unique_ptr<SettingsWatcher> watcher_;  // before window_ is destroyed, see ~App

    // The Settings window (D3, native since K.2): made on demand, destroyed when closed. It gets the whole
    // state (PushState) right after it is made and after every change.
    std::unique_ptr<SettingsWindow> settingsWindow_;

    TrayIcon tray_;  // D2: added once the hidden window exists, removed in Run() when it ends
    // The Voice tab (D5). After settingsWindow_ and speaker_: it is destroyed first (its workers post to the window).
    std::unique_ptr<VoiceTab> voiceTab_;
};
