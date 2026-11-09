// Unit tests for cli.h: the pure parts of lexia_cli (arguments, expected.txt text, JSON).
#include <doctest/doctest.h>

#include <nlohmann/json.hpp>
#include <string>
#include <utility>
#include <vector>

#include "cli.h"
#include "version.h"

namespace {

TextLine MakeLine(int left, int top, int right, int bottom, const std::string& text, float conf) {
    TextLine l;
    l.box = {left, top, right, bottom};
    l.text = text;
    l.conf = conf;
    return l;
}

// A Block whose box is the union of its Lines' boxes, like GroupBlocks makes it.
TextBlock MakeBlock(const std::string& text, std::vector<TextLine> lines) {
    TextBlock b;
    b.text = text;
    b.lines = std::move(lines);
    b.box = b.lines.front().box;
    for (const TextLine& l : b.lines) {
        if (l.box.left < b.box.left) b.box.left = l.box.left;
        if (l.box.top < b.box.top) b.box.top = l.box.top;
        if (l.box.right > b.box.right) b.box.right = l.box.right;
        if (l.box.bottom > b.box.bottom) b.box.bottom = l.box.bottom;
    }
    return b;
}

}  // namespace

// ----------------------------------------------------------------- arguments ---

TEST_CASE("ParseCliArgs: just an image path") {
    const CliParse p = ParseCliArgs({"shot.png"});
    REQUIRE(p.ok);
    CHECK(p.args.imagePath == "shot.png");
    CHECK(p.args.langCode.empty());  // empty = the default Language
    CHECK_FALSE(p.args.hasRect);
    CHECK(p.args.jsonPath.empty());
    CHECK(p.args.drawPath.empty());
    CHECK(p.args.scorePath.empty());
}

TEST_CASE("ParseCliArgs: every option, in any order") {
    const CliParse p = ParseCliArgs({"--draw", "out.png", "--lang", "bg", "shot.png", "--json",
                                     "out.json", "--rect", "10,20,300,40", "--score",
                                     "expected.txt"});
    REQUIRE(p.ok);
    CHECK(p.args.imagePath == "shot.png");
    CHECK(p.args.langCode == "bg");
    CHECK(p.args.jsonPath == "out.json");
    CHECK(p.args.drawPath == "out.png");
    CHECK(p.args.scorePath == "expected.txt");
    REQUIRE(p.args.hasRect);
    CHECK(p.args.rect.left == 10);
    CHECK(p.args.rect.top == 20);
    CHECK(p.args.rect.right == 310);  // x + w
    CHECK(p.args.rect.bottom == 60);  // y + h
}

TEST_CASE("ParseCliArgs: --say with --wav needs no image") {
    const CliParse p = ParseCliArgs({"--say", "Hello there.", "--lang", "bg", "--wav", "out.wav"});
    REQUIRE(p.ok);
    CHECK(p.args.sayText == "Hello there.");
    CHECK(p.args.wavPath == "out.wav");
    CHECK(p.args.langCode == "bg");
    CHECK(p.args.imagePath.empty());
}

TEST_CASE("ParseCliArgs: --say without --wav is an error that names --wav") {
    const CliParse p = ParseCliArgs({"--say", "Hello"});
    REQUIRE_FALSE(p.ok);
    CHECK(p.error.find("--wav") != std::string::npos);
}

TEST_CASE("ParseCliArgs: --wav without --say is an error that names --say") {
    const CliParse p = ParseCliArgs({"shot.png", "--wav", "out.wav"});
    REQUIRE_FALSE(p.ok);
    CHECK(p.error.find("--say") != std::string::npos);
}

TEST_CASE("ParseCliArgs: --say cannot be combined with an image or its options") {
    CHECK_FALSE(ParseCliArgs({"shot.png", "--say", "Hi", "--wav", "o.wav"}).ok);
    CHECK_FALSE(ParseCliArgs({"--say", "Hi", "--wav", "o.wav", "--json", "o.json"}).ok);
    CHECK_FALSE(ParseCliArgs({"--say", "Hi", "--wav", "o.wav", "--rect", "0,0,5,5"}).ok);
}

TEST_CASE("ParseCliArgs: --say with only spaces is an error") {
    CHECK_FALSE(ParseCliArgs({"--say", "  ", "--wav", "o.wav"}).ok);
}

TEST_CASE("ParseCliArgs: --help is not an error") {
    const CliParse p = ParseCliArgs({"--help"});
    CHECK(p.ok);
    CHECK(p.help);
}

TEST_CASE("ParseCliArgs: --version is not an error and needs no image") {
    const CliParse p = ParseCliArgs({"--version"});
    CHECK(p.ok);
    CHECK(p.version);
    CHECK_FALSE(p.help);
}

TEST_CASE("CliVersionText: the program name and Lexia's version, one line") {
    CHECK(CliVersionText() == std::string("lexia_cli ") + LEXIA_VERSION + "\n");
}

TEST_CASE("LEXIA_VERSION: major.minor.patch, matching the numeric parts") {
    const std::string expected = std::to_string(LEXIA_VERSION_MAJOR) + "." +
                                 std::to_string(LEXIA_VERSION_MINOR) + "." +
                                 std::to_string(LEXIA_VERSION_PATCH);
    CHECK(std::string(LEXIA_VERSION) == expected);
}

TEST_CASE("ParseCliArgs: no image is an error") {
    CHECK_FALSE(ParseCliArgs({}).ok);
    const CliParse p = ParseCliArgs({"--lang", "en"});
    CHECK_FALSE(p.ok);
    CHECK_FALSE(p.error.empty());
}

TEST_CASE("ParseCliArgs: a second image is an error") {
    CHECK_FALSE(ParseCliArgs({"a.png", "b.png"}).ok);
}

TEST_CASE("ParseCliArgs: an unknown option is an error that names it") {
    const CliParse p = ParseCliArgs({"a.png", "--bogus"});
    REQUIRE_FALSE(p.ok);
    CHECK(p.error.find("--bogus") != std::string::npos);
}

TEST_CASE("ParseCliArgs: an option without its value is an error") {
    for (const char* opt : {"--lang", "--rect", "--json", "--draw", "--score", "--say", "--wav"}) {
        CAPTURE(opt);
        const CliParse p = ParseCliArgs({"a.png", opt});
        REQUIRE_FALSE(p.ok);
        CHECK(p.error.find(opt) != std::string::npos);
    }
}

TEST_CASE("ParseCliArgs: --lang takes the two-letter Language codes, nothing else") {
    CHECK(ParseCliArgs({"a.png", "--lang", "en"}).ok);
    CHECK(ParseCliArgs({"a.png", "--lang", "bg"}).ok);
    CHECK_FALSE(ParseCliArgs({"a.png", "--lang", "eng"}).ok);  // the settings.json ids are not CLI codes
    CHECK_FALSE(ParseCliArgs({"a.png", "--lang", "bul"}).ok);
    const CliParse p = ParseCliArgs({"a.png", "--lang", "xx"});
    REQUIRE_FALSE(p.ok);
    CHECK(p.error.find("xx") != std::string::npos);
    CHECK(p.error.find("en|bg") != std::string::npos);  // says what is accepted
}

TEST_CASE("ParseCliArgs: --rect must be x,y,w,h with whole numbers, w and h above 0") {
    for (const char* bad : {"1,2,3", "1,2,3,4,5", "a,b,c,d", "1,2,3,", "1.5,2,3,4", "1,2,0,4",
                            "1,2,3,0", "-1,2,3,4", "1,-2,3,4", "", "1 2 3 4"}) {
        CAPTURE(bad);
        CHECK_FALSE(ParseCliArgs({"a.png", "--rect", bad}).ok);
    }
    CHECK(ParseCliArgs({"a.png", "--rect", "0,0,1,1"}).ok);
}

TEST_CASE("ParseCliArgs: --rect whose right or bottom edge would pass INT_MAX is an error") {
    // 2147483647 = INT_MAX: x + w and y + h must still fit in an int.
    for (const char* bad : {"1,0,2147483647,1", "0,1,1,2147483647", "2147483647,0,1,1", "1000,1000,2147483000,5"}) {
        CAPTURE(bad);
        CHECK_FALSE(ParseCliArgs({"a.png", "--rect", bad}).ok);
    }
    const CliParse edge = ParseCliArgs({"a.png", "--rect", "0,1,2147483647,2147483646"});
    REQUIRE(edge.ok);
    CHECK(edge.args.rect.right == 2147483647);
    CHECK(edge.args.rect.bottom == 2147483647);
}

TEST_CASE("CliUsage: shows the command line and the Language codes") {
    const std::string u = CliUsage();
    CHECK(u.find("lexia_cli") != std::string::npos);
    CHECK(u.find("--rect") != std::string::npos);
    CHECK(u.find("--json") != std::string::npos);
    CHECK(u.find("--draw") != std::string::npos);
    CHECK(u.find("--score") != std::string::npos);
    CHECK(u.find("--lang en|bg") != std::string::npos);
    CHECK(u.find("default en)") != std::string::npos);
}

// ------------------------------------------------------- expected.txt format ---

TEST_CASE("BlocksToExpectedText: no Blocks gives an empty string") {
    CHECK(BlocksToExpectedText({}).empty());
}

TEST_CASE("BlocksToExpectedText: one paragraph per Block, a blank line between, one final newline") {
    const std::vector<TextBlock> blocks = {
        MakeBlock("Heading", {MakeLine(0, 0, 100, 20, "Heading", 90.f)}),
        MakeBlock("First paragraph.", {MakeLine(0, 40, 100, 60, "First paragraph.", 90.f)}),
        MakeBlock("Caption", {MakeLine(0, 80, 100, 100, "Caption", 90.f)}),
    };
    CHECK(BlocksToExpectedText(blocks) == "Heading\n\nFirst paragraph.\n\nCaption\n");
}

TEST_CASE("BlocksToExpectedText: UTF-8 text goes through byte for byte") {
    const std::string bg = "\xD0\x97\xD0\xB4\xD1\x80\xD0\xB0\xD0\xB2\xD0\xB5\xD0\xB9";  // Cyrillic
    const std::vector<TextBlock> blocks = {MakeBlock(bg, {MakeLine(0, 0, 100, 20, bg, 90.f)})};
    CHECK(BlocksToExpectedText(blocks) == bg + "\n");
}

// --------------------------------------------------------------- score JSON ---

TEST_CASE("ScoreToJson: the numbers a script reads, as one JSON object") {
    LayoutScore s;
    s.distance = 0.125;
    s.editDistance = 5;
    s.expectedBlocks = 4;
    s.producedBlocks = 6;
    s.blockCountError = 2;
    const nlohmann::json j = nlohmann::json::parse(ScoreToJson(s));
    CHECK(j.at("distance").get<double>() == doctest::Approx(0.125));
    CHECK(j.at("editDistance").get<int>() == 5);
    CHECK(j.at("expectedBlocks").get<int>() == 4);
    CHECK(j.at("producedBlocks").get<int>() == 6);
    CHECK(j.at("blockCountError").get<int>() == 2);
}

// ---------------------------------------------------------------------- JSON ---

TEST_CASE("BlocksToJson: records the Language, the elapsed ms and the Blocks in order") {
    const std::vector<TextBlock> blocks = {
        MakeBlock("one two", {MakeLine(10, 20, 110, 40, "one", 91.5f),
                              MakeLine(10, 45, 110, 65, "two", 80.f)}),
        MakeBlock("three", {MakeLine(200, 20, 300, 40, "three", 70.f)}),
    };
    const nlohmann::json j = nlohmann::json::parse(BlocksToJson(blocks, "en", 1234));
    CHECK(j["language"] == "en");
    CHECK(j["ms"] == 1234);
    REQUIRE(j["blocks"].is_array());
    REQUIRE(j["blocks"].size() == 2);

    const auto& b0 = j["blocks"][0];
    CHECK(b0["index"] == 1);  // the number drawn by --draw
    CHECK(b0["text"] == "one two");
    CHECK(b0["box"]["left"] == 10);
    CHECK(b0["box"]["top"] == 20);
    CHECK(b0["box"]["right"] == 110);
    CHECK(b0["box"]["bottom"] == 65);
    REQUIRE(b0["lines"].size() == 2);
    CHECK(b0["lines"][0]["text"] == "one");
    CHECK(b0["lines"][0]["conf"].get<double>() == doctest::Approx(91.5));
    CHECK(b0["lines"][0]["box"]["left"] == 10);
    CHECK(b0["lines"][0]["box"]["bottom"] == 40);
    CHECK(b0["lines"][1]["text"] == "two");

    const auto& b1 = j["blocks"][1];
    CHECK(b1["index"] == 2);
    CHECK(b1["text"] == "three");
    REQUIRE(b1["lines"].size() == 1);
}

TEST_CASE("BlocksToJson: no Blocks gives an empty array, not null") {
    const nlohmann::json j = nlohmann::json::parse(BlocksToJson({}, "bg", 0));
    CHECK(j["language"] == "bg");
    REQUIRE(j["blocks"].is_array());
    CHECK(j["blocks"].empty());
}

TEST_CASE("BlocksToJson: UTF-8 text is written as UTF-8 and survives a parse") {
    const std::string bg = "\xD0\x97\xD0\xB4\xD1\x80\xD0\xB0\xD0\xB2\xD0\xB5\xD0\xB9";
    const std::vector<TextBlock> blocks = {MakeBlock(bg, {MakeLine(0, 0, 100, 20, bg, 90.f)})};
    const std::string out = BlocksToJson(blocks, "bg", 1);
    CHECK(out.find(bg) != std::string::npos);  // not \uXXXX escapes
    CHECK(nlohmann::json::parse(out)["blocks"][0]["text"] == bg);
}

TEST_CASE("BlocksToJson: bytes that are not valid UTF-8 do not make it throw") {
    const std::vector<TextBlock> blocks = {
        MakeBlock("bad \xFF byte", {MakeLine(0, 0, 100, 20, "bad \xFF byte", 90.f)})};
    CHECK(nlohmann::json::accept(BlocksToJson(blocks, "en", 1)));  // BlocksToJson did not throw, and its output is valid JSON
}

TEST_CASE("ParseCliArgs: --say takes the Voice controls, inside their ranges") {
    const CliParse p = ParseCliArgs({"--say", "Hi", "--wav", "o.wav", "--speed", "1.5", "--pitch", "-3",
                                     "--volume", "0.5"});
    REQUIRE(p.ok);
    CHECK(p.args.voice.speed == doctest::Approx(1.5f));
    CHECK(p.args.voice.pitch == doctest::Approx(-3.f));
    CHECK(p.args.voice.volume == doctest::Approx(0.5f));

    const CliParse d = ParseCliArgs({"--say", "Hi", "--wav", "o.wav"});
    REQUIRE(d.ok);
    CHECK(d.args.voice == VoiceControls{});
}

TEST_CASE("ParseCliArgs: a Voice control outside its range, or not a number, is an error that names it") {
    for (const char* opt : {"--speed", "--pitch", "--volume"}) {
        for (const char* bad : {"99", "-99", "fast", ""}) {
            CAPTURE(opt);
            CAPTURE(bad);
            const CliParse p = ParseCliArgs({"--say", "Hi", "--wav", "o.wav", opt, bad});
            REQUIRE_FALSE(p.ok);
            CHECK(p.error.find(opt) != std::string::npos);
        }
    }
}

TEST_CASE("ParseCliArgs: Voice controls only make sense with --say") {
    CHECK_FALSE(ParseCliArgs({"a.png", "--speed", "1.5"}).ok);
    CHECK_FALSE(ParseCliArgs({"a.png", "--pitch", "1"}).ok);
}

// -------------------------------------------------------------- Voice library ---

TEST_CASE("ParseCliArgs: --voices-catalogue, --voice-download and --voice-add stand alone") {
    const CliParse list = ParseCliArgs({"--voices-catalogue"});
    REQUIRE(list.ok);
    CHECK(list.args.listVoices);
    CHECK(list.args.imagePath.empty());

    const CliParse dl = ParseCliArgs({"--voice-download", "bg_BG-sample-medium"});
    REQUIRE(dl.ok);
    CHECK(dl.args.downloadVoice == "bg_BG-sample-medium");

    const CliParse add = ParseCliArgs({"--voice-add", "C:/v/my.onnx", "--voices-dir", "D:/voices"});
    REQUIRE(add.ok);
    CHECK(add.args.addVoicePath == "C:/v/my.onnx");
    CHECK(add.args.voicesDir == "D:/voices");
}

TEST_CASE("ParseCliArgs: the Voice library options cannot be mixed with each other or with an image or --say") {
    CHECK_FALSE(ParseCliArgs({"--voices-catalogue", "--voice-add", "x.zip"}).ok);
    CHECK_FALSE(ParseCliArgs({"--voice-download", "a", "--voice-add", "x.zip"}).ok);
    CHECK_FALSE(ParseCliArgs({"--voice-add", "x.zip", "a.png"}).ok);
    CHECK_FALSE(ParseCliArgs({"--voices-catalogue", "--say", "Hi", "--wav", "o.wav"}).ok);
    CHECK_FALSE(ParseCliArgs({"--voice-add"}).ok);
    CHECK_FALSE(ParseCliArgs({"--voice-download", ""}).ok);
    CHECK_FALSE(ParseCliArgs({"--voices-dir", "D:/v"}).ok);  // it needs one of the three
    CHECK_FALSE(ParseCliArgs({"--voices-catalogue", "--voices-dir", "D:/v"}).ok);  // lists, writes nothing
}

TEST_CASE("VoiceCatalogueToText: one line per Voice with its Language and size, marking installed ones") {
    CatalogueVoice a;
    a.name = "bg_BG-sample-medium";
    a.languageName = "Bulgarian";
    a.model.sizeBytes = 63221984;
    a.config.sizeBytes = 5123;
    CatalogueVoice b;
    b.name = "en_US-amy-medium";
    b.languageName = "English";
    b.model.sizeBytes = 63000000;
    b.config.sizeBytes = 1000;
    const std::string text = VoiceCatalogueToText({a, b}, {"en_US-amy-medium"});
    CHECK(text == "bg_BG-sample-medium\tBulgarian\t60.3 MB\n"
                  "en_US-amy-medium\tEnglish\t60.1 MB\tinstalled\n");
    CHECK(VoiceCatalogueToText({}, {}).empty());
}

TEST_CASE("VoiceOutcomeToText: says what was added, or the problem id with the fact the message needs") {
    VoiceOutcome ok;
    ok.voiceName = "bg_BG-sample-medium";
    ok.languageCode = "bg";
    ok.languageName = "Bulgarian";
    CHECK(VoiceOutcomeToText(ok) == "added bg_BG-sample-medium (Bulgarian)\n");

    VoiceOutcome german;
    german.problem = VoiceProblem::UnsupportedLanguage;
    german.languageName = "German";
    german.detail = "Lexia cannot read German yet";
    CHECK(VoiceOutcomeToText(german) == "unsupported-language (German): Lexia cannot read German yet\n");

    VoiceOutcome low;
    low.problem = VoiceProblem::NotMedium;
    low.quality = "low";
    low.detail = "only Medium";
    CHECK(VoiceOutcomeToText(low) == "not-medium (low): only Medium\n");

    VoiceOutcome zip;
    zip.problem = VoiceProblem::BadZip;
    zip.detail = "unsafe path";
    CHECK(VoiceOutcomeToText(zip) == "bad-zip: unsafe path\n");
}

TEST_CASE("ParseCliArgs: --expressiveness is gone (D8.2): an unknown option") {
    CHECK_FALSE(ParseCliArgs({"--say", "Hi", "--wav", "o.wav", "--expressiveness", "1.2"}).ok);
}
