#include "settings.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <vector>

#include "common.h"
#include "language.h"
#include "log.h"
#include "strings/strings.h"

using json = nlohmann::ordered_json;  // keeps the keys in the order we write them
namespace fs = std::filesystem;

// %LOCALAPPDATA%\Lexia\settings.json, unless only the exe folder has one (an unzipped, portable
// Lexia whose user put the file beside the exe).
std::filesystem::path ChooseSettingsFile(const fs::path& appDataDir, const fs::path& exeDir) {
    std::error_code ec;
    if (!fs::exists(appDataDir / "settings.json", ec) && fs::exists(exeDir / "settings.json", ec)) {
        return exeDir / "settings.json";
    }
    return appDataDir / "settings.json";
}

// The settings.json that Lexia reads and writes on this PC.
fs::path SettingsPath() {
    const fs::path dataDir = LexiaDataDir();
    if (dataDir.empty()) return ExeDir() / "settings.json";  // no profile folder
    return ChooseSettingsFile(dataDir, ExeDir());
}

// ------------------------------------------------------------------ hotkeys ---

static UINT KeyFromName(const std::string& n) {  // n is lower case
    if (n.size() >= 2 && n.size() <= 3 && n[0] == 'f') {
        const std::string digits = n.substr(1);
        if (std::all_of(digits.begin(), digits.end(),
                        [](unsigned char c) { return std::isdigit(c) != 0; })) {
            const int num = std::stoi(digits);
            if (num >= 1 && num <= 24) return static_cast<UINT>(VK_F1 + (num - 1));
        }
    }
    if (n.size() == 1) {
        const char c = n[0];
        if (c >= 'a' && c <= 'z') return static_cast<UINT>(c - 'a' + 'A');
        if (c >= '0' && c <= '9') return static_cast<UINT>(c);
    }
    static const std::map<std::string, UINT> names = {
        {"space", VK_SPACE},   {"tab", VK_TAB},        {"enter", VK_RETURN},
        {"return", VK_RETURN}, {"insert", VK_INSERT},  {"ins", VK_INSERT},
        {"delete", VK_DELETE}, {"del", VK_DELETE},     {"home", VK_HOME},
        {"end", VK_END},       {"pageup", VK_PRIOR},   {"pagedown", VK_NEXT},
        {"up", VK_UP},         {"down", VK_DOWN},      {"left", VK_LEFT},
        {"right", VK_RIGHT},   {"pause", VK_PAUSE},    {"scrolllock", VK_SCROLL},
        {"printscreen", VK_SNAPSHOT},
    };
    const auto it = names.find(n);
    return it == names.end() ? 0 : it->second;
}

// Keys that are safe to take over system-wide without a modifier.
static bool SafeAlone(UINT vk) {
    return (vk >= VK_F1 && vk <= VK_F24) || vk == VK_PAUSE || vk == VK_SCROLL ||
           vk == VK_SNAPSHOT;
}

bool ParseHotkey(const std::string& text, Hotkey& out) {
    UINT mods = 0, vk = 0;
    std::stringstream ss(text);
    std::string part;
    while (std::getline(ss, part, '+')) {
        part = ToLowerAscii(Trim(part));
        if (part.empty()) return false;
        if (part == "ctrl" || part == "control") mods |= MOD_CONTROL;
        else if (part == "alt") mods |= MOD_ALT;
        else if (part == "shift") mods |= MOD_SHIFT;
        else if (part == "win" || part == "windows") mods |= MOD_WIN;
        else {
            if (vk != 0) return false;  // two main keys
            vk = KeyFromName(part);
            if (vk == 0) return false;
        }
    }
    if (vk == 0) return false;
    if (mods == 0 && !SafeAlone(vk)) return false;  // a bare letter would break typing
    out.mods = mods;
    out.vk = vk;
    out.text = Trim(text);
    return true;
}

// ------------------------------------------------------------------- colours ---

static std::string ColorToString(COLORREF c) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", GetRValue(c), GetGValue(c), GetBValue(c));
    return buf;
}

// "#RRGGBB" or "RRGGBB" -> a COLORREF (which is stored as 0x00BBGGRR). False for anything else.
static bool ParseColor(std::string s, COLORREF& out) {
    s = Trim(s);
    if (!s.empty() && s[0] == '#') s.erase(0, 1);
    if (s.size() != 6) return false;
    for (unsigned char c : s) {
        if (!std::isxdigit(c)) return false;
    }
    const unsigned long rgb = std::stoul(s, nullptr, 16);  // 0xRRGGBB, as written in the file
    out = RGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
    return true;
}

// ----------------------------------------------------------------- file i/o ---

static json VoiceControlsJson(const std::map<std::string, VoiceControls>& voices);

// { "<Language id>": "<Voice name>" }
static json ActiveVoicesJson(const std::map<std::string, std::string>& m) {
    json out = json::object();
    for (const auto& [language, voice] : m) out[language] = voice;
    return out;
}

// A whole Settings as settings.json, with every key, in the order a person reads them best. Used to
// write the file when there is none yet.
static json ToJson(const Settings& s) {
    json j;
    j["hotkeys"] = {{"read", s.read.text},
                    {"pause", s.pause.text},
                    {"speech", s.speech.text},
                    {"switchLanguage", s.switchLanguage.text}};
    j["language"] = s.language;
    j["interfaceLanguage"] = s.interfaceLanguage;
    j["enabled"] = s.enabled;
    j["theme"] = s.theme;
    j["runAtStartup"] = s.runAtStartup;
    j["startMinimised"] = s.startMinimised;
    j["runAsAdmin"] = s.runAsAdmin;
    j["leftHanded"] = s.leftHanded;
    j["readAlong"] = s.readAlong;
    j["speech"] = {{"rate", s.speechRate}, {"volume", s.speechVolume}};
    j["voices"] = VoiceControlsJson(s.voices);
    j["activeVoices"] = ActiveVoicesJson(s.activeVoices);
    j["controlBar"] = {{"size", s.controlBar.size == BarSize::Small ? "small" : "large"},
                       {"x", s.controlBar.x},
                       {"y", s.controlBar.y}};
    j["overlay"] = {{"hoverColor", ColorToString(s.hoverColor)},
                    {"pickedColor", ColorToString(s.pickedColor)}};
    j["layout"] = {{"paragraphGap", s.layout.paragraphGap},
                   {"columnGap", s.layout.columnGap},
                   {"listLine", s.layout.listLine}};
    return j;
}

std::string SettingsToText(const Settings& s) { return ToJson(s).dump(2) + "\n"; }

// Writes a whole new settings.json (its folder too, if missing).
static bool WriteSettings(const fs::path& path, const Settings& s) {
    std::error_code ec;
    if (path.has_parent_path()) fs::create_directories(path.parent_path(), ec);
    std::ofstream out(path);
    if (!out) return false;
    out << SettingsToText(s);
    return static_cast<bool>(out);
}

// root[section][key], or nullptr if it isn't there.
static const json* Find(const json& root, const char* section, const char* key) {
    if (!root.is_object()) return nullptr;
    const json* node = &root;
    if (section) {
        const auto it = root.find(section);
        if (it == root.end() || !it->is_object()) return nullptr;
        node = &*it;
    }
    const auto it = node->find(key);
    return it == node->end() ? nullptr : &*it;
}

// nlohmann parses without recursion, but dump() recurses once per level, so a file nested
// deeper than this would overflow the stack the next time a value is saved. No real
// settings.json is more than four levels deep.
constexpr int kMaxJsonDepth = 64;

// json::parse with that nesting limit. A text too deep is treated like one that is not JSON:
// false, with the reason in `error`. The part too deep is never built.
static bool ParseJson(const std::string& text, bool allowComments, json& out, std::string& error) {
    bool tooDeep = false;
    const json::parser_callback_t limit = [&tooDeep](int depth, json::parse_event_t event, json&) {
        if ((event == json::parse_event_t::object_start || event == json::parse_event_t::array_start) &&
            depth >= kMaxJsonDepth) {
            tooDeep = true;
            return false;  // drop this value
        }
        return true;
    };
    try {
        out = json::parse(text, limit, true, allowComments);
    } catch (const json::exception& e) {
        error = e.what();
        return false;
    }
    if (tooDeep) {
        error = "it is nested more than " + std::to_string(kMaxJsonDepth) + " levels deep";
        return false;
    }
    return true;
}

// Notes are built with the logger's Join so they read exactly like the log lines they become.
template <class... A>
static std::string Note(const A&... parts) {
    return log_detail::Join(parts...);
}

// ----------------------------------------------------------- Voice controls ---

// Reads one control of a Voice. A missing key keeps the default quietly; a value that is not a
// number inside the control's range is a bad value: note + the default, like everywhere else.
// `notes` null = say nothing (the keys of an older file's Presets).
static void ParseControl(const json& obj, const std::string& where, const char* key, VoiceControl control,
                         VoiceControls& dst, std::vector<std::string>* notes) {
    const auto it = obj.find(key);
    if (it == obj.end()) return;
    const ControlRange r = RangeOf(control);
    if (it->is_number()) {
        const double v = it->get<double>();
        if (v >= r.min && v <= r.max) {
            Set(dst, control, static_cast<float>(v));
            return;
        }
    }
    if (notes) {
        notes->push_back(Note(where, ".", key, " must be a number from ", r.min, " to ", r.max,
                              "; using the default ", r.def, "."));
    }
}

// The three Voice controls of one Voice's object.
static void ParseControls(const json& obj, const std::string& where, VoiceControls& dst,
                          std::vector<std::string>* notes) {
    ParseControl(obj, where, "speed", VoiceControl::Speed, dst, notes);
    ParseControl(obj, where, "pitch", VoiceControl::Pitch, dst, notes);
    ParseControl(obj, where, "volume", VoiceControl::Volume, dst, notes);
}

static void ParseVoices(const json& root, std::map<std::string, VoiceControls>& out,
                        std::vector<std::string>& notes) {
    const json* voices = Find(root, nullptr, "voices");
    if (!voices) return;
    if (!voices->is_object()) {
        notes.push_back(Note("voices must be an object with one entry per Voice; ignoring it."));
        return;
    }
    for (const auto& [voiceName, voice] : voices->items()) {
        const std::string where = "voices." + voiceName;
        if (!voice.is_object()) {
            notes.push_back(Note(where, " must be an object (speed, pitch, volume); ignoring it."));
            continue;
        }
        VoiceControls c;
        // An older file kept named Presets and the name of the active one: its controls are the
        // current ones now. Quiet: the file was right when it was written.
        const auto presets = voice.find("presets");
        const auto active = voice.find("preset");
        if (presets != voice.end() && presets->is_object() && active != voice.end() && active->is_string()) {
            const std::string wanted = Trim(active->get<std::string>());
            for (const auto& [rawName, preset] : presets->items()) {
                if (Trim(rawName) == wanted && preset.is_object()) {
                    ParseControls(preset, where, c, nullptr);
                    break;
                }
            }
        }
        ParseControls(voice, where, c, &notes);  // the current format wins over a migrated Preset
        out[voiceName] = c;
    }
}

static void ParseActiveVoices(const json& root, std::map<std::string, std::string>& out,
                              std::vector<std::string>& notes) {
    const json* av = Find(root, nullptr, "activeVoices");
    if (!av) return;
    if (!av->is_object()) {
        notes.push_back(Note("activeVoices must be an object (Language id -> Voice name); ignoring it."));
        return;
    }
    for (const auto& [language, voice] : av->items()) {
        const std::string where = "activeVoices." + language;
        if (!FindLanguage(language)) {
            notes.push_back(Note(where, " is not a Language Lexia knows; ignoring it."));
        } else if (!voice.is_string() || Trim(voice.get<std::string>()).empty()) {
            notes.push_back(Note(where, " must be the name of a Voice; using the default Voice."));
        } else {
            out[language] = Trim(voice.get<std::string>());
        }
    }
}

// The "voices" value of a settings.json (VoiceControlsToJson as JSON, for ToJson).
static json VoiceControlsJson(const std::map<std::string, VoiceControls>& voices) {
    // Three decimals are plenty for a slider and keep 0.8f from being written as 0.800000011920929.
    auto round3 = [](float v) { return std::round(static_cast<double>(v) * 1000.0) / 1000.0; };
    json out = json::object();
    for (const auto& [voiceName, c] : voices) {
        out[voiceName] = {{"speed", round3(c.speed)}, {"pitch", round3(c.pitch)}, {"volume", round3(c.volume)}};
    }
    return out;
}

std::string VoiceControlsToJson(const std::map<std::string, VoiceControls>& voices) {
    return VoiceControlsJson(voices).dump(2);
}

std::string ActiveVoicesToJson(const std::map<std::string, std::string>& activeVoices) {
    return ActiveVoicesJson(activeVoices).dump(2);
}

bool operator==(const Hotkey& a, const Hotkey& b) {
    return a.mods == b.mods && a.vk == b.vk && a.text == b.text;
}

bool operator==(const Settings& a, const Settings& b) {
    return a.read == b.read && a.pause == b.pause && a.speech == b.speech &&
           a.switchLanguage == b.switchLanguage && a.language == b.language &&
           a.interfaceLanguage == b.interfaceLanguage &&
           a.enabled == b.enabled && a.theme == b.theme && a.runAtStartup == b.runAtStartup &&
           a.startMinimised == b.startMinimised && a.runAsAdmin == b.runAsAdmin && a.leftHanded == b.leftHanded && a.readAlong == b.readAlong &&
           a.speechRate == b.speechRate &&
           a.speechVolume == b.speechVolume && a.hoverColor == b.hoverColor &&
           a.pickedColor == b.pickedColor && a.layout.paragraphGap == b.layout.paragraphGap &&
           a.layout.columnGap == b.layout.columnGap && a.layout.listLine == b.layout.listLine &&
           a.voices == b.voices && a.activeVoices == b.activeVoices && a.controlBar.size == b.controlBar.size &&
           a.controlBar.x == b.controlBar.x && a.controlBar.y == b.controlBar.y;
}

std::vector<std::string> KeepToOffered(Settings& s) {
    std::vector<std::string> notes;
    const Language* lang = FindLanguage(s.language);
    if (lang && !lang->offered) {
        notes.push_back(Note("language \"", s.language, "\" (", lang->name, ") is not offered yet; using ",
                             DefaultLanguage().name, "."));
        s.language = DefaultLanguage().id;
    }
    if (!strings::IsOfferedInterfaceLanguage(s.interfaceLanguage)) {
        notes.push_back(Note("interfaceLanguage \"", s.interfaceLanguage, "\" is not offered yet; using \"",
                             strings::DefaultInterfaceLanguage(), "\"."));
        s.interfaceLanguage = strings::DefaultInterfaceLanguage();
    }
    return notes;
}

Settings ParseSettings(const std::string& jsonText, std::vector<std::string>& notes,
                       const std::string& source, bool* wellFormed) {
    Settings s;
    if (wellFormed) *wellFormed = true;

    json j;
    std::string error;
    if (!ParseJson(jsonText, /*allowComments=*/true, j, error)) {
        if (wellFormed) *wellFormed = false;
        notes.push_back(Note("Could not read ", source, ":\n           ", error,
                             "\n           The file was left untouched."));
        return s;
    }

    auto hotkey = [&](const char* key, Hotkey& dst) {
        const json* v = Find(j, "hotkeys", key);
        if (!v) return;
        Hotkey h;
        if (v->is_string() && Trim(v->get<std::string>()).empty()) {
            dst = Hotkey{};  // "" = no key (how a default Lexia file shows an unbound action)
            return;
        }
        if (v->is_string() && ParseHotkey(v->get<std::string>(), h)) {
            dst = h;
            return;
        }
        notes.push_back(Note("hotkeys.", key,
                             " is not a valid key (examples: \"F8\", \"Ctrl+Alt+R\"; letters need a "
                             "modifier). Keeping ",
                             dst.text, "."));
    };
    hotkey("read", s.read);
    hotkey("pause", s.pause);
    hotkey("speech", s.speech);
    hotkey("switchLanguage", s.switchLanguage);

    // Two actions on one key can't both be registered (R35). Compare in a fixed order; the
    // later action of a clash goes back to its own default, and if that is taken too it is
    // left unbound rather than making Lexia exit.
    const Settings defaults;
    struct Action {
        const char* name;
        Hotkey* key;
        const Hotkey* def;
    };
    const Action actions[] = {{"read", &s.read, &defaults.read},
                              {"pause", &s.pause, &defaults.pause},
                              {"speech", &s.speech, &defaults.speech},
                              {"switchLanguage", &s.switchLanguage, &defaults.switchLanguage}};
    auto holder = [&](size_t upTo, const Hotkey& k) -> const char* {  // earlier action on k
        for (size_t j = 0; j < upTo; ++j) {
            if (actions[j].key->IsBound() && actions[j].key->mods == k.mods &&
                actions[j].key->vk == k.vk) {
                return actions[j].name;
            }
        }
        return nullptr;
    };
    for (size_t i = 1; i < std::size(actions); ++i) {
        const Action& a = actions[i];
        const char* first = holder(i, *a.key);
        if (!first) continue;
        const std::string clash = Note("hotkeys.", a.name, " uses ", a.key->text, ", which hotkeys.",
                                       first, " already uses");
        if (!a.def->IsBound()) {  // no default key to fall back to
            notes.push_back(Note(clash, ". hotkeys.", a.name, " is left unbound."));
            *a.key = Hotkey{};
        } else if (!holder(i, *a.def)) {
            notes.push_back(Note(clash, ". Using ", a.def->text, " instead."));
            *a.key = *a.def;
        } else {
            notes.push_back(Note(clash, ", and its default ", a.def->text, " is taken too. hotkeys.",
                                 a.name, " is left unbound."));
            *a.key = Hotkey{};
        }
    }

    if (const json* v = Find(j, nullptr, "language")) {
        if (v->is_string() && FindLanguage(v->get<std::string>())) {
            s.language = v->get<std::string>();
        } else {
            notes.push_back(Note("language is not one Lexia knows; keeping \"", s.language, "\"."));
        }
    }

    if (const json* v = Find(j, nullptr, "interfaceLanguage")) {
        if (v->is_string() && strings::IsInterfaceLanguage(v->get<std::string>())) {
            s.interfaceLanguage = v->get<std::string>();
        } else {
            std::string known;
            for (const strings::InterfaceLanguage& l : strings::InterfaceLanguages()) {
                known += (known.empty() ? "\"" : ", \"") + l.code + "\"";
            }
            notes.push_back(Note("interfaceLanguage must be one of ", known, "; keeping \"",
                                 s.interfaceLanguage, "\"."));
        }
    }

    // A value outside its range is a bad value like any other: note + the default.
    auto integer = [&](const char* key, int lo, int hi, int& dst) {
        const json* v = Find(j, "speech", key);
        if (!v) return;
        // A whole number above INT64_MAX is stored unsigned and would wrap through get<long long>().
        const bool fits = !v->is_number_unsigned() ||
                          (hi >= 0 && v->get<unsigned long long>() <= static_cast<unsigned long long>(hi));
        if (v->is_number_integer() && fits) {
            const long long n = v->get<long long>();
            if (n >= lo && n <= hi) {
                dst = static_cast<int>(n);
                return;
            }
        }
        notes.push_back(Note("speech.", key, " must be a whole number (", lo, " to ", hi,
                             "); keeping ", dst, "."));
    };
    integer("rate", -10, 10, s.speechRate);
    integer("volume", 0, 100, s.speechVolume);

    if (const json* v = Find(j, nullptr, "enabled")) {
        if (v->is_boolean()) {
            s.enabled = v->get<bool>();
        } else {
            notes.push_back(Note("enabled must be true or false; keeping ", s.enabled, "."));
        }
    }

    if (const json* v = Find(j, nullptr, "theme")) {
        const std::string t = v->is_string() ? v->get<std::string>() : std::string();
        if (t == "system" || t == "light" || t == "dark") {
            s.theme = t;
        } else {
            notes.push_back(Note("theme must be \"system\", \"light\" or \"dark\"; keeping \"", s.theme, "\"."));
        }
    }
    auto boolean = [&](const char* key, bool& dst) {
        const json* v = Find(j, nullptr, key);
        if (!v) return;
        if (v->is_boolean()) {
            dst = v->get<bool>();
        } else {
            notes.push_back(Note(key, " must be true or false; keeping ", dst, "."));
        }
    };
    boolean("runAtStartup", s.runAtStartup);
    boolean("startMinimised", s.startMinimised);
    boolean("runAsAdmin", s.runAsAdmin);
    boolean("leftHanded", s.leftHanded);
    boolean("readAlong", s.readAlong);

    auto colour = [&](const char* key, COLORREF& dst) {
        const json* v = Find(j, "overlay", key);
        if (!v) return;
        COLORREF c = 0;
        if (v->is_string() && ParseColor(v->get<std::string>(), c)) {
            dst = c;
        } else {
            notes.push_back(Note("overlay.", key, " must look like \"#FFC800\"; keeping ",
                                 ColorToString(dst), "."));
        }
    };
    colour("hoverColor", s.hoverColor);
    colour("pickedColor", s.pickedColor);

    // Tuning knobs for the grouping (A4). Whole or fractional numbers, in line heights.
    auto knob = [&](const char* key, double lo, double hi, double& dst) {
        const json* v = Find(j, "layout", key);
        if (!v) return;
        if (v->is_number()) {
            const double x = v->get<double>();
            if (x >= lo && x <= hi) {
                dst = x;
                return;
            }
        }
        notes.push_back(Note("layout.", key, " must be a number from ", lo, " to ", hi,
                             " (line heights); keeping ", dst, "."));
    };
    knob("paragraphGap", 0.3, 3.0, s.layout.paragraphGap);
    knob("columnGap", 2.0, 20.0, s.layout.columnGap);
    knob("listLine", 0.0, 30.0, s.layout.listLine);
    ParseVoices(j, s.voices, notes);
    ParseActiveVoices(j, s.activeVoices, notes);

    // The Control bar (D4): x and y are offsets from the monitor's corner, so never below 0; -1 = not set.
    if (const json* v = Find(j, "controlBar", "size")) {
        const std::string t = v->is_string() ? v->get<std::string>() : std::string();
        if (t == "large" || t == "small") {
            s.controlBar.size = t == "small" ? BarSize::Small : BarSize::Large;
        } else {
            notes.push_back(Note("controlBar.size must be \"large\" or \"small\"; keeping \"large\"."));
        }
    }
    auto barPos = [&](const char* key, int& dst) {
        const json* v = Find(j, "controlBar", key);
        if (!v) return;
        if (v->is_number_integer()) {
            const long long n = v->is_number_unsigned()
                                    ? (v->get<unsigned long long>() <= 1000000ULL ? static_cast<long long>(v->get<unsigned long long>()) : -2)
                                    : v->get<long long>();
            if (n >= -1 && n <= 1000000) {
                dst = static_cast<int>(n);
                return;
            }
        }
        notes.push_back(Note("controlBar.", key, " must be a whole number from 0 (-1 = not set); keeping ", dst, "."));
    };
    barPos("x", s.controlBar.x);
    barPos("y", s.controlBar.y);

    return s;
}

// ParseSettings of the text read from `path`, logging each note.
static Settings ParseAndLogSettings(const fs::path& path, const std::string& text, bool* wellFormed) {
    std::vector<std::string> notes;
    const Settings s = ParseSettings(text, notes, Note(path), wellFormed);
    for (const std::string& n : notes) LogWarn("[settings] ", n);
    return s;
}

Settings LoadSettings() {
    const fs::path path = SettingsPath();

    std::error_code ec;
    if (!fs::exists(path, ec)) {
        const Settings defaults;
        if (WriteSettings(path, defaults)) {
            LogInfo("[settings] No settings file yet; created one with the defaults:\n",
                    "           ", path);
        } else {
            LogWarn("[settings] Could not create ", path, " (using the defaults).");
        }
        return defaults;
    }

    std::string text;
    ReadFileToString(path, &text);  // unreadable: empty text, which ParseSettings reports as not JSON

    bool wellFormed = true;
    const Settings s = ParseAndLogSettings(path, text, &wellFormed);
    if (!wellFormed) LogWarn("[settings] Using the default settings.");
    return s;
}

bool RunAsAdminSetting() {
    std::string text;
    if (!ReadFileToString(SettingsPath(), &text, 1024 * 1024)) return false;  // no file (yet): the default, off
    std::vector<std::string> notes;  // LoadSettings reports them when the app reads the file itself
    return ParseSettings(text, notes).runAsAdmin;
}

bool ReloadSettings(Settings& out) {
    const fs::path path = SettingsPath();

    std::string text;
    if (!ReadFileToString(path, &text)) {
        LogWarn("[settings] Could not read ", path, "; keeping the current settings.");
        return false;
    }

    bool wellFormed = true;
    const Settings s = ParseAndLogSettings(path, text, &wellFormed);
    if (!wellFormed) {
        LogWarn("[settings] Keeping the current settings.");
        return false;
    }
    out = s;
    return true;
}

bool UpdateSettingsText(std::string& text, const std::string& key, const std::string& valueJson,
                        std::string& note) {
    json root, value;
    std::string error;
    if (!ParseJson(valueJson, false, value, error) ||
        !ParseJson(text, false, root, error)) {  // comments not allowed here, see the note below
        // Either the file has // comments (ParseSettings accepts them, but a rewrite would drop
        // them), it is nested too deep, or it is not JSON at all. Either way it stays as the user
        // wrote it.
        note = Note("the file can't be rewritten safely (", error, ")");
        return false;
    }
    if (!root.is_object()) {
        note = "the file is not a JSON object";
        return false;
    }

    // Keep the user's indentation (the first indented line decides); two spaces by default.
    int indent = 2;
    char indentChar = ' ';
    const size_t nl = text.find('\n');
    if (nl != std::string::npos) {
        const size_t first = text.find_first_not_of(" \t", nl + 1);
        if (first != std::string::npos && first > nl + 1) {
            indent = static_cast<int>(first - (nl + 1));
            indentChar = text[nl + 1];
        }
    }

    const size_t dot = key.find('.');
    if (dot == std::string::npos) {
        root[key] = value;
    } else {
        json& section = root[key.substr(0, dot)];
        if (!section.is_object()) section = json::object();
        section[key.substr(dot + 1)] = value;
    }
    text = root.dump(indent, indentChar) + "\n";
    return true;
}

bool SaveSettingValue(const std::string& key, const std::string& valueJson) {
    const fs::path path = SettingsPath();

    std::error_code ec;
    if (!fs::exists(path, ec) && !WriteSettings(path, Settings{})) {
        LogWarn("[settings] Could not create ", path, "; ", key, " was not saved.");
        return false;
    }

    std::string text;
    ReadFileToString(path, &text);  // unreadable: empty text, which UpdateSettingsText refuses
    std::string note;
    if (!UpdateSettingsText(text, key, valueJson, note)) {
        LogWarn("[settings] ", key, " was not saved: ", note, ".");
        return false;
    }
    // Write beside the file and swap it in, so a crash mid-write never leaves settings.json empty
    // and the watcher never sees half a file.
    fs::path tmp = path;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        out << text;
        out.close();
        if (!out) {
            LogWarn("[settings] Could not write ", tmp, "; ", key, " was not saved.");
            return false;
        }
    }
    if (!MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        LogWarn("[settings] Could not replace ", path, " (error ", GetLastError(), "); ", key,
                " was not saved.");
        fs::remove(tmp, ec);
        return false;
    }
    return true;
}
