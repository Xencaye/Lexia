#include "ui_host.h"

#include <cmath>

// The pure parts of what the native Settings window and the host say to each other (ui_host.h): the fixed
// links, the theme choice and the Voice tab's part of the state. No Win32 and no JSON: the commands and the
// Voice library's answers are typed values.

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

// The fixed addresses of the window's links. A widget names the link, never an address, so
// nothing but these can be opened.
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

// A Voice control as the window knows it: its name and the step of its slider.
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

// ---- the card ids (see ui_host.h) ----

const std::vector<std::string>& SwitchCardIds() {
    static const std::vector<std::string> ids = {"readAlong",  "runAtStartup", "startMinimised",
                                                  "runAsAdmin", "darkMode",     "leftHanded"};
    return ids;
}

bool IsSwitchCard(const std::string& cardId) {
    for (const std::string& id : SwitchCardIds()) {
        if (id == cardId) return true;
    }
    return false;
}

// A card id that is not a switch is taken as a Key (the page only ever offers the two kinds).
std::string CardTitleId(const std::string& cardId) {
    return IsSwitchCard(cardId) ? "window.switches." + cardId + "Title" : "window.keys." + cardId;
}

std::string CardDescId(const std::string& cardId) {
    return IsSwitchCard(cardId) ? "window.switches." + cardId + "Desc" : "window.keys." + cardId + "Desc";
}
