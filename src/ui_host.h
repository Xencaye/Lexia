#pragma once
// What the native Settings window and the host say to each other, as plain typed data (ADR-0009): the
// `UiState` the window shows, the `PageMessage` command a widget sends (App turns it into an action), the
// fixed links, the theme choice and the Voice tab's part of the state. Pure and Win32-free, so all of it
// is tested. The name is older than the native window: this file was the host's side of the HTML pages'
// JSON bridge, which stream K removed. The Voice library's answers are typed too: voice_events.h.
#include <map>
#include <string>
#include <vector>

#include "language.h"
#include "tts/voice_controls.h"
#include "tts/voice_library.h"

// ------------------------------------------------------------------------- state ---

struct UiKey {
    std::string action;  // "read" | "pause" | "speech"
    std::string key;     // "F8", "Ctrl+Alt+R"; "" = no key
};

// One Voice control as its slider needs it.
struct UiControl {
    double value = 0;
    double min = 0;
    double max = 0;
    double def = 0;
    double step = 0;
};

struct UiVoices {
    std::vector<std::string> installed;
    std::vector<std::string> qualities;  // parallel to `installed`: each Voice's quality, lower case ("" = not stated)
    std::string active;                  // "" = none
    std::vector<std::string> removable;  // the installed Voices the user added (not shipped): they can be deleted
};

// Everything the Settings window draws. A widget keeps no copy of a setting: it shows the last state the
// host set, and the host sets a new one after every change.
struct UiState {
    bool on = true;
    std::string language = "eng";   // a Language id; the window uses it to find voices[language]
    std::string theme = "system";   // the stored choice: "system" | "light" | "dark"
    bool dark = false;              // the effective theme (IsDarkTheme): the window draws with it and the toggle shows it
    std::string version;
    bool runAtStartup = false;
    bool startMinimised = true;     // the stored setting; the window offers it only while runAtStartup is on
    bool runAsAdmin = false;        // the stored setting
    bool elevated = false;          // Lexia runs elevated now (D8.3 fills it)
    bool leftHanded = false;
    bool readAlong = false;         // K9: the read-along bar (G1) on the Overlay and after it (G2); off = no read-along at all
    // K11: the Settings card (a Key action or a switch name, as in ReadKeyCard) that is read aloud now, or "" when
    // none is. Its read button shows the "stop" icon (volume-07) while its card is selected; the host sets it
    // when the reading starts and clears it when it ends or is stopped, however that came about.
    std::string readingCard;
    // Settings::speechVolume, 0..100 %. Steps of 5, like the Voice Volume control (0.05), so the slider
    // moves in visible steps; a value from settings.json that is not on the grid is shown as it is.
    UiControl masterVolume{100, 0, 100, 100, 5};
    std::vector<UiKey> keys;
    std::map<std::string, UiVoices> voices;     // by Language id
    std::map<std::string, UiControl> controls;  // "speed" ...; empty with no Voice
};

// ---------------------------------------------------------------------- commands ---

enum class PageMessageType {
    Unknown,  // the default of a PageMessage nobody filled in; the host ignores it
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
    SetReadAlong,
    SetTheme,
    SetMasterVolume,
    DeleteVoice,
    SavePresetFile,
    LoadPresetFile,
    OpenLink,
    OpenNotices,
};

// The links the window may ask the host to open (`OpenLink`). A widget names a link, never a URL (ADR-0005).
enum class LinkId { Mail, Discord, Github, Itch };

// One command of the Settings window: what a widget callback hands to the host (Screen::Emit). Only the
// fields of its `type` are set; the host checks a value's range against the Voice itself.
struct PageMessage {
    PageMessageType type = PageMessageType::Unknown;
    std::string typeText;  // the command's name (for the log), "" if none
    bool on = false;       // SetOn, SetRunAtStartup, SetStartMinimised, SetRunAsAdmin, SetLeftHanded, SetReadAlong
    std::string theme;     // SetTheme: system | light | dark
    std::string language;  // SetVoice: a Language id
    std::string voice;     // SetVoice, DeleteVoice
    std::string name;      // SetControl / ResetControl: speed|pitch|volume;
                           // Download / CancelDownload: the Voice's name
    double value = 0;      // SetControl; SetMasterVolume (0..100)
    LinkId link = LinkId::Github;  // OpenLink
    std::string action;    // CaptureKey, SetKey, CancelKeyCapture: read | pause | speech;
                           // ReadKeyCard: the card's id, a Key (read | pause | speech) or a switch (see SwitchCardIds)
    std::string key;       // SetKey: the key text, "" = no key
};

// ------------------------------------------------------------------------ cards ---

// The Settings page's card shows what the selected Key or the selected switch does. A card is named by an id:
// a Key's action ("read" | "pause" | "speech") or a switch's name from `SwitchCardIds()`; the page selects by it
// and sends it in `ReadKeyCard`, so the host (which speaks the words) and the page (which draws them) find the
// same text. The words live in strings/: a Key's in `window.keys.<id>` (title) and `window.keys.<id>Desc`, a
// switch's in `window.switches.<id>Title` and `window.switches.<id>Desc`.
// The six switches in the order the page shows them (and the names of their automation ids, "settings.<name>").
const std::vector<std::string>& SwitchCardIds();
bool IsSwitchCard(const std::string& cardId);
// String ids (strings/string_ids.h) of the card's title and of its description (one paragraph per line).
std::string CardTitleId(const std::string& cardId);
std::string CardDescId(const std::string& cardId);

// ------------------------------------------------------------------------ links ---

// The link a new GitHub issue opens: its body holds only the versions ("Lexia 0.5.0, Windows
// 10.0.26300"), percent-encoded. Nothing else is sent (ADR-0005).
std::string BugReportUrl(const std::string& lexiaVersion, const std::string& windowsVersion);

// The fixed URL an `OpenLink` id stands for: mailto:lexia@xencaye.com, the Discord invite, the itch.io
// page, and (Github) the new-issue link of BugReportUrl with the versions given. The host opens it
// only on the user's click (ADR-0005).
std::string LinkUrl(LinkId id, const std::string& lexiaVersion = std::string(),
                    const std::string& windowsVersion = std::string());

// The id as the log writes it: "mail" | "discord" | "github" | "itch".
const char* LinkName(LinkId id);

// ------------------------------------------------------------------------ theme ---

// Is the window dark? `theme` is the setting ("system" | "light" | "dark"); "system" (and anything
// else) follows Windows: `systemUsesLightTheme` is its AppsUseLightTheme.
bool IsDarkTheme(const std::string& theme, bool systemUsesLightTheme);

// ------------------------------------------------------------- the Voice tab (D5) ---

// What the Voice tab's part of the state is made from (plain data; App fills it).
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

// Why a Preset file could not be saved or loaded (the result sheet says it).
// VoiceNotInstalled: the file is for a Voice that is not installed; the event carries its (shortened) name.
enum class PresetProblem { BadFile, CannotWrite, NoVoice, VoiceNotInstalled };
