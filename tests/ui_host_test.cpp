// Unit tests for ui_host.h: the messages between the Settings window's page and the host (the
// contract is ui/README.md). Asserted on the parsed JSON, so key order and spacing do not matter.
#include <doctest/doctest.h>

#include <nlohmann/json.hpp>
#include <cstdint>
#include <string>

#include "strings/string_ids.h"
#include "strings/strings.h"
#include "ui_host.h"

using nlohmann::json;

namespace {

UiState SampleState() {
    UiState s;
    s.on = true;
    s.language = "eng";
    s.theme = "dark";
    s.version = "0.5.0";
    s.runAtStartup = true;
    s.dark = true;
    s.runAsAdmin = true;
    s.elevated = false;
    s.leftHanded = true;
    s.masterVolume.value = 70;
    s.keys = {{"read", "F8"}, {"pause", "F7"}, {"speech", ""}};
    s.voices["eng"] = {{"en_US-ljspeech-medium", "en_US-kristin-medium"}, "en_US-ljspeech-medium", {"en_US-kristin-medium"}};
    s.controls["speed"] = {1.0, 0.5, 2.0, 1.0, 0.05};
    s.controls["pitch"] = {-2.0, -6.0, 6.0, 0.0, 0.5};
    return s;
}

PageMessage Parse(const std::string& text) { return ParsePageMessage(text); }

}  // namespace

// ------------------------------------------------------------------ strings ---

TEST_CASE("StringsMessage: type, language and one text per id") {
    const json m = json::parse(StringsMessage("en"));
    CHECK(m["type"] == "strings");
    CHECK(m["language"] == "en");
    CHECK(m["strings"]["window.nav.home"] == strings::Text("en", "window.nav.home"));
    CHECK(m["strings"].size() == strings::AllIds().size());
}

TEST_CASE("StringsMessage: an unknown Interface language gives English, and says so") {
    const json m = json::parse(StringsMessage("xx"));
    CHECK(m["language"] == "en");
    CHECK(m["strings"]["window.nav.home"] == strings::Text("en", "window.nav.home"));
}

// -------------------------------------------------------------------- state ---

TEST_CASE("StateMessage: the whole state object of ui/README.md") {
    const json m = json::parse(StateMessage(SampleState()));
    CHECK(m["type"] == "state");
    CHECK(m["on"] == true);
    CHECK(m["language"] == "eng");
    CHECK(m["theme"] == "dark");
    CHECK(m["version"] == "0.5.0");
    CHECK(m["runAtStartup"] == true);
    CHECK(m["dark"] == true);
    CHECK(m["runAsAdmin"] == true);
    CHECK(m["elevated"] == false);
    CHECK(m["leftHanded"] == true);
    CHECK(m["masterVolume"] == json{{"value", 70}, {"min", 0}, {"max", 100}, {"def", 100}, {"step", 5}});
    CHECK(m["startMinimised"] == true);  // the default of UiState (D9)
    CHECK_FALSE(m.contains("presets"));

    REQUIRE(m["keys"].size() == 3);
    CHECK(m["keys"][0] == json{{"action", "read"}, {"key", "F8"}});
    CHECK(m["keys"][2] == json{{"action", "speech"}, {"key", ""}});

    CHECK(m["voices"]["eng"]["installed"] == json::array({"en_US-ljspeech-medium", "en_US-kristin-medium"}));
    CHECK(m["voices"]["eng"]["active"] == "en_US-ljspeech-medium");
    CHECK(m["voices"]["eng"]["removable"] == json::array({"en_US-kristin-medium"}));

    CHECK(m["controls"]["speed"] == json{{"value", 1.0}, {"min", 0.5}, {"max", 2.0}, {"def", 1.0}, {"step", 0.05}});
    CHECK(m["controls"]["pitch"]["min"] == -6.0);
    CHECK_FALSE(m["controls"].contains("volume"));

}

TEST_CASE("StateMessage: with no Voice the controls are absent, the toggles off and the master volume full") {
    UiState s;
    s.version = "0.5.0";
    const json m = json::parse(StateMessage(s));
    CHECK(m["on"] == true);
    CHECK(m["theme"] == "system");
    REQUIRE(m.contains("controls"));
    CHECK(m["controls"].empty());
    CHECK(m["dark"] == false);
    CHECK(m["runAsAdmin"] == false);
    CHECK(m["leftHanded"] == false);
    CHECK(m["masterVolume"]["value"] == 100);
    CHECK(m["keys"].is_array());
    CHECK(m["voices"].is_object());
}

TEST_CASE("StateMessage: a name that is not valid UTF-8 does not throw") {
    UiState s = SampleState();
    s.voices["eng"].installed = {std::string("bad\xff")};
    CHECK_NOTHROW(StateMessage(s));
}

// ------------------------------------------------------------- page messages ---

TEST_CASE("ParsePageMessage: messages with no payload") {
    CHECK(Parse(R"({"type":"ready"})").type == PageMessageType::Ready);
    CHECK(Parse(R"({"type":"savePresetFile"})").type == PageMessageType::SavePresetFile);
    CHECK(Parse(R"({"type":"loadPresetFile"})").type == PageMessageType::LoadPresetFile);
    CHECK(Parse(R"({"type":"openCatalogue"})").type == PageMessageType::OpenCatalogue);
    CHECK(Parse(R"({"type":"addFromFile"})").type == PageMessageType::AddFromFile);
    CHECK(Parse(R"({"type":"openNotices"})").type == PageMessageType::OpenNotices);
}

TEST_CASE("ParsePageMessage: setOn and the toggles need a boolean") {
    PageMessage m = Parse(R"({"type":"setOn","on":false})");
    CHECK(m.type == PageMessageType::SetOn);
    CHECK_FALSE(m.on);
    CHECK(Parse(R"({"type":"setOn","on":true})").on);
    CHECK(Parse(R"({"type":"setOn","on":"yes"})").type == PageMessageType::Unknown);
    CHECK(Parse(R"({"type":"setOn"})").type == PageMessageType::Unknown);

    m = Parse(R"({"type":"setRunAtStartup","on":true})");
    CHECK(m.type == PageMessageType::SetRunAtStartup);
    CHECK(m.on);
    m = Parse(R"({"type":"setStartMinimised","on":false})");
    CHECK(m.type == PageMessageType::SetStartMinimised);
    CHECK_FALSE(m.on);
    CHECK(Parse(R"({"type":"setStartMinimised","on":"no"})").type == PageMessageType::Unknown);
    m = Parse(R"({"type":"setRunAsAdmin","on":true})");
    CHECK(m.type == PageMessageType::SetRunAsAdmin);
    CHECK(m.on);
    m = Parse(R"({"type":"setLeftHanded","on":false})");
    CHECK(m.type == PageMessageType::SetLeftHanded);
    CHECK_FALSE(m.on);
    CHECK(Parse(R"({"type":"setLeftHanded","on":1})").type == PageMessageType::Unknown);
    CHECK(Parse(R"({"type":"setRunAsAdmin"})").type == PageMessageType::Unknown);
}

TEST_CASE("ParsePageMessage: setTheme takes system, light or dark only") {
    for (const char* t : {"system", "light", "dark"}) {
        const PageMessage m = Parse(std::string(R"({"type":"setTheme","theme":")") + t + "\"}");
        CHECK(m.type == PageMessageType::SetTheme);
        CHECK(m.theme == t);
    }
    CHECK(Parse(R"({"type":"setTheme","theme":"blue"})").type == PageMessageType::Unknown);
    CHECK(Parse(R"({"type":"setTheme","theme":3})").type == PageMessageType::Unknown);
    CHECK(Parse(R"({"type":"setTheme"})").type == PageMessageType::Unknown);
}

TEST_CASE("ParsePageMessage: setVoice needs a Language and a Voice") {
    const PageMessage m = Parse(R"({"type":"setVoice","language":"eng","voice":"en_US-x-medium"})");
    CHECK(m.type == PageMessageType::SetVoice);
    CHECK(m.language == "eng");
    CHECK(m.voice == "en_US-x-medium");
    CHECK(Parse(R"({"type":"setVoice","language":"eng"})").type == PageMessageType::Unknown);
    CHECK(Parse(R"({"type":"setVoice","language":"eng","voice":""})").type == PageMessageType::Unknown);
    CHECK(Parse(R"({"type":"setVoice","language":5,"voice":"a"})").type == PageMessageType::Unknown);
}

TEST_CASE("ParsePageMessage: setControl needs a known control and a finite number") {
    const PageMessage m = Parse(R"({"type":"setControl","name":"pitch","value":-1.5})");
    CHECK(m.type == PageMessageType::SetControl);
    CHECK(m.name == "pitch");
    CHECK(m.value == doctest::Approx(-1.5));
    for (const char* n : {"speed", "volume"}) {
        CHECK(Parse(std::string(R"({"type":"setControl","value":1,"name":")") + n + "\"}").type ==
              PageMessageType::SetControl);
    }
    CHECK(Parse(R"({"type":"setControl","name":"loudness","value":1})").type == PageMessageType::Unknown);
    CHECK(Parse(R"({"type":"setControl","name":"expressiveness","value":1})").type == PageMessageType::Unknown);  // gone in D8.2
    CHECK(Parse(R"({"type":"setControl","name":"speed","value":"fast"})").type == PageMessageType::Unknown);
    CHECK(Parse(R"({"type":"setControl","name":"speed"})").type == PageMessageType::Unknown);
    CHECK(Parse(R"({"type":"setControl","name":"speed","value":null})").type == PageMessageType::Unknown);
}

TEST_CASE("ParsePageMessage: resetControl needs a known control") {
    const PageMessage m = Parse(R"({"type":"resetControl","name":"volume"})");
    CHECK(m.type == PageMessageType::ResetControl);
    CHECK(m.name == "volume");
    CHECK(Parse(R"({"type":"resetControl","name":"x"})").type == PageMessageType::Unknown);
}

TEST_CASE("ParsePageMessage: the Preset editing and bug report messages are gone (D8.2)") {
    CHECK(Parse(R"({"type":"savePreset","name":"Slow"})").type == PageMessageType::Unknown);
    CHECK(Parse(R"({"type":"selectPreset","name":"Slow"})").type == PageMessageType::Unknown);
    CHECK(Parse(R"({"type":"deletePreset","name":"Slow"})").type == PageMessageType::Unknown);
    CHECK(Parse(R"({"type":"reportBug"})").type == PageMessageType::Unknown);
    // setStartMinimised was gone in D8.2 too, and came back in D9 (tested with the other switches).
}

TEST_CASE("ParsePageMessage: setMasterVolume takes a finite number, clamped into 0..100") {
    PageMessage m = Parse(R"({"type":"setMasterVolume","value":42})");
    CHECK(m.type == PageMessageType::SetMasterVolume);
    CHECK(m.value == doctest::Approx(42));
    CHECK(Parse(R"({"type":"setMasterVolume","value":250})").value == doctest::Approx(100));
    CHECK(Parse(R"({"type":"setMasterVolume","value":-5})").value == doctest::Approx(0));
    CHECK(Parse(R"({"type":"setMasterVolume","value":"loud"})").type == PageMessageType::Unknown);
    CHECK(Parse(R"({"type":"setMasterVolume"})").type == PageMessageType::Unknown);
    CHECK(Parse(R"({"type":"setMasterVolume","value":null})").type == PageMessageType::Unknown);
}

TEST_CASE("ParsePageMessage: deleteVoice needs a Voice name") {
    const PageMessage m = Parse(R"({"type":"deleteVoice","voice":"en_GB-alba-medium"})");
    CHECK(m.type == PageMessageType::DeleteVoice);
    CHECK(m.voice == "en_GB-alba-medium");
    CHECK(Parse(R"({"type":"deleteVoice","voice":""})").type == PageMessageType::Unknown);
    CHECK(Parse(R"({"type":"deleteVoice"})").type == PageMessageType::Unknown);
}

TEST_CASE("ParsePageMessage: openLink takes one of the four link names, nothing else") {
    struct Case { const char* name; LinkId id; };
    for (const Case c : {Case{"mail", LinkId::Mail}, Case{"discord", LinkId::Discord}, Case{"github", LinkId::Github}, Case{"itch", LinkId::Itch}}) {
        const PageMessage m = Parse(std::string(R"({"type":"openLink","link":")") + c.name + "\"}");
        CHECK(m.type == PageMessageType::OpenLink);
        CHECK(m.link == c.id);
        CHECK(std::string(LinkName(c.id)) == c.name);
    }
    CHECK(Parse(R"({"type":"openLink","link":"https://evil.example"})").type == PageMessageType::Unknown);
    CHECK(Parse(R"({"type":"openLink","link":"twitter"})").type == PageMessageType::Unknown);
    CHECK(Parse(R"({"type":"openLink","link":5})").type == PageMessageType::Unknown);
    CHECK(Parse(R"({"type":"openLink"})").type == PageMessageType::Unknown);
}

TEST_CASE("ParsePageMessage: a name longer than 200 bytes is refused") {
    const std::string longName(201, 'a');
    CHECK(Parse(R"({"type":"deleteVoice","voice":")" + longName + R"("})").type == PageMessageType::Unknown);
    CHECK(Parse(R"({"type":"download","name":")" + longName + R"("})").type == PageMessageType::Unknown);
}

TEST_CASE("ParsePageMessage: download and cancelDownload need a Voice name") {
    PageMessage m = Parse(R"({"type":"download","name":"de_DE-thorsten-medium"})");
    CHECK(m.type == PageMessageType::Download);
    CHECK(m.name == "de_DE-thorsten-medium");
    m = Parse(R"({"type":"cancelDownload","name":"x"})");
    CHECK(m.type == PageMessageType::CancelDownload);
    CHECK(Parse(R"({"type":"download","name":""})").type == PageMessageType::Unknown);
    CHECK(Parse(R"({"type":"cancelDownload"})").type == PageMessageType::Unknown);
}

TEST_CASE("ParsePageMessage: key messages need a Key action Lexia has") {
    for (const char* a : {"read", "pause", "speech"}) {
        const PageMessage m = Parse(std::string(R"({"type":"captureKey","action":")") + a + "\"}");
        CHECK(m.type == PageMessageType::CaptureKey);
        CHECK(m.action == a);
        CHECK(Parse(std::string(R"({"type":"cancelKeyCapture","action":")") + a + "\"}").type ==
              PageMessageType::CancelKeyCapture);
    }
    CHECK(Parse(R"({"type":"captureKey","action":"language"})").type == PageMessageType::Unknown);
    CHECK(Parse(R"({"type":"captureKey"})").type == PageMessageType::Unknown);
}

TEST_CASE("ParsePageMessage: readKeyCard names one of Lexia's keys, never text (D10)") {
    const PageMessage m = Parse(R"({"type":"readKeyCard","action":"speech"})");
    CHECK(m.type == PageMessageType::ReadKeyCard);
    CHECK(m.action == "speech");
    CHECK(Parse(R"({"type":"readKeyCard","action":"Hello there"})").type == PageMessageType::Unknown);
    CHECK(Parse(R"({"type":"readKeyCard"})").type == PageMessageType::Unknown);
}

TEST_CASE("ParsePageMessage: setKey takes an action and a key text, which may be empty") {
    const PageMessage m = Parse(R"({"type":"setKey","action":"read","key":"Ctrl+Alt+R"})");
    CHECK(m.type == PageMessageType::SetKey);
    CHECK(m.action == "read");
    CHECK(m.key == "Ctrl+Alt+R");
    CHECK(Parse(R"({"type":"setKey","action":"pause","key":""})").type == PageMessageType::SetKey);
    CHECK(Parse(R"({"type":"setKey","action":"pause"})").type == PageMessageType::Unknown);
    CHECK(Parse(R"({"type":"setKey","action":"nope","key":"F1"})").type == PageMessageType::Unknown);
    CHECK(Parse(R"({"type":"setKey","action":"read","key":4})").type == PageMessageType::Unknown);
}

TEST_CASE("ParsePageMessage: unknown or malformed messages are Unknown") {
    CHECK(Parse("").type == PageMessageType::Unknown);
    CHECK(Parse("not json").type == PageMessageType::Unknown);
    CHECK(Parse("[1,2]").type == PageMessageType::Unknown);
    CHECK(Parse("42").type == PageMessageType::Unknown);
    CHECK(Parse(R"({"on":true})").type == PageMessageType::Unknown);
    CHECK(Parse(R"({"type":5})").type == PageMessageType::Unknown);
    CHECK(Parse(R"({"type":"formatDisk"})").type == PageMessageType::Unknown);
}

TEST_CASE("ParsePageMessage: Unknown keeps the type text for the log") {
    CHECK(Parse(R"({"type":"formatDisk"})").typeText == "formatDisk");
    CHECK(Parse(R"({"type":"setOn","on":"x"})").typeText == "setOn");
}

// ---------------------------------------------------------------------- links ---

TEST_CASE("LinkUrl: the fixed address of each link; the page never sends one") {
    CHECK(LinkUrl(LinkId::Mail) == "mailto:lexia@xencaye.com");
    CHECK(LinkUrl(LinkId::Discord) == "https://discord.gg/FAaZx9KVpB");
    CHECK(LinkUrl(LinkId::Itch) == "https://xencaye.itch.io/lexia");
    CHECK(LinkUrl(LinkId::Github) == "https://github.com/Xencaye/Lexia/issues/new");
}

TEST_CASE("LinkUrl: the GitHub link with the versions is BugReportUrl") {
    CHECK(LinkUrl(LinkId::Github, "0.5.0", "10.0.26300") == BugReportUrl("0.5.0", "10.0.26300"));
    CHECK(LinkUrl(LinkId::Mail, "0.5.0", "10.0.26300") == "mailto:lexia@xencaye.com");  // the versions go to GitHub only
}

// ---------------------------------------------------------------------- bug report ---

TEST_CASE("BugReportUrl: a GitHub new-issue link with only the versions, percent-encoded") {
    CHECK(BugReportUrl("0.5.0", "10.0.26300") ==
          "https://github.com/Xencaye/Lexia/issues/new?body=Lexia%200.5.0%2C%20Windows%2010.0.26300");
}

TEST_CASE("BugReportUrl: nothing but unreserved characters is left as it is") {
    const std::string url = BugReportUrl("a b&c=d#e", "f/g?h");
    const std::string body = url.substr(url.find("body=") + 5);
    CHECK(body.find_first_of(" &=#/?") == std::string::npos);
    CHECK(body.find("%26") != std::string::npos);
}

// --------------------------------------------------------------------- theme ---

TEST_CASE("IsDarkTheme: light and dark are as set; system follows Windows") {
    CHECK_FALSE(IsDarkTheme("light", /*systemUsesLight=*/false));
    CHECK(IsDarkTheme("dark", /*systemUsesLight=*/true));
    CHECK(IsDarkTheme("system", /*systemUsesLight=*/false));
    CHECK_FALSE(IsDarkTheme("system", /*systemUsesLight=*/true));
}

TEST_CASE("IsDarkTheme: a value that is not a theme is System") {
    CHECK(IsDarkTheme("", false));
    CHECK_FALSE(IsDarkTheme("purple", true));
}

// ======================================================= Voice tab (D5) ===

namespace {

InstalledVoice Installed(const std::string& name, const std::string& code, bool shipped = false) {
    return {name, code, name + ".onnx", shipped};
}

VoiceModel SampleModel() {
    VoiceModel m;
    m.languageId = "eng";
    m.installed = {Installed("en_US-ljspeech-medium", "en", true), Installed("en_GB-alba-medium", "en"),
                   Installed("bg_BG-dimitar-medium", "bg", true)};
    return m;
}

}  // namespace

TEST_CASE("FillVoiceState: the offered Languages list their installed Voices; the shipped one is active by default") {
    UiState s;
    FillVoiceState(s, SampleModel());
    REQUIRE(s.voices.count("eng") == 1);
    CHECK(s.voices.at("eng").installed == std::vector<std::string>{"en_US-ljspeech-medium", "en_GB-alba-medium"});
    CHECK(s.voices.at("eng").active == "en_US-ljspeech-medium");
    CHECK(s.voices.count("bul") == 0);  // Bulgarian is not offered (ADR-0007)
}

TEST_CASE("FillVoiceState: the chosen Voice is active, a chosen Voice that is gone is not") {
    VoiceModel m = SampleModel();
    m.activeVoices["eng"] = "en_GB-alba-medium";
    UiState s;
    FillVoiceState(s, m);
    CHECK(s.voices.at("eng").active == "en_GB-alba-medium");

    m.activeVoices["eng"] = "en_gone-medium";
    UiState t;
    FillVoiceState(t, m);
    CHECK(t.voices.at("eng").active == "en_US-ljspeech-medium");
}

TEST_CASE("FillVoiceState: removable lists the installed Voices that are not shipped") {
    UiState s;
    FillVoiceState(s, SampleModel());
    CHECK(s.voices.at("eng").removable == std::vector<std::string>{"en_GB-alba-medium"});
}

TEST_CASE("FillVoiceState: no Voice installed -> empty active, no controls, nothing removable") {
    VoiceModel m;
    m.languageId = "eng";
    UiState s;
    FillVoiceState(s, m);
    CHECK(s.voices.at("eng").installed.empty());
    CHECK(s.voices.at("eng").active.empty());
    CHECK(s.controls.empty());
    CHECK(s.voices.at("eng").removable.empty());
}

TEST_CASE("FillVoiceState: controls have the range, default and step of each control; defaults with no Preset") {
    UiState s;
    FillVoiceState(s, SampleModel());
    REQUIRE(s.controls.size() == 3);
    const UiControl& speed = s.controls.at("speed");
    CHECK(speed.value == doctest::Approx(1.0));
    CHECK(speed.min == doctest::Approx(0.5));
    CHECK(speed.max == doctest::Approx(2.0));
    CHECK(speed.def == doctest::Approx(1.0));
    CHECK(speed.step == doctest::Approx(0.05));
    CHECK(s.controls.at("pitch").min == doctest::Approx(-6.0));
    CHECK(s.controls.at("pitch").max == doctest::Approx(6.0));
    CHECK(s.controls.at("pitch").step == doctest::Approx(0.5));
    CHECK(s.controls.at("volume").max == doctest::Approx(2.0));
    CHECK(s.controls.at("volume").step == doctest::Approx(0.05));
    CHECK(s.controls.count("expressiveness") == 0);  // gone in D8.2
}

TEST_CASE("FillVoiceState: the sliders show the active Voice's controls, not another Voice's") {
    VoiceModel m = SampleModel();
    m.controls["en_US-ljspeech-medium"] = VoiceControls{0.8f, -2.f, 1.f};
    m.controls["en_GB-alba-medium"] = VoiceControls{1.5f, 3.f, 1.f};
    UiState s;
    FillVoiceState(s, m);
    CHECK(s.controls.at("speed").value == doctest::Approx(0.8));
    CHECK(s.controls.at("pitch").value == doctest::Approx(-2.0));
}

TEST_CASE("FillVoiceState: a Voice with no stored controls shows the defaults") {
    VoiceModel m = SampleModel();
    UiState s;
    FillVoiceState(s, m);
    CHECK(s.controls.at("speed").value == doctest::Approx(1.0));
    CHECK(s.controls.at("pitch").value == doctest::Approx(0.0));
    CHECK(s.controls.at("volume").value == doctest::Approx(1.0));
}

// --------------------------------------------------- the Voice library messages ---

namespace {

CatalogueVoice Cat(const std::string& name, const std::string& languageCode, uint64_t model, uint64_t config) {
    CatalogueVoice v;
    v.name = name;
    v.languageCode = languageCode;
    v.quality = "medium";
    v.model.sizeBytes = model;
    v.config.sizeBytes = config;
    return v;
}

}  // namespace

TEST_CASE("CatalogueMessage: Voices of offered Languages, with the Language id, the size of both files and installed") {
    const std::vector<CatalogueVoice> voices = {Cat("en_GB-alba-medium", "en_GB", 1000, 24),
                                                Cat("en_US-amy-medium", "en_US", 2000, 40),
                                                Cat("bg_BG-dimitar-medium", "bg_BG", 3000, 50)};
    const json m = json::parse(CatalogueMessage(voices, {"en_US-amy-medium"}));
    CHECK(m["type"] == "catalogue");
    REQUIRE(m["voices"].size() == 2);  // Bulgarian is not offered
    CHECK(m["voices"][0]["name"] == "en_GB-alba-medium");
    CHECK(m["voices"][0]["language"] == "eng");
    CHECK(m["voices"][0]["sizeBytes"] == 1024);
    CHECK(m["voices"][0]["installed"] == false);
    CHECK(m["voices"][1]["name"] == "en_US-amy-medium");
    CHECK(m["voices"][1]["sizeBytes"] == 2040);
    CHECK(m["voices"][1]["installed"] == true);
}

TEST_CASE("CatalogueFailedMessage and the download messages carry the ProblemId") {
    const json failed = json::parse(CatalogueFailedMessage(VoiceProblem::Network));
    CHECK(failed["type"] == "catalogueFailed");
    CHECK(failed["problem"] == "network");

    const json progress = json::parse(DownloadProgressMessage("v", 10, 100));
    CHECK(progress["type"] == "downloadProgress");
    CHECK(progress["name"] == "v");
    CHECK(progress["done"] == 10);
    CHECK(progress["total"] == 100);

    const json done = json::parse(DownloadDoneMessage("v"));
    CHECK(done["type"] == "downloadDone");
    CHECK(done["name"] == "v");
}

TEST_CASE("DownloadFailedMessage: the problem code, and the language name or quality when the outcome has them") {
    VoiceOutcome o;
    o.problem = VoiceProblem::UnsupportedLanguage;
    o.languageName = "German";
    json m = json::parse(DownloadFailedMessage("de_DE-x-medium", o));
    CHECK(m["type"] == "downloadFailed");
    CHECK(m["name"] == "de_DE-x-medium");
    CHECK(m["problem"] == "unsupported-language");
    CHECK(m["language"] == "German");
    CHECK_FALSE(m.contains("quality"));

    VoiceOutcome low;
    low.problem = VoiceProblem::NotMedium;
    low.quality = "low";
    m = json::parse(DownloadFailedMessage("v", low));
    CHECK(m["problem"] == "not-medium");
    CHECK(m["quality"] == "low");
    CHECK_FALSE(m.contains("language"));

    VoiceOutcome cancelled;
    cancelled.problem = VoiceProblem::Cancelled;
    CHECK(json::parse(DownloadFailedMessage("v", cancelled))["problem"] == "cancelled");
}

TEST_CASE("VoiceAddedMessage names the Voice and the Lexia Language id; VoiceRefusedMessage the problem") {
    VoiceOutcome ok;
    ok.voiceName = "en_US-amy-medium";
    ok.languageCode = "en";
    const json added = json::parse(VoiceAddedMessage(ok));
    CHECK(added["type"] == "voiceAdded");
    CHECK(added["voice"] == "en_US-amy-medium");
    CHECK(added["language"] == "eng");

    VoiceOutcome bad;
    bad.problem = VoiceProblem::BadZip;
    const json refused = json::parse(VoiceRefusedMessage(bad));
    CHECK(refused["type"] == "voiceRefused");
    CHECK(refused["problem"] == "bad-zip");
}

// ----------------------------------------------- D8: delete Voice and Preset file answers ---

TEST_CASE("VoiceDeleteFailedMessage: the Voice and the ProblemId") {
    const json m = json::parse(VoiceDeleteFailedMessage("en_GB-alba-medium", VoiceProblem::CannotWrite));
    CHECK(m["type"] == "voiceDeleteFailed");
    CHECK(m["voice"] == "en_GB-alba-medium");
    CHECK(m["problem"] == ProblemId(VoiceProblem::CannotWrite));
}

TEST_CASE("PresetSavedMessage and PresetLoadedMessage carry the file's name") {
    const json saved = json::parse(PresetSavedMessage("calm.json"));
    CHECK(saved["type"] == "presetSaved");
    CHECK(saved["name"] == "calm.json");
    const json loaded = json::parse(PresetLoadedMessage("calm.json"));
    CHECK(loaded["type"] == "presetLoaded");
    CHECK(loaded["name"] == "calm.json");
}

TEST_CASE("PresetFailedMessage: badFile, cannotWrite or noVoice") {
    CHECK(json::parse(PresetFailedMessage(PresetProblem::BadFile))["problem"] == "badFile");
    CHECK(json::parse(PresetFailedMessage(PresetProblem::CannotWrite))["problem"] == "cannotWrite");
    const json m = json::parse(PresetFailedMessage(PresetProblem::NoVoice));
    CHECK(m["type"] == "presetFailed");
    CHECK(m["problem"] == "noVoice");
}
