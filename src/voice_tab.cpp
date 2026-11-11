#include "voice_tab.h"

#include <windows.h>

#include <shlobj.h>
#include <shobjidl.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>

#include "common.h"
#include "language.h"
#include "log.h"
#include "net/winhttp_fetch.h"
#include "strings/string_ids.h"
#include "strings/strings.h"
#include "tts/piper_voice.h"
#include "tts/preset_file.h"
#include "tts/voice_download.h"

namespace {

// The page's control name -> the control.
bool ControlFromName(const std::string& name, VoiceControl& out) {
    if (name == "speed") out = VoiceControl::Speed;
    else if (name == "pitch") out = VoiceControl::Pitch;
    else if (name == "volume") out = VoiceControl::Volume;
    else return false;
    return true;
}

// What a native file dialog is asked: one file type, where to start and what to suggest.
struct FileDialog {
    bool save = false;               // a Save dialog (asks before it replaces a file), else an Open dialog
    std::wstring title;
    std::wstring filterName;         // "Voice files (.zip, .onnx, .onnx.json)"
    std::wstring filterSpec;         // "*.zip;*.onnx;*.onnx.json"
    std::filesystem::path folder;    // opens here; empty = Windows' choice
    std::wstring defaultName;        // Save: the suggested file name, without the extension
    std::wstring defaultExtension;   // Save: added when the name has none ("json")
};

// The native file dialog, owned by the Settings window. An empty path = cancelled or failed.
std::filesystem::path ChooseFile(HWND owner, const FileDialog& d) {
    std::filesystem::path result;
    IFileDialog* dialog = nullptr;
    if (FAILED(CoCreateInstance(d.save ? CLSID_FileSaveDialog : CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&dialog))) ||
        !dialog) {
        LogWarn("[voice] The file dialog could not be created.");
        return result;
    }
    const COMDLG_FILTERSPEC filter[] = {{d.filterName.c_str(), d.filterSpec.c_str()}};
    DWORD options = 0;
    dialog->GetOptions(&options);
    options |= FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | (d.save ? FOS_OVERWRITEPROMPT : FOS_FILEMUSTEXIST);
    dialog->SetOptions(options);
    dialog->SetFileTypes(1, filter);
    dialog->SetTitle(d.title.c_str());
    if (!d.defaultExtension.empty()) dialog->SetDefaultExtension(d.defaultExtension.c_str());
    if (!d.defaultName.empty()) dialog->SetFileName(d.defaultName.c_str());
    if (!d.folder.empty()) {
        IShellItem* folder = nullptr;
        if (SUCCEEDED(SHCreateItemFromParsingName(d.folder.c_str(), nullptr, IID_PPV_ARGS(&folder))) && folder) {
            dialog->SetFolder(folder);
            folder->Release();
        }
    }
    if (SUCCEEDED(dialog->Show(owner))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item)) && item) {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path) {
                result = path;
                CoTaskMemFree(path);
            }
            item->Release();
        }
    }
    dialog->Release();
    return result;
}

}  // namespace

VoiceTab::VoiceTab(Settings& settings, Settings& fileSettings, Speaker& speaker, Host host)
    : settings_(settings), fileSettings_(fileSettings), speaker_(speaker), host_(std::move(host)) {}

VoiceTab::~VoiceTab() {
    // A running download or catalogue fetch stops at once: the flag, and the closing of the WinHTTP request that a
    // blocked connect or receive waits on (FetchCancel), so Quit never waits for the network timeouts.
    // Cancel() is safe whether or not a fetch runs (it only closes a handle a running fetch has registered).
    workCancel_.Cancel();
    catalogueCancel_.Cancel();
    if (catalogueThread_.joinable()) catalogueThread_.join();
    if (workThread_.joinable()) workThread_.join();
}

// ------------------------------------------------------------------- state ---

VoiceModel VoiceTab::Model() const {
    const VoiceFolders folders = DefaultVoiceFolders();
    VoiceModel m;
    m.installed = ListInstalledVoices(folders.voices, folders.userVoices);
    m.activeVoices = settings_.activeVoices;
    m.controls = settings_.voices;
    m.languageId = host_.languageId ? host_.languageId() : DefaultLanguage().id;
    return m;
}

void VoiceTab::FillState(UiState& state) const { FillVoiceState(state, Model()); }

std::string VoiceTab::ActiveVoice() const {
    const VoiceModel m = Model();
    const Language* lang = FindLanguage(m.languageId);
    return lang ? ActiveVoiceName(m, *lang) : std::string();
}

// Gives the Speaker the current controls of every Voice and the chosen Voice of each Language (the
// Speaker knows Languages by code, settings.json by id).
void VoiceTab::Apply() {
    speaker_.SetVoiceControls(settings_.voices);
    std::map<std::string, std::string> byCode;
    for (const auto& [id, voice] : settings_.activeVoices) {
        if (const Language* lang = FindLanguage(id)) byCode[lang->code] = voice;
    }
    speaker_.SetActiveVoices(std::move(byCode));
}

void VoiceTab::SettingsApplied() { Apply(); }  // the controls or the choice of Voices may have changed in settings.json

// Saves the "voices" section (the controls of every Voice); fileSettings_ follows our own save.
void VoiceTab::SaveVoices() {
    if (SaveSettingValue("voices", VoiceControlsToJson(settings_.voices))) fileSettings_.voices = settings_.voices;
}

void VoiceTab::SaveActiveVoices() {
    if (SaveSettingValue("activeVoices", ActiveVoicesToJson(settings_.activeVoices))) {
        fileSettings_.activeVoices = settings_.activeVoices;
    }
}

// ---------------------------------------------------------------- messages ---

bool VoiceTab::Handle(const PageMessage& m) {
    switch (m.type) {
    case PageMessageType::SetVoice: SetVoice(m); break;
    case PageMessageType::SetControl: SetControl(m, false); break;
    case PageMessageType::ResetControl: SetControl(m, true); break;
    case PageMessageType::DeleteVoice: DeleteVoice(m.voice); break;
    case PageMessageType::SavePresetFile: SavePresetFile(); break;
    case PageMessageType::LoadPresetFile: LoadPresetFile(); break;
    case PageMessageType::OpenCatalogue: OpenCatalogue(); break;
    case PageMessageType::Download: Download(m.name); break;
    case PageMessageType::CancelDownload: CancelDownload(m.name); break;
    case PageMessageType::AddFromFile: AddFromFile(); break;
    default: return false;
    }
    return true;
}

void VoiceTab::SetVoice(const PageMessage& m) {
    const Language* lang = FindLanguage(m.language);
    const VoiceModel model = Model();
    bool known = false;
    if (lang && lang->offered) {
        for (const InstalledVoice& v : model.installed) known = known || (v.name == m.voice && v.languageCode == lang->code);
    }
    if (!known) {
        LogWarn("[voice] The page chose a Voice that is not installed: ", m.voice);
    } else if (settings_.activeVoices[lang->id] != m.voice) {
        settings_.activeVoices[lang->id] = m.voice;
        SaveActiveVoices();
        Apply();
    }
    host_.pushState();  // also when nothing changed: the page draws only the state
}

void VoiceTab::SetControl(const PageMessage& m, bool reset) {
    VoiceControl control;
    const std::string voice = ActiveVoice();
    if (!voice.empty() && ControlFromName(m.name, control)) {
        VoiceControls c = settings_.ControlsFor(voice);
        if (reset) {
            ResetControl(c, control);
        } else {
            Set(c, control, static_cast<float>(m.value));  // clamped into the range
        }
        SetControls(voice, c);
    }
    host_.pushState();
}

// The Voice's current controls: kept in the settings, saved, and given to the Speaker at once.
void VoiceTab::SetControls(const std::string& voice, const VoiceControls& c) {
    if (settings_.ControlsFor(voice) == c && settings_.voices.count(voice)) return;
    settings_.voices[voice] = c;
    SaveVoices();
    speaker_.SetVoiceControls(settings_.voices);
}

// Deletes a Voice the user added. A Voice that is not one of those (a shipped one, or none at all) is
// refused; the page does not offer it, so this is only a message that did not come from the page's
// buttons. The refusal's problem is Busy, which the page shows as its general text.
void VoiceTab::DeleteVoice(const std::string& voice) {
    const VoiceModel model = Model();
    const auto found = std::find_if(model.installed.begin(), model.installed.end(),
                                    [&](const InstalledVoice& v) { return v.name == voice; });
    if (found == model.installed.end() || found->shipped) {
        LogWarn("[voice] Delete Voice \"", voice, found == model.installed.end() ? "\": no such Voice is installed."
                                                                                : "\": it is not a Voice the user added.");
        Tell([&](VoiceEventSink& e) { e.VoiceDeleteFailed(voice, VoiceProblem::Busy); });
        host_.pushState();
        return;
    }
    const std::filesystem::path dir = UserVoicesDir();
    if (dir.empty()) {
        LogWarn("[voice] Delete Voice \"", voice, "\": no %LOCALAPPDATA% to find it in.");
        Tell([&](VoiceEventSink& e) { e.VoiceDeleteFailed(voice, VoiceProblem::CannotWrite); });
        host_.pushState();
        return;
    }

    // A Language that speaks this Voice goes to another one of its Voices first, then the Speaker lets go
    // of every loaded Voice (a loaded Voice keeps its model file open, and the files could not go).
    const std::map<std::string, std::string> before = settings_.activeVoices;
    std::vector<InstalledVoice> remaining = model.installed;
    remaining.erase(std::remove_if(remaining.begin(), remaining.end(), [&](const InstalledVoice& v) { return v.name == voice; }),
                    remaining.end());
    for (const Language& lang : AllLanguages()) {
        if (!lang.offered || ActiveVoiceName(model, lang) != voice) continue;
        if (const InstalledVoice* next = PickVoice(remaining, lang.code, std::string())) {
            settings_.activeVoices[lang.id] = next->name;
        } else {
            settings_.activeVoices.erase(lang.id);
        }
    }
    Apply();
    speaker_.ReleaseVoices();

    const VoiceOutcome outcome = RemoveVoice(dir, voice);
    if (!outcome.ok()) {
        LogWarn("[voice] The Voice ", voice, " could not be deleted (", ProblemId(outcome.problem), "): ", outcome.detail);
        settings_.activeVoices = before;
        Apply();
        Tell([&](VoiceEventSink& e) { e.VoiceDeleteFailed(voice, outcome.problem); });
        host_.pushState();
        return;
    }
    LogInfo("[voice] Deleted the Voice ", voice, ".");
    if (settings_.activeVoices != before) SaveActiveVoices();
    if (settings_.voices.erase(voice) > 0) SaveVoices();
    Apply();
    host_.pushState();
}

// Saves the current controls of the active Voice to a Preset file the person names. A cancelled dialog
// sends nothing to the page.
void VoiceTab::SavePresetFile() {
    const std::string voice = ActiveVoice();
    if (voice.empty()) {
        Tell([](VoiceEventSink& e) { e.PresetFailed(PresetProblem::NoVoice, std::string()); });
        return;
    }
    const std::filesystem::path folder = PresetsDir(LexiaDataDir());
    if (!folder.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(folder, ec);  // a failure only means the dialog opens elsewhere
    }
    const std::string& lang = settings_.interfaceLanguage;
    FileDialog d;
    d.save = true;
    d.title = Utf8ToWide(strings::Text(lang, strings::id::kWindowPresetSaveDialogTitle));
    d.filterName = Utf8ToWide(strings::Text(lang, strings::id::kWindowPresetFilter));
    d.filterSpec = L"*.json";
    d.folder = folder;
    d.defaultName = Utf8ToWide(voice);
    d.defaultExtension = L"json";
    const std::filesystem::path path = ChooseFile(host_.owner ? host_.owner() : nullptr, d);
    if (path.empty()) return;

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << PresetToJson(settings_.ControlsFor(voice), voice);
    out.close();
    if (!out) {
        LogWarn("[voice] The Preset file could not be written: ", path);
        Tell([](VoiceEventSink& e) { e.PresetFailed(PresetProblem::CannotWrite, std::string()); });
        return;
    }
    LogInfo("[voice] Saved the controls of ", voice, " to a Preset file.");
    Tell([&](VoiceEventSink& e) { e.PresetSaved(path.stem().u8string()); });
}

// Loads a Preset file the person chooses: its controls go to the Voice the file names (the active Voice when it
// names none), unless that Voice is not installed.
void VoiceTab::LoadPresetFile() {
    const std::string voice = ActiveVoice();
    if (voice.empty()) {
        Tell([](VoiceEventSink& e) { e.PresetFailed(PresetProblem::NoVoice, std::string()); });
        return;
    }
    const std::string& lang = settings_.interfaceLanguage;
    FileDialog d;
    d.title = Utf8ToWide(strings::Text(lang, strings::id::kWindowPresetLoadDialogTitle));
    d.filterName = Utf8ToWide(strings::Text(lang, strings::id::kWindowPresetFilter));
    d.filterSpec = L"*.json";
    d.folder = PresetsDir(LexiaDataDir());
    const std::filesystem::path path = ChooseFile(host_.owner ? host_.owner() : nullptr, d);
    if (path.empty()) return;

    constexpr std::uintmax_t kMaxPresetFileBytes = 64 * 1024;  // a Preset file is a few lines
    std::string text;
    if (!ReadFileToString(path, &text, kMaxPresetFileBytes)) {
        LogWarn("[voice] The Preset file could not be read: ", path);
        Tell([](VoiceEventSink& e) { e.PresetFailed(PresetProblem::BadFile, std::string()); });
        return;
    }
    const PresetFileOutcome loaded = ParsePresetFile(text);
    if (!loaded.ok()) {
        LogWarn("[voice] The file is not a Preset file (", PresetFileProblemId(loaded.problem), "): ", path);
        Tell([](VoiceEventSink& e) { e.PresetFailed(PresetProblem::BadFile, std::string()); });
        return;
    }
    // The file names the Voice it was saved for: its controls go to that Voice if it is here (the active Voice
    // stays as it is), and a Voice that is not installed changes nothing. The name is the file's own text, so
    // it is only shown and logged, shortened, and never used as a path (ChoosePresetTarget only compares it
    // with the installed names).
    const PresetTarget target = ChoosePresetTarget(loaded.voice, voice, InstalledNames());
    if (target.kind == PresetTargetKind::NotInstalled) {
        const std::string shown = PresetVoiceForDisplay(target.voice);
        LogWarn("[voice] The Preset file is for the Voice \"", shown, "\", which is not installed: nothing was changed.");
        Tell([&](VoiceEventSink& e) { e.PresetFailed(PresetProblem::VoiceNotInstalled, shown); });
        return;
    }
    const bool active = target.kind == PresetTargetKind::ActiveVoice;
    LogInfo("[voice] Loaded a Preset file for ", target.voice, active ? "." : " (not the active Voice).");
    SetControls(target.voice, loaded.controls);
    host_.pushState();
    Tell([&](VoiceEventSink& e) { e.PresetLoaded(target.voice, active); });
}

// --------------------------------------------------------- the Voice library ---

std::set<std::string> VoiceTab::InstalledNames() const {
    std::set<std::string> names;
    for (const InstalledVoice& v : Model().installed) names.insert(v.name);
    return names;
}

// Joins a worker thread that has finished; a running one is left alone.
void VoiceTab::Join(std::thread& t, const std::atomic<bool>& running) {
    if (t.joinable() && !running) t.join();
}

void VoiceTab::OpenCatalogue() {
    Join(catalogueThread_, catalogueRunning_);
    if (catalogueRunning_) return;  // the answer is on its way
    catalogueRunning_ = true;
    catalogueCancel_.Reset();  // the previous thread has finished (Join above), so nothing uses it now
    catalogueThread_ = std::thread([this] {
        std::error_code ec;
        const std::filesystem::path temp = std::filesystem::temp_directory_path(ec);
        // The catalogue fetch has no progress to refuse, so it is stopped by FetchCancel (the flag, and the
        // closing of the request that a blocked connect or receive waits on).
        const FetchFn fetch = [this](const std::string& url, const std::filesystem::path& to, uint64_t maxBytes,
                                     const std::function<bool(uint64_t)>& onBytes, std::string* error) {
            return WinHttpFetch(url, to, maxBytes, onBytes, error, &catalogueCancel_);
        };
        const CatalogueResult result = FetchVoiceCatalogue(fetch, temp);
        if (catalogueCancel_.Cancelled()) {  // Quit (or the window ending): no answer is wanted
            LogInfo("[voice] The Voice catalogue fetch was cancelled.");
            catalogueRunning_ = false;
            return;
        }
        PostUi([this, result] {
            if (!result.error.ok()) {
                LogWarn("[voice] The Voice catalogue could not be fetched: ", result.error.detail);
                Tell([](VoiceEventSink& e) { e.CatalogueFailed(VoiceProblem::Network); });
            } else {
                catalogue_ = result.voices;
                Tell([&](VoiceEventSink& e) { e.CatalogueShown(OfferedCatalogue(catalogue_, InstalledNames())); });
            }
        });
        catalogueRunning_ = false;
    });
}

void VoiceTab::Download(const std::string& name) {
    Join(workThread_, workRunning_);
    if (workRunning_) {
        LogWarn("[voice] A download or an add is running; the request for ", name, " was ignored.");
        Tell([&](VoiceEventSink& e) { e.DownloadFailed(name, ProblemFacts{VoiceProblem::Busy, {}, {}}); });
        return;
    }
    const auto it = std::find_if(catalogue_.begin(), catalogue_.end(), [&](const CatalogueVoice& v) { return v.name == name; });
    if (it == catalogue_.end()) {
        Tell([&](VoiceEventSink& e) { e.DownloadFailed(name, ProblemFacts{VoiceProblem::NotInCatalogue, {}, {}}); });
        return;
    }
    const std::filesystem::path dest = UserVoicesDir();
    if (dest.empty()) {
        Tell([&](VoiceEventSink& e) { e.DownloadFailed(name, ProblemFacts{VoiceProblem::CannotWrite, {}, {}}); });
        return;
    }
    workCancel_.Reset();  // the previous worker has finished (Join above)
    downloadName_ = name;
    workRunning_ = true;
    workThread_ = std::thread([this, voice = *it, dest] {
        using Clock = std::chrono::steady_clock;
        Clock::time_point lastPost{};
        const auto progress = [&](uint64_t done, uint64_t total) {
            const Clock::time_point now = Clock::now();
            if (now - lastPost >= std::chrono::milliseconds(100)) {  // about ten a second
                lastPost = now;
                PostUi([this, name = voice.name, done, total] {
                    if (downloadName_ == name) Tell([&](VoiceEventSink& e) { e.DownloadProgress(name, done, total); });
                });
            }
            return !workCancel_.Cancelled();
        };
        // Cancel also closes the request a blocked connect or receive waits on, so it works at once.
        const FetchFn fetch = [this](const std::string& url, const std::filesystem::path& to, uint64_t maxBytes,
                                     const std::function<bool(uint64_t)>& onBytes, std::string* error) {
            return WinHttpFetch(url, to, maxBytes, onBytes, error, &workCancel_);
        };
        const VoiceOutcome outcome = DownloadVoice(voice, fetch, progress, dest);
        PostUi([this, outcome, name = voice.name] {
            downloadName_.clear();
            if (outcome.ok()) {
                LogInfo("[voice] Downloaded the Voice ", name, ".");
                speaker_.VoicesChanged();  // the next Block loads the current files (a Language may have had no Voice)
                Tell([&](VoiceEventSink& e) { e.DownloadDone(name); });
                host_.pushState();
            } else {
                if (outcome.problem != VoiceProblem::Cancelled) {
                    LogWarn("[voice] The download of ", name, " failed (", ProblemId(outcome.problem), "): ", outcome.detail);
                }
                Tell([&](VoiceEventSink& e) { e.DownloadFailed(name, FactsOf(outcome)); });
            }
        });
        workRunning_ = false;
    });
}

void VoiceTab::CancelDownload(const std::string& name) {
    if (workRunning_ && downloadName_ == name) workCancel_.Cancel();
}

void VoiceTab::AddFromFile() {
    Join(workThread_, workRunning_);
    if (workRunning_) {
        LogWarn("[voice] A download or an add is running; Add from file was ignored.");
        Tell([](VoiceEventSink& e) { e.VoiceRefused(ProblemFacts{VoiceProblem::Busy, {}, {}}); });
        return;
    }
    const std::string& lang = settings_.interfaceLanguage;
    FileDialog d;
    d.title = Utf8ToWide(strings::Text(lang, strings::id::kWindowAddFileDialogTitle));
    d.filterName = Utf8ToWide(strings::Text(lang, strings::id::kWindowAddFileFilter));
    d.filterSpec = L"*.zip;*.onnx;*.onnx.json";
    const std::filesystem::path path = ChooseFile(host_.owner ? host_.owner() : nullptr, d);
    if (path.empty()) return;  // cancelled: the page waits for nothing
    AddFile(path.wstring());
}

void VoiceTab::AddFile(const std::wstring& path) {
    const std::filesystem::path dest = UserVoicesDir();
    if (dest.empty()) {
        Tell([](VoiceEventSink& e) { e.VoiceRefused(ProblemFacts{VoiceProblem::CannotWrite, {}, {}}); });
        return;
    }
    workCancel_.Reset();
    downloadName_.clear();
    workRunning_ = true;
    workThread_ = std::thread([this, path, dest] {
        const VoiceOutcome outcome = AddVoice(std::filesystem::path(path), dest);
        PostUi([this, outcome] {
            if (outcome.ok()) {
                LogInfo("[voice] Added the Voice ", outcome.voiceName, ".");
                speaker_.VoicesChanged();  // a newer file of a loaded Voice, or the first Voice of a Language
                Tell([&](VoiceEventSink& e) { e.VoiceAdded(AddedFactsOf(outcome)); });
                host_.pushState();
            } else {
                LogWarn("[voice] A Voice file was refused (", ProblemId(outcome.problem), "): ", outcome.detail);
                Tell([&](VoiceEventSink& e) { e.VoiceRefused(FactsOf(outcome)); });
            }
        });
        workRunning_ = false;
    });
}

// Runs `job` on the UI thread (a worker thread's answer). The page messages and the settings are only
// touched there.
void VoiceTab::PostUi(std::function<void()> job) {
    // A closure still queued when the VoiceTab is destroyed finds the token expired and does nothing.
    host_.runOnUi([weak = std::weak_ptr<bool>(alive_), job = std::move(job)] {
        if (weak.lock()) job();
    });
}

void VoiceTab::Tell(const std::function<void(VoiceEventSink&)>& call) const {
    if (!host_.events) return;
    if (VoiceEventSink* sink = host_.events()) call(*sink);
}
