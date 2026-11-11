#pragma once
// User settings, stored as settings.json in %LOCALAPPDATA%\Lexia (or next to lexia.exe, see
// SettingsPath).
#include <windows.h>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "control_bar_layout.h"
#include "layout.h"
#include "tts/voice_controls.h"

struct Hotkey {
    UINT mods = 0;  // MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_WIN (without MOD_NOREPEAT)
    UINT vk = 0;    // virtual-key code
    std::string text;  // as shown to the user, e.g. "Ctrl+Alt+L"

    // vk == 0 means the action has no key: it clashed with another one (R35). Nothing is
    // registered for it; the console help shows it as "(no key)" (the Overlay translates that).
    bool IsBound() const { return vk != 0; }

    // The key as shown to the user: `text`, or "(no key)" when unbound.
    std::string Label() const { return IsBound() ? text : std::string("(no key)"); }
};

// The Control bar on the Overlay (D4). x and y are offsets in px from the top left corner of the
// monitor the Session is on; -1 = not set (top centre).
struct ControlBarSettings {
    BarSize size = BarSize::Large;
    int x = -1;
    int y = -1;
};

struct Settings {
    Hotkey read = {0, VK_F8, "F8"};
    // Pause key and Speech key are only registered while something is being spoken or paused
    // (hotkeys.h KeyWanted), so a game keeps F7 / F6 while Lexia is silent.
    Hotkey pause = {0, VK_F7, "F7"};
    Hotkey speech = {0, VK_F6, "F6"};  // one tap = Skip, two taps = two Skips, three taps = Stop (speech_key.h)
    Hotkey switchLanguage;  // unbound by default; "Ctrl+Alt+L" in settings.json binds it

    std::string language = "eng";  // Language on startup (a Language id)

    // The Interface language (see GLOSSARY): the language of Lexia's own words. A code from
    // strings::InterfaceLanguages() ("en", "bg"), separate from `language`.
    std::string interfaceLanguage = "en";

    // On / Off (see GLOSSARY). Remembered across restarts. Off = no keys are registered. Off on the
    // very first start (no settings.json yet): the user turns Lexia on when they are ready.
    bool enabled = false;

    // The Settings window (D3). The theme of the window and the Control bar: "system" (follow
    // Windows), "light" or "dark". runAtStartup mirrors the Run key. startMinimised: a start at logon
    // stays in the tray (the Run value gets --minimised); it only means something while runAtStartup is
    // on. A manual start always opens the Settings window.
    std::string theme = "system";
    bool runAtStartup = false;
    bool startMinimised = true;
    bool runAsAdmin = false;   // start Lexia elevated (elevation.h, ADR-0008)
    bool leftHanded = false;   // the right mouse button picks, the left one leaves (mouse_role.h)
    bool readAlong = false; // K9 "enable read-along": off = no read-along at all; on = the G1 bar under the heard word on the Overlay and (G2) after it (read_along_layer.h)

    int speechRate = 0;      // -10 (slow) .. 10 (fast)
    int speechVolume = 100;  // 0 .. 100

    COLORREF hoverColor = RGB(0xFF, 0xC8, 0x00);   // outline of the block under the mouse
    COLORREF pickedColor = RGB(0x50, 0xC8, 0x78);  // outline of areas already read

    ControlBarSettings controlBar;

    LayoutParams layout;  // how Lines are grouped into Blocks; only settings.json changes it

    // Each Voice's current controls, keyed by the Voice's name (the file name in voices.json without
    // ".onnx", e.g. "en_US-ljspeech-medium"). A Voice with no entry speaks with the defaults.
    // settings.json: "voices": { "<name>": { "speed": 1, "pitch": 0, "volume": 1 } }
    // (An older file kept named Presets: { "preset": "<active>", "presets": { "<Preset>": {...} } };
    // reading it, the active Preset's controls become the current ones, with no note.)
    std::map<std::string, VoiceControls> voices;

    // The Voice that speaks each Language, by Language id ("eng") -> Voice name. A Language with no
    // entry, or whose Voice is not installed, speaks with the first installed Voice for it (the
    // shipped one). settings.json: "activeVoices": { "eng": "en_US-ljspeech-medium" }
    std::map<std::string, std::string> activeVoices;

    // Retired Voices that Lexia has already dealt with (AdoptRetiredVoices): a Voice that an older
    // version shipped and is still in the program folder is copied into the user's Voices once; its name
    // is kept here so that deleting the copy on the Voices page sticks (else the next start would copy it
    // back). Sorted, no duplicates. settings.json: "adoptedVoices": ["en_US-ljspeech-medium"]
    std::vector<std::string> adoptedVoices;

    VoiceControls ControlsFor(const std::string& voiceName) const {
        const auto it = voices.find(voiceName);
        return it == voices.end() ? VoiceControls{} : it->second;
    }
};

// Which settings.json to use: <appDataDir>/settings.json if it exists, else <exeDir>/settings.json
// if that exists (a zip copy that was already used before the file moved, or a read-only
// install), else <appDataDir>/settings.json, which will be created. Only looks, never writes.
std::filesystem::path ChooseSettingsFile(const std::filesystem::path& appDataDir,
                                         const std::filesystem::path& exeDir);

// ChooseSettingsFile for %LOCALAPPDATA%\Lexia and the folder of lexia.exe.
std::filesystem::path SettingsPath();

// "F8", "Ctrl+Alt+R", "Shift+F9"... Letters and digits need at least one modifier. (In
// settings.json an empty string "" means "no key"; that is handled by ParseSettings.)
bool ParseHotkey(const std::string& text, Hotkey& out);

// The pure half of LoadSettings: parses the text of a settings.json (// comments allowed).
// Does not touch the file system or the logger. Every problem is appended to `notes` (without
// the "[settings] " prefix LoadSettings adds when it logs them), and the value concerned keeps
// its default, so one bad value never spoils the others. Malformed JSON, or JSON nested more than
// 64 levels deep, gives one note and all defaults. `source` only names the file in that note. `wellFormed`, if given, says whether
// the text was usable JSON at all (false = the defaults above are not what the user wrote).
Settings ParseSettings(const std::string& jsonText, std::vector<std::string>& notes,
                       const std::string& source = "the settings file", bool* wellFormed = nullptr);

// Only what Lexia offers (docs/adr/0007-english-only-for-now.md): a `language` or
// `interfaceLanguage` that Lexia knows but does not offer yet becomes the default, with a note
// (no "[settings] " prefix) for each. The rest of `s` is kept. ParseSettings itself keeps them, so
// it reads a file as it is written; the app calls this on what it is about to apply.
std::vector<std::string> KeepToOffered(Settings& s);

bool operator==(const Hotkey& a, const Hotkey& b);
bool operator==(const Settings& a, const Settings& b);
inline bool operator!=(const Settings& a, const Settings& b) { return !(a == b); }

// The text of a settings.json holding `s`: what LoadSettings writes when there is no file, and
// what ParseSettings reads back.
std::string SettingsToText(const Settings& s);

// The "voices" value of a settings.json as JSON text (an object keyed by Voice name), the
// inverse of the part of ParseSettings that reads it. For saving the Voice controls: a caller that
// keeps the rest of the file replaces just that key with this text.
std::string VoiceControlsToJson(const std::map<std::string, VoiceControls>& voices);

// The "activeVoices" value as JSON text, for SaveSettingValue("activeVoices", ...).
std::string ActiveVoicesToJson(const std::map<std::string, std::string>& activeVoices);

// The "adoptedVoices" value as JSON text (a sorted array of names), for SaveSettingValue("adoptedVoices", ...).
std::string AdoptedVoicesToJson(const std::vector<std::string>& adoptedVoices);

// Reads settings.json and runs it through ParseSettings, logging each note. If the file
// doesn't exist it is created with the defaults. A file that can't be parsed is left untouched.
Settings LoadSettings();

// Only the `runAsAdmin` value of the settings file, read before anything else starts (wWinMain decides
// on the elevated relaunch with it, ADR-0008). Silent: it logs nothing and makes no file; false when there
// is no usable file. LoadSettings reads the file again later and reports its problems.
bool RunAsAdminSetting();

// LoadSettings for a running Lexia: reads settings.json again and logs the notes. Returns false,
// leaving `out` alone and logging why, if the file is missing, unreadable or not JSON; the
// caller then keeps the settings it has. A bad value inside valid JSON is a note and that value's
// default, exactly as at start-up.
bool ReloadSettings(Settings& out);

// The pure half of SaveSettingValue: sets one value in the text of a settings.json and leaves
// every other value, the key order and the indentation as they were. `key` is "name" or
// "section.name"; `valueJson` is the new value as JSON text ("true", "\"bul\""). Returns false
// and sets `note` (text untouched) if the text is not JSON (or is nested more than 64 levels
// deep), is not an object, or contains //
// comments, which rewriting would silently drop.
bool UpdateSettingsText(std::string& text, const std::string& key, const std::string& valueJson,
                        std::string& note);

// Read-modify-write of one key of settings.json, so the user's other values survive. Creates
// the file with the defaults first if it doesn't exist. Logs a warning and returns false if it
// can't be done; the running settings are not affected either way.
bool SaveSettingValue(const std::string& key, const std::string& valueJson);
