// The typed Voice library events (K.3): the text id of every problem in each place it is shown, the facts that go
// into a text, the filter that keeps only the Voices of offered Languages.
#include "strings/string_ids.h"
#include "strings/strings.h"
#include "ui_host.h"
#include "voice_events.h"

#include <doctest/doctest.h>

#include <set>
#include <string>
#include <vector>

namespace {

using namespace strings::id;

ProblemFacts Facts(VoiceProblem p, const std::string& language = "", const std::string& quality = "") {
    return ProblemFacts{p, language, quality};
}

CatalogueVoice Voice(const std::string& name, const std::string& languageCode, uint64_t model, uint64_t config) {
    CatalogueVoice v;
    v.name = name;
    v.languageCode = languageCode;
    v.model.sizeBytes = model;
    v.config.sizeBytes = config;
    return v;
}

}  // namespace

TEST_CASE("ProblemTextId: every problem has its text, and three read differently by place") {
    // The same rule as the old page's problemText().
    CHECK(std::string(ProblemTextId(Facts(VoiceProblem::UnsupportedLanguage), ProblemWhere::File)) == kWindowProblemUnsupportedLanguage);
    CHECK(std::string(ProblemTextId(Facts(VoiceProblem::UnsupportedLanguage), ProblemWhere::Download)) == kWindowProblemUnsupportedLanguage);
    CHECK(std::string(ProblemTextId(Facts(VoiceProblem::MissingFile), ProblemWhere::File)) == kWindowProblemMissingFile);
    CHECK(std::string(ProblemTextId(Facts(VoiceProblem::BadConfig), ProblemWhere::File)) == kWindowProblemBadConfig);
    CHECK(std::string(ProblemTextId(Facts(VoiceProblem::BadZip), ProblemWhere::File)) == kWindowProblemBadZip);
    CHECK(std::string(ProblemTextId(Facts(VoiceProblem::CannotWrite), ProblemWhere::Download)) == kWindowProblemCannotWrite);
    CHECK(std::string(ProblemTextId(Facts(VoiceProblem::NotInCatalogue), ProblemWhere::Download)) == kWindowProblemNotInCatalogue);
    CHECK(std::string(ProblemTextId(Facts(VoiceProblem::BadDownload), ProblemWhere::Download)) == kWindowProblemBadDownload);

    // Network: the list, or one download.
    CHECK(std::string(ProblemTextId(Facts(VoiceProblem::Network), ProblemWhere::Catalogue)) == kWindowProblemCatalogueNetwork);
    CHECK(std::string(ProblemTextId(Facts(VoiceProblem::Network), ProblemWhere::Download)) == kWindowProblemNetwork);
    // BadName: a file the person chose, or a Voice of the list.
    CHECK(std::string(ProblemTextId(Facts(VoiceProblem::BadName), ProblemWhere::File)) == kWindowProblemBadName);
    CHECK(std::string(ProblemTextId(Facts(VoiceProblem::BadName), ProblemWhere::Download)) == kWindowProblemBadNameDownload);
    // NotMedium: with a quality, or without.
    CHECK(std::string(ProblemTextId(Facts(VoiceProblem::NotMedium, "", "low"), ProblemWhere::File)) == kWindowProblemNotMedium);
    CHECK(std::string(ProblemTextId(Facts(VoiceProblem::NotMedium), ProblemWhere::File)) == kWindowProblemNotMediumUnknown);
    // No text of their own: the general one.
    CHECK(std::string(ProblemTextId(Facts(VoiceProblem::Busy), ProblemWhere::Download)) == kWindowProblemUnknown);
    CHECK(std::string(ProblemTextId(Facts(VoiceProblem::Cancelled), ProblemWhere::Download)) == kWindowProblemUnknown);
    CHECK(std::string(ProblemTextId(Facts(VoiceProblem::None), ProblemWhere::File)) == kWindowProblemUnknown);
}

TEST_CASE("ProblemText: every problem reads in every place and the facts are filled in") {
    const VoiceProblem all[] = {VoiceProblem::None,       VoiceProblem::UnsupportedLanguage, VoiceProblem::NotMedium,
                                VoiceProblem::BadName,    VoiceProblem::MissingFile,         VoiceProblem::BadConfig,
                                VoiceProblem::BadZip,     VoiceProblem::CannotWrite,         VoiceProblem::NotInCatalogue,
                                VoiceProblem::Network,    VoiceProblem::BadDownload,         VoiceProblem::Cancelled,
                                VoiceProblem::Busy};
    for (VoiceProblem p : all) {
        for (ProblemWhere where : {ProblemWhere::Catalogue, ProblemWhere::Download, ProblemWhere::File}) {
            const std::string text = ProblemText("en", Facts(p, "German", "low"), where);
            CHECK_FALSE(text.empty());
            CHECK(text.find('{') == std::string::npos);  // no placeholder is left
        }
    }
    CHECK(ProblemText("en", Facts(VoiceProblem::UnsupportedLanguage, "German"), ProblemWhere::File) ==
          "Lexia can't read German yet. Choose a voice for a language that Lexia reads.");
    // No name came: "this language", never a hole in the sentence.
    CHECK(ProblemText("en", Facts(VoiceProblem::UnsupportedLanguage), ProblemWhere::File) ==
          "Lexia can't read this language yet. Choose a voice for a language that Lexia reads.");
    CHECK(ProblemText("en", Facts(VoiceProblem::NotMedium, "", "low"), ProblemWhere::File) ==
          "This voice has low quality. Lexia uses only voices of Medium quality. Choose a Medium voice.");
}

TEST_CASE("OfferedCatalogue keeps only the Voices of offered Languages and marks the installed ones") {
    // English is offered; Bulgarian is in the code but not offered (ADR-0007); German is not a Language of Lexia.
    const std::vector<CatalogueVoice> catalogue = {Voice("en_US-amy-medium", "en_US", 60, 3),
                                                   Voice("bg_BG-sample-medium", "bg_BG", 50, 2),
                                                   Voice("de_DE-thorsten-medium", "de_DE", 40, 1),
                                                   Voice("en_GB-alba-medium", "en_GB", 70, 5)};
    const std::vector<CatalogueEntry> shown = OfferedCatalogue(catalogue, {"en_GB-alba-medium"});
    REQUIRE(shown.size() == 2);
    CHECK(shown[0].name == "en_US-amy-medium");
    CHECK(shown[0].language == "eng");
    CHECK(shown[0].sizeBytes == 63);  // model + config
    CHECK_FALSE(shown[0].installed);
    CHECK(shown[1].name == "en_GB-alba-medium");
    CHECK(shown[1].installed);
}

TEST_CASE("the typed facts of a refusal and of an added Voice") {
    VoiceOutcome refused;
    refused.problem = VoiceProblem::UnsupportedLanguage;
    refused.languageName = "German";
    refused.quality = "low";
    const ProblemFacts f = FactsOf(refused);
    CHECK(f.problem == VoiceProblem::UnsupportedLanguage);
    CHECK(f.language == "German");
    CHECK(f.quality == "low");

    VoiceOutcome added;
    added.voiceName = "en_US-amy-medium";
    added.languageCode = "en";
    const VoiceAddedFacts a = AddedFactsOf(added);
    CHECK(a.voice == "en_US-amy-medium");
    CHECK(a.language == "eng");

    // A Language that is not offered gives no Language id.
    added.languageCode = "bg";
    CHECK(AddedFactsOf(added).language.empty());

    const std::vector<CatalogueVoice> catalogue = {Voice("en_US-amy-medium", "en_US", 60, 3),
                                                   Voice("de_DE-thorsten-medium", "de_DE", 40, 1)};
    CHECK(OfferedCatalogue(catalogue, {}).size() == 1);  // the German Voice is of a Language Lexia does not offer
}
