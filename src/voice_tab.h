#pragma once
// The Voice tab's side of the Settings window (D5, D8.3): Voice choice, Voice controls, Preset files and
// the Voice library (download, add from file, delete). App owns one and hands it the Voice commands of the window.
// The pure parts (the state, the answers) are in ui_host.*; this class is the glue: settings.json,
// the Speaker, worker threads for the network and the file dialog.
//
// Threads: everything public runs on the UI thread. The catalogue fetch and a download run on worker
// threads (one of each at a time) and hand their answers back through Host::runOnUi, which posts to
// the UI thread; an answer for a window that has been closed is dropped (Host::events gives no sink).
// The network is used only when the page asks (ADR-0005).
#include <windows.h>

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "settings.h"
#include "speech.h"
#include "tts/voice_catalogue.h"
#include "ui_host.h"
#include "voice_events.h"

class VoiceTab {
public:
    struct Host {
        std::function<VoiceEventSink*()> events;             // UI thread; the Settings window's sink, nullptr without a window
        std::function<void()> pushState;                      // UI thread
        std::function<HWND()> owner;                          // the Settings window, or nullptr
        std::function<std::string()> languageId;              // the current Language's id
        std::function<void(std::function<void()>)> runOnUi;   // any thread: runs it later on the UI thread
    };

    // `settings` and `fileSettings` are App's: a change is made in `settings` and saved, and
    // `fileSettings` follows a successful save (so the file watcher sees no outside edit).
    VoiceTab(Settings& settings, Settings& fileSettings, Speaker& speaker, Host host);
    ~VoiceTab();  // cancels a download and waits for the workers
    VoiceTab(const VoiceTab&) = delete;
    VoiceTab& operator=(const VoiceTab&) = delete;

    // True if `m` is a Voice tab message (it has been dealt with and the state pushed or answered).
    bool Handle(const PageMessage& m);

    // The Voices and controls of `state`.
    void FillState(UiState& state) const;

    // Tells the Speaker the Voice choice and the Voices' controls. Call at start, and (SettingsApplied)
    // after settings were applied: settings.json may have changed either under the running app.
    void Apply();
    void SettingsApplied();

private:
    VoiceModel Model() const;
    std::string ActiveVoice() const;  // the current Language's active Voice's name, "" if none
    void SaveVoices();        // settings_.voices -> settings.json
    void SaveActiveVoices();  // settings_.activeVoices -> settings.json
    void SetVoice(const PageMessage& m);
    void SetControl(const PageMessage& m, bool reset);
    void SetControls(const std::string& voice, const VoiceControls& c);  // keep, save, tell the Speaker
    void DeleteVoice(const std::string& voice);
    void SavePresetFile();  // the file dialog, then the current controls to a Preset file
    void LoadPresetFile();  // the file dialog, then the file's controls become the current ones
    // Tells the Settings window's Voice library sink (typed events, voice_events.h); nothing happens when no
    // window is open (the next state shows the result, as for a download that finished).
    void Tell(const std::function<void(VoiceEventSink&)>& call) const;
    void OpenCatalogue();
    void Download(const std::string& name);
    void CancelDownload(const std::string& name);
    void AddFromFile();
    void AddFile(const std::wstring& path);
    std::set<std::string> InstalledNames() const;
    void Join(std::thread& t, const std::atomic<bool>& running);
    // Worker threads: runs `job` on the UI thread unless this VoiceTab is gone by then.
    void PostUi(std::function<void()> job);

    Settings& settings_;
    Settings& fileSettings_;
    Speaker& speaker_;
    Host host_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);  // closures posted to the UI thread hold a weak_ptr to it

    std::vector<CatalogueVoice> catalogue_;           // the last catalogue that came

    std::thread catalogueThread_;
    std::atomic<bool> catalogueRunning_{false};
    std::thread workThread_;                          // a download, or adding a file
    std::atomic<bool> workRunning_{false};
    std::atomic<bool> cancel_{false};
    std::string downloadName_;                        // UI thread; the Voice that downloads, "" = none
};
