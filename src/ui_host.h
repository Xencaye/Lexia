#pragma once
// The bridge between the Settings window's page (ui/) and the host: the host's side of the contract
// in ui/README.md. Pure and Win32-free: plain data in, JSON text out, and JSON text in, a checked
// struct out. The Win32 / WebView2 glue (settings_window.*, app.cpp) only carries the text.
//
// Every later ticket (D5 Voice tab, D6 Settings tab) adds its fields and messages here.
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "language.h"
#include "tts/voice_catalogue.h"
#include "tts/voice_controls.h"
#include "tts/voice_library.h"

// ----------------------------------------------------------------- host to page ---

struct UiKey {
    std::string action;  // "read" | "pause" | "speech"
    std::string key;     // "F8", "Ctrl+Alt+R"; "" = no key
};

// One Voice control as the page's slider needs it (README: controls).
struct UiControl {
    double value = 0;
    double min = 0;
    double max = 0;
    double def = 0;
    double step = 0;
};

struct UiVoices {
    std::vector<std::string> installed;
    std::string active;                  // "" = none
    std::vector<std::string> removable;  // the installed Voices the user added (not shipped): they can be deleted
};

// Everything the `state` message says. The page keeps no copy of a setting: it draws this.
struct UiState {
    bool on = true;
    std::string language = "eng";   // a Language id; the page uses it to find voices[language]
    std::string theme = "system";   // the stored choice: "system" | "light" | "dark"
    bool dark = false;              // the effective theme (IsDarkTheme); the page draws and the toggle shows this
    std::string version;
    bool runAtStartup = false;
    bool startMinimised = true;     // the stored setting; the page offers it only while runAtStartup is on
    bool runAsAdmin = false;        // the stored setting
    bool elevated = false;          // Lexia runs elevated now (D8.3 fills it)
    bool leftHanded = false;
    // Settings::speechVolume, 0..100 %. Steps of 5, like the Voice Volume control (0.05), so the slider
    // moves in visible steps; a value from settings.json that is not on the grid is shown as it is.
    UiControl masterVolume{100, 0, 100, 100, 5};
    std::vector<UiKey> keys;
    std::map<std::string, UiVoices> voices;     // by Language id
    std::map<std::string, UiControl> controls;  // "speed" ...; empty with no Voice
};

// The `strings` message: the Interface language's whole table. An unknown code gives English (and
// says "en" in `language`).
std::string StringsMessage(const std::string& interfaceLanguage);

// The `state` message. Never throws: text that is not UTF-8 is replaced, not refused.
std::string StateMessage(const UiState& state);

// ----------------------------------------------------------------- page to host ---

enum class PageMessageType {
    Unknown,  // not JSON, no type, a type Lexia does not know, or a payload that failed its checks
    Ready,
    SetOn,
    SetVoice,
    SetControl,
    ResetControl,
    OpenCatalogue,
    Download,
    CancelDownload,
    AddFromFile,
    CaptureKey,
    ReadKeyCard,
    SetKey,
    CancelKeyCapture,
    SetRunAtStartup,
    SetStartMinimised,
    SetRunAsAdmin,
    SetLeftHanded,
    SetTheme,
    SetMasterVolume,
    DeleteVoice,
    SavePresetFile,
    LoadPresetFile,
    OpenLink,
    OpenNotices,
};

// The links the page may ask the host to open (`openLink`). The page never sends a URL (ADR-0005).
enum class LinkId { Mail, Discord, Github, Itch };

// A checked page message. Only the fields of its `type` are set; each has passed its check, so the
// host may use them without asking again (a value's range against the Voice is the host's to check).
struct PageMessage {
    PageMessageType type = PageMessageType::Unknown;
    std::string typeText;  // the message's "type" as sent (for the log), "" if there was none
    bool on = false;       // SetOn, SetRunAtStartup, SetStartMinimised, SetRunAsAdmin, SetLeftHanded
    std::string theme;     // SetTheme: system | light | dark
    std::string language;  // SetVoice: a Language id
    std::string voice;     // SetVoice, DeleteVoice
    std::string name;      // SetControl / ResetControl: speed|pitch|volume;
                           // Download / CancelDownload: the Voice's name
    double value = 0;      // SetControl; SetMasterVolume (clamped into 0..100)
    LinkId link = LinkId::Github;  // OpenLink
    std::string action;    // CaptureKey, SetKey, CancelKeyCapture, ReadKeyCard: read | pause | speech
    std::string key;       // SetKey: the key text, "" = no key
};

// Parses and checks one message from the page. Never throws.
PageMessage ParsePageMessage(const std::string& json);

// ------------------------------------------------------------------------ links ---

// The link a new GitHub issue opens: its body holds only the versions ("Lexia 0.5.0, Windows
// 10.0.26300"), percent-encoded. Nothing else is sent (ADR-0005).
std::string BugReportUrl(const std::string& lexiaVersion, const std::string& windowsVersion);

// The fixed URL a `openLink` id stands for: mailto:lexia@xencaye.com, the Discord invite, the itch.io
// page, and (Github) the new-issue link of BugReportUrl with the versions given. The host opens it
// only on the user's click (ADR-0005).
std::string LinkUrl(LinkId id, const std::string& lexiaVersion = std::string(),
                    const std::string& windowsVersion = std::string());

// The id as the page sends it: "mail" | "discord" | "github" | "itch".
const char* LinkName(LinkId id);

// ------------------------------------------------------------------------ theme ---

// Is the window dark? `theme` is the setting ("system" | "light" | "dark"); "system" (and anything
// else) follows Windows: `systemUsesLightTheme` is its AppsUseLightTheme.
bool IsDarkTheme(const std::string& theme, bool systemUsesLightTheme);

// ------------------------------------------------------------- the Voice tab (D5) ---

// What the Voice tab's part of `state` is made from (plain data; App fills it).
struct VoiceModel {
    std::vector<InstalledVoice> installed;                 // ListInstalledVoices
    std::map<std::string, std::string> activeVoices;       // Settings::activeVoices (Language id -> Voice name)
    std::map<std::string, VoiceControls> controls;         // Settings::voices (the current controls), by Voice name; absent = the defaults
    std::string languageId = "eng";                        // the current Language
};

// The Voice that speaks Language `lang` (its name), "" if none is installed.
std::string ActiveVoiceName(const VoiceModel& m, const Language& lang);

// Sets `voices` (every offered Language, with the Voices the user may delete) and `controls` (of the
// current Language's active Voice; empty with none).
void FillVoiceState(UiState& state, const VoiceModel& model);

// The Voice library's host-to-page messages (README). A problem is its ProblemId.
std::string CatalogueMessage(const std::vector<CatalogueVoice>& voices, const std::set<std::string>& installedNames);
std::string CatalogueFailedMessage(VoiceProblem problem);
std::string DownloadProgressMessage(const std::string& name, uint64_t done, uint64_t total);
std::string DownloadDoneMessage(const std::string& name);
std::string DownloadFailedMessage(const std::string& name, const VoiceOutcome& outcome);
std::string VoiceAddedMessage(const VoiceOutcome& outcome);  // outcome.ok()
std::string VoiceRefusedMessage(const VoiceOutcome& outcome);

// Why a Preset file could not be saved or loaded (the page shows a result sheet).
enum class PresetProblem { BadFile, CannotWrite, NoVoice };
const char* PresetProblemId(PresetProblem problem);  // "badFile" | "cannotWrite" | "noVoice"

// `voiceDeleteFailed {voice, problem}`: problem is the ProblemId of the VoiceProblem.
std::string VoiceDeleteFailedMessage(const std::string& voice, VoiceProblem problem);

// `presetSaved {name}` / `presetLoaded {name}`: the file's name without the folder. A cancelled
// file dialog sends nothing.
std::string PresetSavedMessage(const std::string& name);
std::string PresetLoadedMessage(const std::string& name);
std::string PresetFailedMessage(PresetProblem problem);
