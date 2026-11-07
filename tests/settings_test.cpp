// Unit tests for settings.h: ParseHotkey and ParseSettings (the pure parse step of
// LoadSettings; they touch neither the file system nor the logger). The ChooseSettingsFile tests use
// temporary folders.
// Notes are asserted on by the key name they mention, not by their full wording.
#include <doctest/doctest.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "settings.h"

namespace {

struct Parsed {
    Settings settings;
    std::vector<std::string> notes;
};

Parsed Parse(const std::string& json) {
    Parsed p;
    p.settings = ParseSettings(json, p.notes);
    return p;
}

bool Mentions(const std::vector<std::string>& notes, const std::string& what) {
    for (const std::string& n : notes) {
        if (n.find(what) != std::string::npos) return true;
    }
    return false;
}

}  // namespace

TEST_CASE("ParseHotkey: function keys work without a modifier") {
    Hotkey h;
    CHECK(ParseHotkey("F8", h));
    CHECK(h.mods == 0);
    CHECK(h.vk == static_cast<UINT>(VK_F8));
    CHECK(h.text == "F8");

    CHECK(ParseHotkey("f1", h));
    CHECK(h.vk == static_cast<UINT>(VK_F1));
    CHECK(ParseHotkey("F24", h));
    CHECK(h.vk == static_cast<UINT>(VK_F24));
}

TEST_CASE("ParseHotkey: F0 and F25 are not keys") {
    Hotkey h;
    CHECK_FALSE(ParseHotkey("F0", h));
    CHECK_FALSE(ParseHotkey("F25", h));
}

TEST_CASE("ParseHotkey: modifier combinations, any case, any order, spaces around parts") {
    Hotkey h;
    CHECK(ParseHotkey("Ctrl+Alt+R", h));
    CHECK(h.mods == (MOD_CONTROL | MOD_ALT));
    CHECK(h.vk == static_cast<UINT>('R'));
    CHECK(h.text == "Ctrl+Alt+R");

    CHECK(ParseHotkey("shift+F9", h));
    CHECK(h.mods == MOD_SHIFT);
    CHECK(h.vk == static_cast<UINT>(VK_F9));

    CHECK(ParseHotkey("F9 + Control + Windows", h));
    CHECK(h.mods == (MOD_CONTROL | MOD_WIN));
    CHECK(h.vk == static_cast<UINT>(VK_F9));
}

TEST_CASE("ParseHotkey: letters and digits need a modifier") {
    Hotkey h;
    CHECK_FALSE(ParseHotkey("R", h));
    CHECK_FALSE(ParseHotkey("5", h));
    CHECK_FALSE(ParseHotkey("Shift", h));  // a modifier alone is not a key

    CHECK(ParseHotkey("Ctrl+5", h));
    CHECK(h.vk == static_cast<UINT>('5'));
    CHECK(h.mods == MOD_CONTROL);
}

TEST_CASE("ParseHotkey: refuses garbage, two main keys and empty parts") {
    Hotkey h;
    CHECK_FALSE(ParseHotkey("", h));
    CHECK_FALSE(ParseHotkey("Ctrl+Banana", h));
    CHECK_FALSE(ParseHotkey("F8+F9", h));
    CHECK_FALSE(ParseHotkey("Ctrl+R+T", h));
    CHECK_FALSE(ParseHotkey("Ctrl++R", h));
    CHECK_FALSE(ParseHotkey("Ctrl+", h));
}

TEST_CASE("ParseHotkey: a refused text leaves the output unchanged") {
    Hotkey h = {MOD_ALT, 'Q', "Alt+Q"};
    CHECK_FALSE(ParseHotkey("Ctrl+Banana", h));
    CHECK(h.mods == MOD_ALT);
    CHECK(h.vk == static_cast<UINT>('Q'));
    CHECK(h.text == "Alt+Q");
}

TEST_CASE("ParseHotkey: named keys; only some are safe without a modifier") {
    Hotkey h;
    CHECK(ParseHotkey("Pause", h));
    CHECK(h.vk == static_cast<UINT>(VK_PAUSE));
    CHECK_FALSE(ParseHotkey("Space", h));  // typing would break
    CHECK(ParseHotkey("Ctrl+Space", h));
    CHECK(h.vk == static_cast<UINT>(VK_SPACE));
}

TEST_CASE("Hotkey::Label: the key text, or (no key) when unbound") {
    Hotkey none;
    CHECK(!none.IsBound());
    CHECK(none.Label() == "(no key)");
    CHECK(Settings().read.Label() == "F8");
    CHECK(Settings().pause.Label() == "F7");
    CHECK(Settings().speech.Label() == "F6");
    CHECK(Settings().switchLanguage.Label() == "(no key)");  // unbound until the user picks one
}

TEST_CASE("ParseSettings: empty object gives the defaults and no notes") {
    const Parsed p = Parse("{}");
    const Settings d;
    CHECK(p.notes.empty());
    CHECK(p.settings.read.vk == d.read.vk);
    CHECK(p.settings.pause.vk == d.pause.vk);
    CHECK(p.settings.speech.vk == d.speech.vk);
    CHECK(p.settings.switchLanguage.vk == d.switchLanguage.vk);
    CHECK(p.settings.language == "eng");
    CHECK(p.settings.speechRate == 0);
    CHECK(p.settings.speechVolume == 100);
    CHECK(p.settings.hoverColor == d.hoverColor);
    CHECK(p.settings.pickedColor == d.pickedColor);
}

TEST_CASE("ParseSettings: a full valid file is applied, with // comments allowed") {
    const Parsed p = Parse(R"({
        // my keys
        "hotkeys": {"read": "Ctrl+Alt+R", "pause": "F9", "speech": "F11", "switchLanguage": "Shift+F2"},
        "language": "bul",
        "speech": {"rate": -3, "volume": 40},
        "overlay": {"hoverColor": "#112233", "pickedColor": "abcdef"}
    })");
    CHECK(p.notes.empty());
    CHECK(p.settings.read.mods == (MOD_CONTROL | MOD_ALT));
    CHECK(p.settings.read.vk == static_cast<UINT>('R'));
    CHECK(p.settings.pause.vk == static_cast<UINT>(VK_F9));
    CHECK(p.settings.speech.vk == static_cast<UINT>(VK_F11));
    CHECK(p.settings.switchLanguage.mods == MOD_SHIFT);
    CHECK(p.settings.switchLanguage.vk == static_cast<UINT>(VK_F2));
    CHECK(p.settings.language == "bul");
    CHECK(p.settings.speechRate == -3);
    CHECK(p.settings.speechVolume == 40);
    CHECK(p.settings.hoverColor == RGB(0x11, 0x22, 0x33));
    CHECK(p.settings.pickedColor == RGB(0xAB, 0xCD, 0xEF));
}

TEST_CASE("ParseSettings: bad hotkey text -> note, default for that key only") {
    const Parsed p = Parse(R"({"hotkeys": {"read": "R", "pause": "F9"}})");
    CHECK(p.notes.size() == 1);
    CHECK(Mentions(p.notes, "hotkeys.read"));
    CHECK(p.settings.read.vk == static_cast<UINT>(VK_F8));  // default
    CHECK(p.settings.pause.vk == static_cast<UINT>(VK_F9));  // the good one is kept
}

TEST_CASE("ParseSettings: a hotkey that is not a string -> note, default") {
    const Parsed p = Parse(R"({"hotkeys": {"speech": 6}})");
    CHECK(p.notes.size() == 1);
    CHECK(Mentions(p.notes, "hotkeys.speech"));
    CHECK(p.settings.speech.vk == Settings().speech.vk);
}

TEST_CASE("ParseSettings: out-of-range or non-integer rate/volume -> note, default") {
    SUBCASE("rate too high") {
        const Parsed p = Parse(R"({"speech": {"rate": 11, "volume": 50}})");
        CHECK(p.notes.size() == 1);
        CHECK(Mentions(p.notes, "speech.rate"));
        CHECK(p.settings.speechRate == 0);
        CHECK(p.settings.speechVolume == 50);  // the other value is untouched
    }
    SUBCASE("rate too low") {
        const Parsed p = Parse(R"({"speech": {"rate": -11}})");
        CHECK(Mentions(p.notes, "speech.rate"));
        CHECK(p.settings.speechRate == 0);
    }
    SUBCASE("volume out of range") {
        const Parsed hi = Parse(R"({"speech": {"volume": 101}})");
        CHECK(Mentions(hi.notes, "speech.volume"));
        CHECK(hi.settings.speechVolume == 100);
        const Parsed lo = Parse(R"({"speech": {"volume": -1}})");
        CHECK(Mentions(lo.notes, "speech.volume"));
        CHECK(lo.settings.speechVolume == 100);
    }
    SUBCASE("the ends of the range are fine") {
        const Parsed p = Parse(R"({"speech": {"rate": 10, "volume": 0}})");
        CHECK(p.notes.empty());
        CHECK(p.settings.speechRate == 10);
        CHECK(p.settings.speechVolume == 0);
    }
    SUBCASE("not a whole number") {
        const Parsed p = Parse(R"({"speech": {"rate": 2.5, "volume": "loud"}})");
        CHECK(p.notes.size() == 2);
        CHECK(p.settings.speechRate == 0);
        CHECK(p.settings.speechVolume == 100);
    }
    SUBCASE("a whole number above INT64_MAX does not wrap into the range") {
        // 2^64 - 1 would read as -1 through a signed 64-bit integer, and 2^63 as INT64_MIN.
        const Parsed p = Parse(R"({"speech": {"rate": 18446744073709551615, "volume": 9223372036854775808}})");
        CHECK(p.notes.size() == 2);
        CHECK(Mentions(p.notes, "speech.rate"));
        CHECK(Mentions(p.notes, "speech.volume"));
        CHECK(p.settings.speechRate == 0);
        CHECK(p.settings.speechVolume == 100);
    }
}

TEST_CASE("ParseSettings: a bad colour -> note, default for that colour only") {
    const Parsed p = Parse(R"({"overlay": {"hoverColor": "red", "pickedColor": "#000001"}})");
    CHECK(p.notes.size() == 1);
    CHECK(Mentions(p.notes, "overlay.hoverColor"));
    CHECK(p.settings.hoverColor == Settings{}.hoverColor);
    CHECK(p.settings.pickedColor == RGB(0x00, 0x00, 0x01));

    CHECK(Mentions(Parse(R"({"overlay": {"pickedColor": "#12345"}})").notes, "overlay.pickedColor"));
    CHECK(Mentions(Parse(R"({"overlay": {"pickedColor": "#12345G"}})").notes, "overlay.pickedColor"));
    CHECK(Mentions(Parse(R"({"overlay": {"pickedColor": 123456}})").notes, "overlay.pickedColor"));
}

TEST_CASE("ParseSettings: an unknown Language id -> note, default Language") {
    const Parsed p = Parse(R"({"language": "klingon", "speech": {"rate": 4}})");
    CHECK(p.notes.size() == 1);
    CHECK(Mentions(p.notes, "language"));
    CHECK(p.settings.language == "eng");
    CHECK(p.settings.speechRate == 4);  // the rest of the file still applies
}

TEST_CASE("ParseSettings: malformed JSON -> one note, all defaults") {
    const Parsed p = Parse(R"({"hotkeys": {"read": "F9"})");  // missing closing brace
    CHECK(p.notes.size() == 1);
    CHECK(p.settings.read.vk == static_cast<UINT>(VK_F8));
    CHECK(p.settings.speechVolume == 100);

    CHECK(Parse("").notes.size() == 1);
    CHECK(Parse("not json at all").notes.size() == 1);
}

TEST_CASE("ParseSettings: unknown keys and sections are ignored") {
    const Parsed p = Parse(R"({"colour": "blue", "speech": {"pitch": 3}})");
    CHECK(p.notes.empty());
}

// ---------------------------------------------------------------- R35: duplicate keys ---
// Actions are compared in the order read, pause, speech, switchLanguage. The later action of a
// clash falls back to its own default; if that default is taken too, it stays unbound.

TEST_CASE("ParseSettings: the default keys do not clash with each other") {
    const Settings d;
    CHECK(d.read.IsBound());
    CHECK(Parse("{}").notes.empty());
}

TEST_CASE("ParseSettings: the second action on a key gets a note and its default") {
    // pause is set to read's key; pause falls back to its default (F7).
    const Parsed p = Parse(R"({"hotkeys": {"read": "F9", "pause": "F9"}})");
    CHECK(p.notes.size() == 1);
    CHECK(Mentions(p.notes, "hotkeys.pause"));
    CHECK(p.settings.read.vk == static_cast<UINT>(VK_F9));  // the earlier action keeps the key
    CHECK(p.settings.pause.vk == static_cast<UINT>(VK_F7));
    CHECK(p.settings.pause.IsBound());
}

TEST_CASE("ParseSettings: a clash needs the same modifiers as well as the same key") {
    const Parsed p = Parse(R"({"hotkeys": {"read": "Ctrl+F9", "pause": "F9", "speech": "Shift+F9"}})");
    CHECK(p.notes.empty());
    CHECK(p.settings.read.mods == MOD_CONTROL);
    CHECK(p.settings.pause.mods == 0);
    CHECK(p.settings.speech.mods == MOD_SHIFT);
}

TEST_CASE("ParseSettings: modifier order and case do not hide a clash") {
    const Parsed p = Parse(R"({"hotkeys": {"read": "Ctrl+Alt+R", "pause": "alt+ctrl+r"}})");
    CHECK(p.notes.size() == 1);
    CHECK(Mentions(p.notes, "hotkeys.pause"));
    CHECK(p.settings.pause.mods == 0);
    CHECK(p.settings.pause.vk == static_cast<UINT>(VK_F7));  // its default
}

TEST_CASE("ParseSettings: a clash with the later action's default is not a clash yet") {
    // read takes F7 (pause's default) and pause moves away, so nothing clashes.
    const Parsed p = Parse(R"({"hotkeys": {"read": "F7", "pause": "F9"}})");
    CHECK(p.notes.empty());
    CHECK(p.settings.read.vk == static_cast<UINT>(VK_F7));
    CHECK(p.settings.pause.vk == static_cast<UINT>(VK_F9));
}

TEST_CASE("ParseSettings: if the default also clashes, the action is left unbound") {
    // read = F7, pause = F7: pause clashes, and its default (F7) is read's key too.
    const Parsed p = Parse(R"({"hotkeys": {"read": "F7", "pause": "F7"}})");
    CHECK(p.notes.size() == 1);
    CHECK(Mentions(p.notes, "hotkeys.pause"));
    CHECK(Mentions(p.notes, "unbound"));
    CHECK(p.settings.read.vk == static_cast<UINT>(VK_F7));
    CHECK_FALSE(p.settings.pause.IsBound());
    CHECK(p.settings.pause.vk == 0);
    // The other actions are untouched.
    CHECK(p.settings.speech.vk == Settings().speech.vk);
    CHECK(p.settings.speech.IsBound());
}

TEST_CASE("ParseSettings: an unbound action does not stop later actions being checked") {
    // pause ends up unbound, but speech = F7 still clashes with read; speech gets its default.
    const Parsed p = Parse(R"({"hotkeys": {"read": "F7", "pause": "F7", "speech": "F7"}})");
    CHECK(p.notes.size() == 2);
    CHECK_FALSE(p.settings.pause.IsBound());
    CHECK(p.settings.speech.vk == Settings().speech.vk);
}

TEST_CASE("ParseSettings: three-way clash resolves in order") {
    const Parsed p = Parse(R"({"hotkeys": {"read": "F6", "pause": "F6", "speech": "F6"}})");
    // pause -> its default F7 (free). speech -> its default F6 is read's key -> unbound.
    CHECK(p.notes.size() == 2);
    CHECK(p.settings.read.vk == static_cast<UINT>(VK_F6));
    CHECK(p.settings.pause.vk == static_cast<UINT>(VK_F7));
    CHECK_FALSE(p.settings.speech.IsBound());
}

TEST_CASE("ParseSettings: the defaults are F8 read, F7 pause, F6 speech, no Language key") {
    const Settings d;
    CHECK(d.read.vk == static_cast<UINT>(VK_F8));
    CHECK(d.pause.vk == static_cast<UINT>(VK_F7));
    CHECK(d.speech.vk == static_cast<UINT>(VK_F6));
    CHECK_FALSE(d.switchLanguage.IsBound());
    CHECK(Parse("{}").notes.empty());
}

TEST_CASE("ParseSettings: an empty key text leaves the action unbound, without a note") {
    const Parsed p = Parse(R"({"hotkeys": {"switchLanguage": "", "pause": ""}})");
    CHECK(p.notes.empty());
    CHECK_FALSE(p.settings.switchLanguage.IsBound());
    CHECK_FALSE(p.settings.pause.IsBound());  // explicit "" overrides the default F7
    CHECK(p.settings.speech.vk == static_cast<UINT>(VK_F6));
}

TEST_CASE("ParseSettings: the Language key can still be bound in settings.json") {
    const Parsed p = Parse(R"({"hotkeys": {"switchLanguage": "Ctrl+Alt+L"}})");
    CHECK(p.notes.empty());
    CHECK(p.settings.switchLanguage.vk == static_cast<UINT>('L'));
    CHECK(p.settings.switchLanguage.mods == (MOD_CONTROL | MOD_ALT));
}

TEST_CASE("ParseSettings: a Language key on another action's key is left unbound (it has no default)") {
    const Parsed p = Parse(R"({"hotkeys": {"switchLanguage": "F8"}})");
    CHECK(p.notes.size() == 1);
    CHECK(Mentions(p.notes, "hotkeys.switchLanguage"));
    CHECK(Mentions(p.notes, "unbound"));
    CHECK_FALSE(p.settings.switchLanguage.IsBound());
    CHECK(p.settings.read.vk == static_cast<UINT>(VK_F8));
}

TEST_CASE("ParseSettings: an old file with hotkeys.quit loads without a note and the key is ignored") {
    // Lexia has no Quit key any more (quitting is from the tray). An old file may still say
    // quit = F6, which is the Speech key now: that must not clash or make a note, like "stop".
    const Parsed p = Parse(R"({"hotkeys": {"read": "F8", "stop": "F7", "quit": "F6", "switchLanguage": "Ctrl+Alt+L"}})");
    CHECK(p.notes.empty());
    CHECK(p.settings.speech.vk == static_cast<UINT>(VK_F6));
    CHECK(p.settings.read.vk == static_cast<UINT>(VK_F8));
    CHECK(p.settings.switchLanguage.vk == static_cast<UINT>('L'));
}

TEST_CASE("ParseSettings: a bad hotkey that falls back to a taken default is a clash too") {
    // pause is invalid, so it keeps its default F7, but read already took F7.
    const Parsed p = Parse(R"({"hotkeys": {"read": "F7", "pause": "R"}})");
    CHECK_FALSE(p.settings.pause.IsBound());
    CHECK(Mentions(p.notes, "hotkeys.pause"));
}

TEST_CASE("ParseSettings: enabled is Off by default (the first start) and a boolean sets it") {
    CHECK_FALSE(Parse("{}").settings.enabled);
    CHECK_FALSE(Parse(R"({"enabled": false})").settings.enabled);
    CHECK(Parse(R"({"enabled": true})").settings.enabled);
}

TEST_CASE("ParseSettings: a non-boolean enabled -> note, keeps the default (Off)") {
    const Parsed p = Parse(R"({"enabled": "yes"})");
    CHECK(p.notes.size() == 1);
    CHECK(Mentions(p.notes, "enabled"));
    CHECK_FALSE(p.settings.enabled);
}

TEST_CASE("UpdateSettingsText: changes only the one value, keeps order and the other values") {
    std::string text = "{\n  \"hotkeys\": {\"read\": \"F9\"},\n  \"language\": \"eng\",\n"
                       "  \"speech\": {\"rate\": 3, \"volume\": 40}\n}\n";
    std::string note;
    REQUIRE(UpdateSettingsText(text, "language", "\"bul\"", note));
    const Parsed p = Parse(text);
    CHECK(p.notes.empty());
    CHECK(p.settings.language == "bul");
    CHECK(p.settings.read.vk == static_cast<UINT>(VK_F9));
    CHECK(p.settings.speechRate == 3);
    CHECK(p.settings.speechVolume == 40);
    CHECK(text.find("\"hotkeys\"") < text.find("\"language\""));
    CHECK(text.find("\"language\"") < text.find("\"speech\""));
}

TEST_CASE("UpdateSettingsText: keeps four-space and tab indentation") {
    std::string note;
    std::string four = "{\n    \"language\": \"eng\"\n}\n";
    REQUIRE(UpdateSettingsText(four, "language", "\"bul\"", note));
    CHECK(four == "{\n    \"language\": \"bul\"\n}\n");

    std::string tab = "{\n\t\"language\": \"eng\"\n}\n";
    REQUIRE(UpdateSettingsText(tab, "language", "\"bul\"", note));
    CHECK(tab == "{\n\t\"language\": \"bul\"\n}\n");
}

TEST_CASE("UpdateSettingsText: adds a missing key, and a missing section for section.name") {
    std::string note;
    std::string text = "{\n  \"language\": \"eng\"\n}\n";
    REQUIRE(UpdateSettingsText(text, "enabled", "false", note));
    CHECK_FALSE(Parse(text).settings.enabled);
    REQUIRE(UpdateSettingsText(text, "speech.rate", "5", note));
    CHECK(Parse(text).settings.speechRate == 5);
    CHECK(Parse(text).settings.language == "eng");
}

TEST_CASE("UpdateSettingsText: a file with // comments, bad JSON or a non-object is left untouched") {
    std::string note;
    for (const char* original : {"{\n  // my note\n  \"language\": \"eng\"\n}\n", "{ not json",
                                 "[1, 2]"}) {
        std::string text = original;
        note.clear();
        CHECK_FALSE(UpdateSettingsText(text, "language", "\"bul\"", note));
        CHECK(text == original);
        CHECK_FALSE(note.empty());
    }
}

namespace {

// A fresh folder under the system temp folder, removed at the end of the test.
struct TempDirs {
    std::filesystem::path root, appData, exe;
    TempDirs() {
        root = std::filesystem::temp_directory_path() /
               ("lexia_settings_test_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)));
        appData = root / "appdata" / "Lexia";
        exe = root / "exe";
        std::filesystem::create_directories(appData);
        std::filesystem::create_directories(exe);
    }
    ~TempDirs() {
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }
};

void Touch(const std::filesystem::path& p) { std::ofstream(p) << "{}"; }

}  // namespace

TEST_CASE("ChooseSettingsFile: neither file exists -> the appdata one, to be created") {
    TempDirs d;
    CHECK(ChooseSettingsFile(d.appData, d.exe) == d.appData / "settings.json");
}

TEST_CASE("ChooseSettingsFile: only the exe folder has one -> it is used") {
    TempDirs d;
    Touch(d.exe / "settings.json");
    CHECK(ChooseSettingsFile(d.appData, d.exe) == d.exe / "settings.json");
}

TEST_CASE("ChooseSettingsFile: the appdata file wins when both exist") {
    TempDirs d;
    Touch(d.exe / "settings.json");
    Touch(d.appData / "settings.json");
    CHECK(ChooseSettingsFile(d.appData, d.exe) == d.appData / "settings.json");
}

TEST_CASE("ChooseSettingsFile: the appdata folder itself need not exist") {
    TempDirs d;
    std::filesystem::remove_all(d.appData);
    CHECK(ChooseSettingsFile(d.appData, d.exe) == d.appData / "settings.json");
}

TEST_CASE("ParseSettings: wellFormed is false only for text that is not JSON") {
    std::vector<std::string> notes;
    bool wellFormed = true;
    ParseSettings(R"({"hotkeys": {"read": "F9"})", notes, "x", &wellFormed);
    CHECK_FALSE(wellFormed);

    notes.clear();
    ParseSettings("", notes, "x", &wellFormed);
    CHECK_FALSE(wellFormed);

    // Valid JSON with a bad value is still well formed: that value alone goes back to its default.
    notes.clear();
    ParseSettings(R"({"speech": {"rate": 99}})", notes, "x", &wellFormed);
    CHECK(wellFormed);
    CHECK(notes.size() == 1);

    notes.clear();
    ParseSettings("{}", notes, "x", &wellFormed);
    CHECK(wellFormed);
}

namespace {

// {"language": "bul", "deep": [[[...]]]} with `depth` nested arrays.
std::string DeeplyNested(int depth) {
    return R"({"language": "bul", "deep": )" + std::string(static_cast<size_t>(depth), '[') +
           std::string(static_cast<size_t>(depth), ']') + "}";
}

}  // namespace

TEST_CASE("ParseSettings: a file nested too deep is not JSON: one note, all defaults") {
    // Saving it again would overflow the stack in the recursive dump().
    std::vector<std::string> notes;
    bool wellFormed = true;
    const Settings s = ParseSettings(DeeplyNested(1000), notes, "x", &wellFormed);
    CHECK_FALSE(wellFormed);
    CHECK(notes.size() == 1);
    CHECK(s.language == "eng");  // the "bul" beside it is not applied either

    // A little nesting, as a real file has, is fine.
    notes.clear();
    const Settings shallow = ParseSettings(DeeplyNested(10), notes, "x", &wellFormed);
    CHECK(wellFormed);
    CHECK(notes.empty());
    CHECK(shallow.language == "bul");
}

TEST_CASE("UpdateSettingsText: a file nested too deep is left untouched") {
    const std::string original = DeeplyNested(1000);
    std::string text = original;
    std::string note;
    CHECK_FALSE(UpdateSettingsText(text, "language", "\"eng\"", note));
    CHECK(text == original);
    CHECK_FALSE(note.empty());

    std::string shallow = DeeplyNested(10);
    CHECK(UpdateSettingsText(shallow, "language", "\"eng\"", note));
}

TEST_CASE("Settings: equality compares every value") {
    const Settings a;
    CHECK(a == Settings{});

    Settings b;
    b.language = "bul";
    CHECK(a != b);
    b = a;
    b.enabled = !a.enabled;
    CHECK(a != b);
    b = a;
    b.startMinimised = !a.startMinimised;
    CHECK(a != b);
    b = a;
    b.read.vk = VK_F9;
    CHECK(a != b);
    b = a;
    b.speechRate = 1;
    CHECK(a != b);
    b = a;
    b.hoverColor = RGB(1, 2, 3);
    CHECK(a != b);

    // The same file parses to equal Settings: what the live reload relies on to skip no-ops.
    CHECK(Parse(R"({"language": "bul"})").settings == Parse(R"({ "language" : "bul" })").settings);
}
TEST_CASE("ParseSettings: the layout knobs default to the tuned values") {
    const Parsed p = Parse("{}");
    const LayoutParams d;
    CHECK(p.settings.layout.paragraphGap == doctest::Approx(d.paragraphGap));
    CHECK(p.settings.layout.columnGap == doctest::Approx(d.columnGap));
    CHECK(p.settings.layout.listLine == doctest::Approx(d.listLine));
}

TEST_CASE("ParseSettings: layout knobs are applied, whole or fractional numbers") {
    const Parsed p = Parse(R"({"layout": {"paragraphGap": 1.2, "columnGap": 4, "listLine": 0}})");
    CHECK(p.notes.empty());
    CHECK(p.settings.layout.paragraphGap == doctest::Approx(1.2));
    CHECK(p.settings.layout.columnGap == doctest::Approx(4.0));
    CHECK(p.settings.layout.listLine == doctest::Approx(0.0));
}

TEST_CASE("ParseSettings: a layout knob out of range or not a number -> note, default for that knob") {
    SUBCASE("below and above the range") {
        const Parsed p = Parse(R"({"layout": {"paragraphGap": 0.1, "columnGap": 25, "listLine": 3}})");
        CHECK(p.notes.size() == 2);
        CHECK(Mentions(p.notes, "layout.paragraphGap"));
        CHECK(Mentions(p.notes, "layout.columnGap"));
        CHECK(p.settings.layout.paragraphGap == doctest::Approx(LayoutParams().paragraphGap));
        CHECK(p.settings.layout.columnGap == doctest::Approx(LayoutParams().columnGap));
        CHECK(p.settings.layout.listLine == doctest::Approx(3.0));  // the good one is kept
    }
    SUBCASE("a string") {
        const Parsed p = Parse(R"({"layout": {"listLine": "8"}})");
        CHECK(p.notes.size() == 1);
        CHECK(Mentions(p.notes, "layout.listLine"));
        CHECK(p.settings.layout.listLine == doctest::Approx(LayoutParams().listLine));
    }
    SUBCASE("range ends are accepted") {
        const Parsed p = Parse(R"({"layout": {"paragraphGap": 0.3, "columnGap": 20, "listLine": 30}})");
        CHECK(p.notes.empty());
        CHECK(p.settings.layout.paragraphGap == doctest::Approx(0.3));
        CHECK(p.settings.layout.columnGap == doctest::Approx(20.0));
        CHECK(p.settings.layout.listLine == doctest::Approx(30.0));
    }
}

TEST_CASE("SettingsToText: what is written is read back, layout section included") {
    Settings s;
    s.language = "bul";
    s.speechRate = 4;
    s.layout.paragraphGap = 1.25;
    s.layout.columnGap = 7.5;
    s.layout.listLine = 12;
    const Parsed p = Parse(SettingsToText(s));
    CHECK(p.notes.empty());
    CHECK(p.settings.language == "bul");
    CHECK(p.settings.speechRate == 4);
    CHECK(p.settings.layout.paragraphGap == doctest::Approx(1.25));
    CHECK(p.settings.layout.columnGap == doctest::Approx(7.5));
    CHECK(p.settings.layout.listLine == doctest::Approx(12.0));
}

TEST_CASE("SettingsToText: the default file has a layout section with the default knobs") {
    const std::string text = SettingsToText(Settings());
    CHECK(text.find("\"layout\"") != std::string::npos);
    CHECK(text.find("\"paragraphGap\"") != std::string::npos);
    CHECK(Parse(text).notes.empty());
}

TEST_CASE("interfaceLanguage: English unless settings.json says another one Lexia has") {
    CHECK(Settings{}.interfaceLanguage == "en");
    CHECK(Parse("{}").settings.interfaceLanguage == "en");

    const Parsed bg = Parse(R"({"interfaceLanguage": "bg"})");
    CHECK(bg.settings.interfaceLanguage == "bg");
    CHECK(bg.notes.empty());
}

TEST_CASE("interfaceLanguage: a bad value is a note and the default, the rest is kept") {
    for (const char* bad : {"\"de\"", "\"BG\"", "\"bul\"", "5", "null", "true", "\"\""}) {
        const Parsed p =
            Parse(std::string(R"({"speech": {"rate": 3}, "interfaceLanguage": )") + bad + "}");
        INFO("value: ", bad);
        CHECK(p.settings.interfaceLanguage == "en");
        CHECK(Mentions(p.notes, "interfaceLanguage"));
        CHECK(p.settings.speechRate == 3);
    }
}

TEST_CASE("KeepToOffered: a Language or Interface language not offered yet becomes the default, with a note") {
    Settings s = Parse(R"({"language": "bul", "interfaceLanguage": "bg", "speech": {"rate": 3}})").settings;
    REQUIRE(s.language == "bul");  // ParseSettings reads the file as it is written
    const std::vector<std::string> notes = KeepToOffered(s);
    CHECK(s.language == "eng");
    CHECK(s.interfaceLanguage == "en");
    CHECK(s.speechRate == 3);  // the rest is kept
    CHECK(notes.size() == 2);
    CHECK(Mentions(notes, "language \"bul\""));
    CHECK(Mentions(notes, "interfaceLanguage \"bg\""));

    Settings english;
    CHECK(KeepToOffered(english).empty());
    CHECK(english == Settings{});
}

TEST_CASE("interfaceLanguage takes part in settings equality") {
    Settings a, b;
    CHECK(a == b);
    b.interfaceLanguage = "bg";
    CHECK(a != b);
}

// ----------------------------------------------------- Voice controls ---

TEST_CASE("ParseSettings: no voices section means every Voice has the default controls") {
    const Parsed p = Parse("{}");
    CHECK(p.settings.voices.empty());
    CHECK(p.notes.empty());
    CHECK(Settings{}.ControlsFor("en_US-ljspeech-medium") == VoiceControls{});
}

TEST_CASE("ParseSettings: the current controls per Voice, a left-out control is the default") {
    const Parsed p = Parse(R"({"voices": {
        "en_US-ljspeech-medium": {"speed": 0.8, "pitch": -2, "volume": 0.9, "expressiveness": 0.7},
        "other": {"speed": 1.5},
        "empty": {}}})");
    CHECK(p.notes.empty());  // the old "expressiveness" is ignored silently
    const VoiceControls& c = p.settings.voices.at("en_US-ljspeech-medium");
    CHECK(c.speed == doctest::Approx(0.8f));
    CHECK(c.pitch == doctest::Approx(-2.f));
    CHECK(c.volume == doctest::Approx(0.9f));
    CHECK(p.settings.voices.at("other").speed == doctest::Approx(1.5f));
    CHECK(p.settings.voices.at("other").pitch == 0.f);
    CHECK(p.settings.voices.at("empty") == VoiceControls{});
    CHECK(p.settings.ControlsFor("missing") == VoiceControls{});
}

TEST_CASE("ParseSettings: an older file's active Preset becomes the current controls, with no note") {
    const Parsed p = Parse(R"({"voices": {
        "a": {"preset": "Calm", "presets": {
            "Calm": {"speed": 0.8, "pitch": -2, "volume": 0.9, "expressiveness": 0.7},
            "Quick": {"speed": 1.5}}},
        "b": {"preset": "Gone", "presets": {"A": {"speed": 1.2}}},
        "c": {"presets": {"A": {"speed": 1.2}}},
        "d": {"preset": " Padded ", "presets": {"Padded": {"pitch": 3}}},
        "e": {"preset": "Bad", "presets": {"Bad": {"speed": 9, "pitch": "high", "volume": 0.5}, "X": 7}}}})");
    CHECK(p.notes.empty());
    const VoiceControls& a = p.settings.voices.at("a");
    CHECK(a.speed == doctest::Approx(0.8f));
    CHECK(a.pitch == doctest::Approx(-2.f));
    CHECK(a.volume == doctest::Approx(0.9f));
    CHECK(p.settings.voices.at("b") == VoiceControls{});  // no such Preset: the defaults
    CHECK(p.settings.voices.at("c") == VoiceControls{});  // none active: the defaults
    CHECK(p.settings.voices.at("d").pitch == doctest::Approx(3.f));
    const VoiceControls& e = p.settings.voices.at("e");   // bad values of an old Preset: the defaults
    CHECK(e.speed == 1.f);
    CHECK(e.pitch == 0.f);
    CHECK(e.volume == doctest::Approx(0.5f));
}

TEST_CASE("ParseSettings: the current keys win over an older file's Preset") {
    const Parsed p = Parse(R"({"voices": {"v": {"speed": 1.9, "preset": "P", "presets": {"P": {"speed": 0.6, "pitch": 4}}}}})");
    CHECK(p.notes.empty());
    CHECK(p.settings.voices.at("v").speed == doctest::Approx(1.9f));
    CHECK(p.settings.voices.at("v").pitch == doctest::Approx(4.f));
}

TEST_CASE("ParseSettings: a bad control value gets a note and its default, the others stay") {
    const Parsed p = Parse(R"({"voices": {"v": {"speed": 9, "pitch": "high", "volume": 0.5, "expressiveness": 99}}})");
    REQUIRE(p.settings.voices.count("v") == 1);
    const VoiceControls& c = p.settings.voices.at("v");
    CHECK(c.speed == 1.f);
    CHECK(c.pitch == 0.f);
    CHECK(c.volume == doctest::Approx(0.5f));
    CHECK(Mentions(p.notes, "voices.v.speed"));
    CHECK(Mentions(p.notes, "voices.v.pitch"));
    CHECK_FALSE(Mentions(p.notes, "expressiveness"));  // an old key is ignored silently
    CHECK_FALSE(Mentions(p.notes, "voices.v.volume"));
}

TEST_CASE("ParseSettings: wrongly shaped voices data is noted and skipped without spoiling the rest") {
    Parsed p = Parse(R"({"voices": []})");
    CHECK(p.settings.voices.empty());
    CHECK(Mentions(p.notes, "voices"));

    p = Parse(R"({"voices": {"a": 5, "c": {"speed": 1.1}}})");
    CHECK(Mentions(p.notes, "voices.a"));
    CHECK(p.settings.voices.count("a") == 0);
    CHECK(p.settings.voices.at("c").speed == doctest::Approx(1.1f));
}

TEST_CASE("VoiceControlsToJson writes what ParseSettings reads back") {
    std::map<std::string, VoiceControls> voices;
    VoiceControls c;
    c.speed = 1.25f;
    c.pitch = -3.f;
    c.volume = 0.5f;
    voices["en_US-ljspeech-medium"] = c;
    voices["bg_BG-dimitar-medium"];  // a Voice with the defaults

    const std::string text = "{\"voices\": " + VoiceControlsToJson(voices) + "}";
    const Parsed p = Parse(text);
    CHECK(p.notes.empty());
    CHECK(p.settings.voices == voices);
    CHECK(text.find("preset") == std::string::npos);  // only the current controls are stored
}

// ------------------------------------------------------------------ Control bar (D4) ---

TEST_CASE("ParseSettings: the Control bar is Large at the default place, the theme System") {
    const Parsed p = Parse("{}");
    CHECK(p.notes.empty());
    CHECK(p.settings.controlBar.size == BarSize::Large);
    CHECK(p.settings.controlBar.x == -1);
    CHECK(p.settings.controlBar.y == -1);
    CHECK(p.settings.theme == "system");
}

TEST_CASE("ParseSettings: controlBar size and position are read") {
    const Parsed p = Parse(R"({"controlBar": {"size": "small", "x": 120, "y": 40}, "theme": "dark"})");
    CHECK(p.notes.empty());
    CHECK(p.settings.controlBar.size == BarSize::Small);
    CHECK(p.settings.controlBar.x == 120);
    CHECK(p.settings.controlBar.y == 40);
    CHECK(p.settings.theme == "dark");
}

TEST_CASE("ParseSettings: a bad controlBar value -> note, default for that value only") {
    const Parsed p = Parse(R"({"controlBar": {"size": "huge", "x": "left", "y": 55}, "theme": "pink"})");
    CHECK(Mentions(p.notes, "controlBar.size"));
    CHECK(Mentions(p.notes, "controlBar.x"));
    CHECK(Mentions(p.notes, "theme"));
    CHECK(p.settings.controlBar.size == BarSize::Large);
    CHECK(p.settings.controlBar.x == -1);
    CHECK(p.settings.controlBar.y == 55);
    CHECK(p.settings.theme == "system");
}

TEST_CASE("ParseSettings: a controlBar position below -1 is a bad value") {
    const Parsed p = Parse(R"({"controlBar": {"x": -7}})");
    CHECK(Mentions(p.notes, "controlBar.x"));
    CHECK(p.settings.controlBar.x == -1);
}

TEST_CASE("SettingsToText: the Control bar and the theme survive a round trip") {
    Settings s;
    s.controlBar.size = BarSize::Small;
    s.controlBar.x = 300;
    s.controlBar.y = 12;
    s.theme = "light";
    const Parsed p = Parse(SettingsToText(s));
    CHECK(p.notes.empty());
    CHECK(p.settings == s);
}

TEST_CASE("UpdateSettingsText: controlBar.x is added to a file that has no controlBar section") {
    std::string text = "{\n  \"enabled\": true\n}\n";
    std::string note;
    REQUIRE(UpdateSettingsText(text, "controlBar.x", "88", note));
    const Parsed p = Parse(text);
    CHECK(p.settings.controlBar.x == 88);
    CHECK(p.settings.enabled);
}

TEST_CASE("ParseSettings: theme is System by default; system, light and dark are read") {
    CHECK(Parse("{}").settings.theme == "system");
    CHECK(Parse(R"({"theme": "dark"})").settings.theme == "dark");
    CHECK(Parse(R"({"theme": "light"})").settings.theme == "light");
    CHECK(Parse(R"({"theme": "system"})").settings.theme == "system");
}

TEST_CASE("ParseSettings: a theme that is not system, light or dark -> note, stays System") {
    for (const char* bad : {R"({"theme": "blue"})", R"({"theme": 3})", R"({"theme": "Dark"})"}) {
        const Parsed p = Parse(bad);
        CHECK(Mentions(p.notes, "theme"));
        CHECK(p.settings.theme == "system");
    }
}

TEST_CASE("ParseSettings: runAtStartup, runAsAdmin and leftHanded are off by default and booleans set them") {
    const Settings d = Parse("{}").settings;
    CHECK_FALSE(d.runAtStartup);
    CHECK_FALSE(d.runAsAdmin);
    CHECK_FALSE(d.leftHanded);
    const Settings s = Parse(R"({"runAtStartup": true, "runAsAdmin": true, "leftHanded": true})").settings;
    CHECK(s.runAtStartup);
    CHECK(s.runAsAdmin);
    CHECK(s.leftHanded);
}

TEST_CASE("ParseSettings: a non-boolean runAtStartup, runAsAdmin or leftHanded -> note, stays off") {
    const Parsed p = Parse(R"({"runAtStartup": "yes", "runAsAdmin": 1, "leftHanded": "left"})");
    CHECK(Mentions(p.notes, "runAtStartup"));
    CHECK(Mentions(p.notes, "runAsAdmin"));
    CHECK(Mentions(p.notes, "leftHanded"));
    CHECK_FALSE(p.settings.runAtStartup);
    CHECK_FALSE(p.settings.runAsAdmin);
    CHECK_FALSE(p.settings.leftHanded);
}

TEST_CASE("ParseSettings: startMinimised is on by default, a boolean sets it, another value is a note (D9)") {
    CHECK(Parse("{}").settings.startMinimised);
    CHECK_FALSE(Parse(R"({"startMinimised": false})").settings.startMinimised);
    const Parsed p = Parse(R"({"startMinimised": "no"})");
    CHECK(Mentions(p.notes, "startMinimised"));
    CHECK(p.settings.startMinimised);
}

TEST_CASE("SettingsToText round-trips theme, runAtStartup, startMinimised, runAsAdmin and leftHanded") {
    Settings s;
    s.theme = "dark";
    s.runAtStartup = true;
    s.startMinimised = false;
    s.runAsAdmin = true;
    s.leftHanded = true;
    const Settings back = Parse(SettingsToText(s)).settings;
    CHECK(back.theme == "dark");
    CHECK(back.runAtStartup);
    CHECK_FALSE(back.startMinimised);
    CHECK(back.runAsAdmin);
    CHECK(back.leftHanded);
    CHECK(back == s);
}

TEST_CASE("UpdateSettingsText sets theme and keeps the rest") {
    std::string text = SettingsToText(Settings{});
    std::string note;
    REQUIRE(UpdateSettingsText(text, "theme", "\"light\"", note));
    CHECK(Parse(text).settings.theme == "light");
}

// ------------------------------------------------------- activeVoices (D5) ---

TEST_CASE("ParseSettings: no activeVoices means the shipped Voice speaks (empty map)") {
    const Parsed p = Parse("{}");
    CHECK(p.settings.activeVoices.empty());
    CHECK(p.notes.empty());
}

TEST_CASE("ParseSettings: activeVoices maps a Language id to a Voice name") {
    const Parsed p = Parse(R"({"activeVoices": {"eng": "en_GB-alba-medium"}})");
    REQUIRE(p.settings.activeVoices.count("eng") == 1);
    CHECK(p.settings.activeVoices.at("eng") == "en_GB-alba-medium");
    CHECK(p.notes.empty());
}

TEST_CASE("ParseSettings: a bad activeVoices entry is dropped with a note, the good one stays") {
    const Parsed p = Parse(R"({"activeVoices": {"eng": "en_GB-alba-medium", "xyz": "v", "bul": 5, "bul2": ""}})");
    CHECK(p.settings.activeVoices.size() == 1);
    CHECK(Mentions(p.notes, "activeVoices.xyz"));
    CHECK(Mentions(p.notes, "activeVoices.bul"));
}

TEST_CASE("ParseSettings: activeVoices that is not an object -> note, no entries") {
    const Parsed p = Parse(R"({"activeVoices": ["a"]})");
    CHECK(p.settings.activeVoices.empty());
    CHECK(Mentions(p.notes, "activeVoices"));
}

TEST_CASE("SettingsToText round-trips activeVoices, and it takes part in equality") {
    Settings s;
    s.activeVoices["eng"] = "en_US-amy-medium";
    CHECK(Parse(SettingsToText(s)).settings.activeVoices == s.activeVoices);
    CHECK(s != Settings{});
}

TEST_CASE("ActiveVoicesToJson writes an object keyed by Language id") {
    const std::map<std::string, std::string> m{{"eng", "en_US-amy-medium"}};
    std::string text = SettingsToText(Settings{});
    std::string note;
    REQUIRE(UpdateSettingsText(text, "activeVoices", ActiveVoicesToJson(m), note));
    CHECK(Parse(text).settings.activeVoices == m);
}

namespace {

// Points %LOCALAPPDATA% at a new temporary folder for one test, and puts the old value back after it.
// SaveSettingValue / RunAsAdminSetting find settings.json through it (LexiaDataDir), so the tests never
// touch the real one.
struct TempLocalAppData {
    std::filesystem::path dir;
    std::string old;
    bool hadOld = false;
    TempLocalAppData() {
        dir = std::filesystem::temp_directory_path() /
              ("lexia_settings_test_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)));
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir);
        char* v = nullptr;
        size_t n = 0;
        if (_dupenv_s(&v, &n, "LOCALAPPDATA") == 0 && v) {
            old = v;
            hadOld = true;
            free(v);
        }
        _putenv_s("LOCALAPPDATA", dir.string().c_str());
    }
    ~TempLocalAppData() {
        _putenv_s("LOCALAPPDATA", hadOld ? old.c_str() : "");
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    }
    std::filesystem::path File() const { return dir / "Lexia" / "settings.json"; }
};

std::string Slurp(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

}  // namespace

TEST_CASE("--set-run-as-admin on a PC with no settings file: the file is made, runAsAdmin is on") {
    // With no file in the data folder, ChooseSettingsFile takes a settings.json next to the exe (the portable
    // layout). A developer can have one beside build\Release\lexia_tests.exe; the save would then go into it,
    // outside the temporary folder. So the case is checked only where the exe folder has none.
    if (std::filesystem::exists(ExeDir() / "settings.json")) {
        MESSAGE("Skipped: a settings.json is next to the test exe, and this case must not write into it.");
        return;
    }
    TempLocalAppData tmp;
    REQUIRE_FALSE(std::filesystem::exists(tmp.File()));
    CHECK_FALSE(RunAsAdminSetting());
    REQUIRE(SaveSettingValue("runAsAdmin", "true"));
    REQUIRE(std::filesystem::exists(tmp.File()));
    CHECK(RunAsAdminSetting());
    // Every other value is the default, so the new file works as the first start would have made it.
    std::vector<std::string> notes;
    Settings s = ParseSettings(Slurp(tmp.File()), notes);
    CHECK(notes.empty());
    s.runAsAdmin = false;
    CHECK(s == Settings{});
}

TEST_CASE("--set-run-as-admin on an existing settings file keeps its other keys, on and off") {
    TempLocalAppData tmp;
    std::filesystem::create_directories(tmp.File().parent_path());
    {
        std::ofstream out(tmp.File(), std::ios::binary);
        out << "{\n  \"enabled\": true,\n  \"language\": \"eng\",\n  \"hotkeys\": { \"read\": \"F9\" },\n  \"runAsAdmin\": true\n}\n";
    }
    CHECK(RunAsAdminSetting());
    REQUIRE(SaveSettingValue("runAsAdmin", "false"));
    CHECK_FALSE(RunAsAdminSetting());
    std::vector<std::string> notes;
    Settings s = ParseSettings(Slurp(tmp.File()), notes);
    CHECK(s.enabled);
    CHECK(s.language == "eng");
    CHECK(s.read.text == "F9");
    REQUIRE(SaveSettingValue("runAsAdmin", "true"));
    CHECK(RunAsAdminSetting());
    CHECK(ParseSettings(Slurp(tmp.File()), notes).read.text == "F9");
}

TEST_CASE("--set-run-as-admin leaves a settings file that is not JSON alone and reports failure") {
    TempLocalAppData tmp;
    std::filesystem::create_directories(tmp.File().parent_path());
    {
        std::ofstream out(tmp.File(), std::ios::binary);
        out << "not json at all";
    }
    CHECK_FALSE(SaveSettingValue("runAsAdmin", "true"));
    CHECK(Slurp(tmp.File()) == "not json at all");
}