// Unit tests for ui_host.h: the fixed links, the theme choice and the Voice tab's part of the state
// (FillVoiceState). The messages between the Settings window and the host are typed (PageMessage,
// VoiceEventSink), so there is no text to parse here.
#include <doctest/doctest.h>

#include <string>

#include "ui_host.h"


// ---------------------------------------------------------------------- links ---

TEST_CASE("LinkUrl: the fixed address of each link; the window never builds one") {
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
