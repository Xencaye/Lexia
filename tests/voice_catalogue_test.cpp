// Unit tests for tts/voice_catalogue.h: parsing and filtering the piper-voices catalogue.
#include <doctest/doctest.h>

#include <string>

#include "tts/voice_catalogue.h"

namespace {

// One catalogue entry, shaped like the real voices.json of rhasspy/piper-voices.
std::string Entry(const std::string& key, const std::string& family, const std::string& code,
                  const std::string& english, const std::string& quality, const std::string& dir = "") {
    const std::string base = (dir.empty() ? family + "/" + code + "/x/" + quality : dir) + "/" + key;
    return "\"" + key + "\": {\"key\": \"" + key + "\", \"name\": \"x\", \"language\": {\"code\": \"" + code +
           "\", \"family\": \"" + family + "\", \"name_english\": \"" + english + "\"}, \"quality\": \"" + quality +
           "\", \"num_speakers\": 1, \"files\": {\"" + base + ".onnx\": {\"size_bytes\": 1000, \"md5_digest\": "
           "\"FC1CE62A4F04F089E22B8C3A13BDE28A\"}, \"" + base + ".onnx.json\": {\"size_bytes\": 50, "
           "\"md5_digest\": \"5fb4aa5e5b20d4ed515d40f0b4ce16f0\"}, \"" + base + "/MODEL_CARD\": "
           "{\"size_bytes\": 5, \"md5_digest\": \"6056805758b0136214309e799209d9c9\"}}, \"aliases\": []}";
}

}  // namespace

TEST_CASE("ParseVoiceCatalogue: reads the name, language, quality and the two files with size and MD5") {
    const std::string json = "{" + Entry("bg_BG-dimitar-medium", "bg", "bg_BG", "Bulgarian", "medium") + "}";
    const auto voices = ParseVoiceCatalogue(json);
    REQUIRE(voices.size() == 1);
    const CatalogueVoice& v = voices[0];
    CHECK(v.name == "bg_BG-dimitar-medium");
    CHECK(v.languageCode == "bg_BG");
    CHECK(v.languageName == "Bulgarian");
    CHECK(v.quality == "medium");
    CHECK(v.model.path == "bg/bg_BG/x/medium/bg_BG-dimitar-medium.onnx");
    CHECK(v.model.sizeBytes == 1000);
    CHECK(v.model.md5 == "fc1ce62a4f04f089e22b8c3a13bde28a");  // lower-cased
    CHECK(v.config.path == "bg/bg_BG/x/medium/bg_BG-dimitar-medium.onnx.json");
    CHECK(v.config.sizeBytes == 50);
}

TEST_CASE("ParseVoiceCatalogue: text that is not a catalogue gives no Voices and an error line") {
    std::string error;
    CHECK(ParseVoiceCatalogue("not json", &error).empty());
    CHECK_FALSE(error.empty());
    error.clear();
    CHECK(ParseVoiceCatalogue("[1, 2]", &error).empty());
    CHECK_FALSE(error.empty());
    CHECK(ParseVoiceCatalogue("{}", nullptr).empty());  // an empty catalogue is fine, and no error pointer is fine
}

TEST_CASE("ParseVoiceCatalogue: an entry without both files, a size, an MD5 or a plain path is left out") {
    const std::string noConfig =
        "\"a-b-medium\": {\"language\": {\"code\": \"en_US\"}, \"quality\": \"medium\", \"files\": "
        "{\"a/a-b-medium.onnx\": {\"size_bytes\": 5, \"md5_digest\": \"fc1ce62a4f04f089e22b8c3a13bde28a\"}}}";
    const std::string noMd5 =
        "\"c-d-medium\": {\"language\": {\"code\": \"en_US\"}, \"quality\": \"medium\", \"files\": "
        "{\"c/c-d-medium.onnx\": {\"size_bytes\": 5}, \"c/c-d-medium.onnx.json\": {\"size_bytes\": 5, "
        "\"md5_digest\": \"fc1ce62a4f04f089e22b8c3a13bde28a\"}}}";
    const std::string climbing = Entry("e-f-medium", "en", "en_US", "English", "medium", "../../evil");
    const std::string spaced = Entry("g h-medium", "en", "en_US", "English", "medium", "en");
    const std::string good = Entry("i-j-medium", "en", "en_US", "English", "medium");
    const auto voices = ParseVoiceCatalogue("{" + noConfig + "," + noMd5 + "," + climbing + "," + spaced + "," + good + "}");
    REQUIRE(voices.size() == 1);
    CHECK(voices[0].name == "i-j-medium");
}

TEST_CASE("FilterCatalogueForLexia: Lexia's Languages in Medium quality, sorted by name") {
    const std::string json = "{" + Entry("en_US-zed-medium", "en", "en_US", "English", "medium") + "," +
                             Entry("en_US-low-low", "en", "en_US", "English", "low") + "," +
                             Entry("de_DE-thorsten-medium", "de", "de_DE", "German", "medium") + "," +
                             Entry("bg_BG-dimitar-medium", "bg", "bg_BG", "Bulgarian", "medium") + "," +
                             Entry("en_GB-alan-medium", "en", "en_GB", "English", "medium") + "," +
                             Entry("en_US-big-high", "en", "en_US", "English", "x_high") + "}";
    const auto kept = FilterCatalogueForLexia(ParseVoiceCatalogue(json));
    REQUIRE(kept.size() == 3);
    CHECK(kept[0].name == "bg_BG-dimitar-medium");
    CHECK(kept[1].name == "en_GB-alan-medium");
    CHECK(kept[2].name == "en_US-zed-medium");
}

TEST_CASE("CheckCatalogueVoice: names the language Lexia cannot read, or the quality that is not Medium") {
    const auto de = ParseVoiceCatalogue("{" + Entry("de_DE-thorsten-medium", "de", "de_DE", "German", "medium") + "}");
    REQUIRE(de.size() == 1);
    const VoiceOutcome refused = CheckCatalogueVoice(de[0]);
    CHECK(refused.problem == VoiceProblem::UnsupportedLanguage);
    CHECK(refused.languageName == "German");

    const auto low = ParseVoiceCatalogue("{" + Entry("en_US-low-low", "en", "en_US", "English", "low") + "}");
    REQUIRE(low.size() == 1);
    const VoiceOutcome notMedium = CheckCatalogueVoice(low[0]);
    CHECK(notMedium.problem == VoiceProblem::NotMedium);
    CHECK(notMedium.quality == "low");
}
