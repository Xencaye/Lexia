#include "ui_host.h"

#include <algorithm>
#include <cmath>
#include <nlohmann/json.hpp>

#include "strings/string_ids.h"
#include "strings/strings.h"

// The host's side of the bridge to the ui/ pages (the contract is ui/README.md): the JSON messages the
// host sends (strings, state, the Voice tab's answers) and the check of every message the page sends.
// Pure and Win32-free, so all of it is tested. The page is treated as untrusted input: a message
// with a wrong type, a missing field or a too long string is Unknown and does nothing.
using nlohmann::ordered_json;

namespace {

// One message as compact JSON text. ordered_json keeps the keys in the order written.
std::string Dump(const ordered_json& j) {
    // replace: a stray non-UTF-8 byte becomes U+FFFD instead of an exception.
    return j.dump(-1, ' ', false, ordered_json::error_handler_t::replace);
}

const size_t kMaxNameBytes = 200;  // a Voice, a Language: nothing the page sends is longer

// A string field of `j`, if there is one that is not too long.
bool GetString(const nlohmann::json& j, const char* key, std::string& out) {
    const auto it = j.find(key);
    if (it == j.end() || !it->is_string()) return false;
    out = it->get<std::string>();
    return out.size() <= kMaxNameBytes;
}

bool GetNonEmpty(const nlohmann::json& j, const char* key, std::string& out) {
    return GetString(j, key, out) && !out.empty();
}

bool GetBool(const nlohmann::json& j, const char* key, bool& out) {
    const auto it = j.find(key);
    if (it == j.end() || !it->is_boolean()) return false;
    out = it->get<bool>();
    return true;
}

bool IsControlName(const std::string& s) {
    return s == "speed" || s == "pitch" || s == "volume";
}

bool IsKeyAction(const std::string& s) { return s == "read" || s == "pause" || s == "speech"; }

bool IsTheme(const std::string& s) { return s == "system" || s == "light" || s == "dark"; }

bool LinkFromName(const std::string& s, LinkId& out) {
    if (s == "mail") out = LinkId::Mail;
    else if (s == "discord") out = LinkId::Discord;
    else if (s == "github") out = LinkId::Github;
    else if (s == "itch") out = LinkId::Itch;
    else return false;
    return true;
}

}  // namespace

// { type: "strings", language, strings: { id: text } }: every text the page shows, sent once when the
// page is ready. An unknown Interface language gets English.
std::string StringsMessage(const std::string& interfaceLanguage) {
    const std::string code = strings::IsInterfaceLanguage(interfaceLanguage)
                                 ? interfaceLanguage
                                 : strings::DefaultInterfaceLanguage();
    ordered_json table = ordered_json::object();
    for (const char* id : strings::AllIds()) table[id] = strings::Text(code, id);
    return Dump({{"type", "strings"}, {"language", code}, {"strings", std::move(table)}});
}

// { type: "state", ... }: everything the page draws. The page keeps no copy of a setting; it draws the
// last state it got, and the host sends a new one after every change.
std::string StateMessage(const UiState& s) {
    ordered_json keys = ordered_json::array();
    for (const UiKey& k : s.keys) keys.push_back({{"action", k.action}, {"key", k.key}});

    ordered_json voices = ordered_json::object();
    for (const auto& [language, v] : s.voices) {
        voices[language] = {{"installed", v.installed}, {"active", v.active}, {"removable", v.removable}};
    }

    ordered_json controls = ordered_json::object();
    for (const auto& [name, c] : s.controls) {
        controls[name] = {{"value", c.value}, {"min", c.min}, {"max", c.max}, {"def", c.def}, {"step", c.step}};
    }

    return Dump({{"type", "state"},
                 {"on", s.on},
                 {"language", s.language},
                 {"theme", s.theme},
                 {"dark", s.dark},
                 {"version", s.version},
                 {"runAtStartup", s.runAtStartup},
                 {"startMinimised", s.startMinimised},
                 {"runAsAdmin", s.runAsAdmin},
                 {"elevated", s.elevated},
                 {"leftHanded", s.leftHanded},
                 {"masterVolume", {{"value", s.masterVolume.value}, {"min", s.masterVolume.min}, {"max", s.masterVolume.max}, {"def", s.masterVolume.def}, {"step", s.masterVolume.step}}},
                 {"keys", std::move(keys)},
                 {"voices", std::move(voices)},
                 {"controls", std::move(controls)}});
}

// One message from the page -> a PageMessage whose fields are checked. Not JSON, no "type", an unknown
// type, or a bad field: type stays Unknown (typeText keeps the type for the log).
PageMessage ParsePageMessage(const std::string& text) {
    PageMessage m;
    const nlohmann::json j = nlohmann::json::parse(text, nullptr, /*allow_exceptions=*/false);
    if (!j.is_object()) return m;
    const auto typeIt = j.find("type");
    if (typeIt == j.end() || !typeIt->is_string()) return m;
    const std::string type = typeIt->get<std::string>();
    m.typeText = type.substr(0, kMaxNameBytes);

    // `ok` says whether the payload passed; a failed one leaves the type Unknown.
    auto accept = [&m](PageMessageType t, bool ok) {
        if (ok) m.type = t;
    };

    if (type == "ready") {
        accept(PageMessageType::Ready, true);
    } else if (type == "openCatalogue") {
        accept(PageMessageType::OpenCatalogue, true);
    } else if (type == "addFromFile") {
        accept(PageMessageType::AddFromFile, true);
    } else if (type == "openNotices") {
        accept(PageMessageType::OpenNotices, true);
    } else if (type == "setOn") {
        accept(PageMessageType::SetOn, GetBool(j, "on", m.on));
    } else if (type == "setRunAtStartup") {
        accept(PageMessageType::SetRunAtStartup, GetBool(j, "on", m.on));
    } else if (type == "setStartMinimised") {
        accept(PageMessageType::SetStartMinimised, GetBool(j, "on", m.on));
    } else if (type == "setRunAsAdmin") {
        accept(PageMessageType::SetRunAsAdmin, GetBool(j, "on", m.on));
    } else if (type == "setLeftHanded") {
        accept(PageMessageType::SetLeftHanded, GetBool(j, "on", m.on));
    } else if (type == "savePresetFile") {
        accept(PageMessageType::SavePresetFile, true);
    } else if (type == "loadPresetFile") {
        accept(PageMessageType::LoadPresetFile, true);
    } else if (type == "deleteVoice") {
        accept(PageMessageType::DeleteVoice, GetNonEmpty(j, "voice", m.voice));
    } else if (type == "openLink") {
        std::string name;
        accept(PageMessageType::OpenLink, GetString(j, "link", name) && LinkFromName(name, m.link));
    } else if (type == "setMasterVolume") {
        const auto v = j.find("value");
        const bool number = v != j.end() && v->is_number();
        if (number) m.value = v->get<double>();
        const bool ok = number && std::isfinite(m.value);
        if (ok) m.value = std::min(std::max(m.value, 0.0), 100.0);  // percent
        accept(PageMessageType::SetMasterVolume, ok);
    } else if (type == "setTheme") {
        accept(PageMessageType::SetTheme, GetString(j, "theme", m.theme) && IsTheme(m.theme));
    } else if (type == "setVoice") {
        accept(PageMessageType::SetVoice, GetNonEmpty(j, "language", m.language) && GetNonEmpty(j, "voice", m.voice));
    } else if (type == "setControl") {
        const auto v = j.find("value");
        const bool number = v != j.end() && v->is_number();
        if (number) m.value = v->get<double>();
        accept(PageMessageType::SetControl, GetString(j, "name", m.name) && IsControlName(m.name) && number &&
                                                std::isfinite(m.value));
    } else if (type == "resetControl") {
        accept(PageMessageType::ResetControl, GetString(j, "name", m.name) && IsControlName(m.name));
    } else if (type == "download") {
        accept(PageMessageType::Download, GetNonEmpty(j, "name", m.name));
    } else if (type == "cancelDownload") {
        accept(PageMessageType::CancelDownload, GetNonEmpty(j, "name", m.name));
    } else if (type == "readKeyCard") {
        accept(PageMessageType::ReadKeyCard, GetString(j, "action", m.action) && IsKeyAction(m.action));
    } else if (type == "captureKey") {
        accept(PageMessageType::CaptureKey, GetString(j, "action", m.action) && IsKeyAction(m.action));
    } else if (type == "cancelKeyCapture") {
        accept(PageMessageType::CancelKeyCapture, GetString(j, "action", m.action) && IsKeyAction(m.action));
    } else if (type == "setKey") {
        accept(PageMessageType::SetKey, GetString(j, "action", m.action) && IsKeyAction(m.action) &&
                                            GetString(j, "key", m.key));  // "" = no key
    }
    return m;
}

// A new GitHub issue with the Lexia and Windows versions filled in, and nothing else (no log, no
// screen: ADR-0005). The body is percent-encoded: every byte other than A-Z a-z 0-9 - . _ ~ is %XX.
std::string BugReportUrl(const std::string& lexiaVersion, const std::string& windowsVersion) {
    const std::string body = "Lexia " + lexiaVersion + ", Windows " + windowsVersion;
    std::string out = "https://github.com/Xencaye/Lexia/issues/new?body=";
    static const char* hex = "0123456789ABCDEF";
    for (const unsigned char c : body) {
        const bool plain = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                           c == '-' || c == '.' || c == '_' || c == '~';
        if (plain) {
            out += static_cast<char>(c);
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 15];
        }
    }
    return out;
}

const char* LinkName(LinkId id) {
    switch (id) {
    case LinkId::Mail: return "mail";
    case LinkId::Discord: return "discord";
    case LinkId::Github: return "github";
    case LinkId::Itch: return "itch";
    }
    return "";
}

// The fixed addresses of the page's links. The page sends only the link's name, never an address, so
// a page cannot make Lexia open anything else.
std::string LinkUrl(LinkId id, const std::string& lexiaVersion, const std::string& windowsVersion) {
    switch (id) {
    case LinkId::Mail: return "mailto:lexia@xencaye.com";
    case LinkId::Discord: return "https://discord.gg/FAaZx9KVpB";
    case LinkId::Itch: return "https://xencaye.itch.io/lexia";
    case LinkId::Github:
        if (lexiaVersion.empty() && windowsVersion.empty()) return "https://github.com/Xencaye/Lexia/issues/new";
        return BugReportUrl(lexiaVersion, windowsVersion);
    }
    return std::string();
}

// "dark" and "light" are fixed; "system" (or anything else) follows Windows' app mode.
bool IsDarkTheme(const std::string& theme, bool systemUsesLightTheme) {
    if (theme == "dark") return true;
    if (theme == "light") return false;
    return !systemUsesLightTheme;
}

// ------------------------------------------------------------- the Voice tab (D5) ---

namespace {

// A Voice control as the page knows it: its name in the messages and the step of its slider.
struct ControlInfo {
    const char* name;
    VoiceControl control;
    double step;
};
const ControlInfo kControls[] = {{"speed", VoiceControl::Speed, 0.05},
                                 {"pitch", VoiceControl::Pitch, 0.5},
                                 {"volume", VoiceControl::Volume, 0.05}};

// Round to the three decimals the sliders use, so 0.8f is not sent as 0.800000011920929.
double Round3(double v) { return std::round(v * 1000.0) / 1000.0; }

// A Lexia Language id for a Voice language code ("en" or the catalogue's "en_GB"); nullptr if none is offered.
const Language* OfferedLanguageForCode(const std::string& code) {
    const Language* l = FindLanguageByCode(code.substr(0, code.find_first_of("_-")));
    return l && l->offered ? l : nullptr;
}

// The facts the page puts into a refusal's text ("Lexia can't read {language} yet").
void AddProblemFacts(ordered_json& j, const VoiceOutcome& o) {
    if (!o.languageName.empty()) j["language"] = o.languageName;
    if (!o.quality.empty()) j["quality"] = o.quality;
}

}  // namespace

// The Voice that speaks `lang`: the user's choice (activeVoices) if it is installed, else the one
// PickVoice prefers. "" when the Language has no Voice.
std::string ActiveVoiceName(const VoiceModel& m, const Language& lang) {
    const auto chosen = m.activeVoices.find(lang.id);
    const InstalledVoice* v = PickVoice(m.installed, lang.code, chosen == m.activeVoices.end() ? std::string() : chosen->second);
    return v ? v->name : std::string();
}

// The Voice tab's part of the state: for each offered Language, its installed Voices (the shipped
// ones cannot be deleted) and the active one; and the sliders of the current Language's Voice. With
// no Voice for the current Language there are no sliders.
void FillVoiceState(UiState& state, const VoiceModel& m) {
    state.voices.clear();
    state.controls.clear();
    for (const Language& lang : AllLanguages()) {
        if (!lang.offered) continue;
        UiVoices v;
        for (const InstalledVoice& i : m.installed) {
            if (i.languageCode == lang.code) {
                v.installed.push_back(i.name);
                if (!i.shipped) v.removable.push_back(i.name);
            }
        }
        v.active = ActiveVoiceName(m, lang);
        state.voices[lang.id] = std::move(v);
    }

    const Language* current = FindLanguage(m.languageId);
    if (!current) return;
    const std::string voice = ActiveVoiceName(m, *current);
    if (voice.empty()) return;

    const auto cit = m.controls.find(voice);
    const VoiceControls shown = cit == m.controls.end() ? VoiceControls{} : Clamped(cit->second);
    for (const ControlInfo& c : kControls) {
        const ControlRange r = RangeOf(c.control);
        state.controls[c.name] = UiControl{Round3(Get(shown, c.control)), r.min, r.max, r.def, c.step};
    }
}

// The downloadable Voices for the Voice download dialog: only those of offered Languages (ADR-0007).
std::string CatalogueMessage(const std::vector<CatalogueVoice>& voices, const std::set<std::string>& installedNames) {
    ordered_json list = ordered_json::array();
    for (const CatalogueVoice& v : voices) {
        const Language* lang = OfferedLanguageForCode(v.languageCode);
        if (!lang) continue;
        list.push_back({{"name", v.name},
                        {"language", lang->id},
                        {"sizeBytes", v.model.sizeBytes + v.config.sizeBytes},
                        {"installed", installedNames.count(v.name) > 0}});
    }
    return Dump({{"type", "catalogue"}, {"voices", std::move(list)}});
}

std::string CatalogueFailedMessage(VoiceProblem problem) {
    return Dump({{"type", "catalogueFailed"}, {"problem", ProblemId(problem)}});
}

std::string DownloadProgressMessage(const std::string& name, uint64_t done, uint64_t total) {
    return Dump({{"type", "downloadProgress"}, {"name", name}, {"done", done}, {"total", total}});
}

std::string DownloadDoneMessage(const std::string& name) { return Dump({{"type", "downloadDone"}, {"name", name}}); }

std::string DownloadFailedMessage(const std::string& name, const VoiceOutcome& o) {
    ordered_json j = {{"type", "downloadFailed"}, {"name", name}, {"problem", ProblemId(o.problem)}};
    AddProblemFacts(j, o);
    return Dump(j);
}

std::string VoiceAddedMessage(const VoiceOutcome& o) {
    const Language* lang = OfferedLanguageForCode(o.languageCode);
    return Dump({{"type", "voiceAdded"}, {"voice", o.voiceName}, {"language", lang ? lang->id : std::string()}});
}

std::string VoiceRefusedMessage(const VoiceOutcome& o) {
    ordered_json j = {{"type", "voiceRefused"}, {"problem", ProblemId(o.problem)}};
    AddProblemFacts(j, o);
    return Dump(j);
}

// ---------------------------------------------------------------- D8: Preset files, delete ---

const char* PresetProblemId(PresetProblem problem) {
    switch (problem) {
    case PresetProblem::BadFile: return "badFile";
    case PresetProblem::CannotWrite: return "cannotWrite";
    case PresetProblem::NoVoice: return "noVoice";
    }
    return "badFile";
}

std::string VoiceDeleteFailedMessage(const std::string& voice, VoiceProblem problem) {
    return Dump({{"type", "voiceDeleteFailed"}, {"voice", voice}, {"problem", ProblemId(problem)}});
}

std::string PresetSavedMessage(const std::string& name) { return Dump({{"type", "presetSaved"}, {"name", name}}); }

std::string PresetLoadedMessage(const std::string& name) { return Dump({{"type", "presetLoaded"}, {"name", name}}); }

std::string PresetFailedMessage(PresetProblem problem) {
    return Dump({{"type", "presetFailed"}, {"problem", PresetProblemId(problem)}});
}
