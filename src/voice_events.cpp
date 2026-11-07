#include "voice_events.h"

#include "strings/string_ids.h"
#include "strings/strings.h"

const char* ProblemTextId(const ProblemFacts& facts, ProblemWhere where) {
    using namespace strings::id;
    switch (facts.problem) {
    case VoiceProblem::Network:
        // The list could not come, or one download did not finish: two sentences.
        return where == ProblemWhere::Catalogue ? kWindowProblemCatalogueNetwork : kWindowProblemNetwork;
    case VoiceProblem::BadName:
        return where == ProblemWhere::File ? kWindowProblemBadName : kWindowProblemBadNameDownload;
    case VoiceProblem::NotMedium:
        // An unknown quality has a sentence that does not name one.
        return facts.quality.empty() ? kWindowProblemNotMediumUnknown : kWindowProblemNotMedium;
    case VoiceProblem::UnsupportedLanguage: return kWindowProblemUnsupportedLanguage;
    case VoiceProblem::MissingFile: return kWindowProblemMissingFile;
    case VoiceProblem::BadConfig: return kWindowProblemBadConfig;
    case VoiceProblem::BadZip: return kWindowProblemBadZip;
    case VoiceProblem::CannotWrite: return kWindowProblemCannotWrite;
    case VoiceProblem::NotInCatalogue: return kWindowProblemNotInCatalogue;
    case VoiceProblem::BadDownload: return kWindowProblemBadDownload;
    case VoiceProblem::None:
    case VoiceProblem::Cancelled:
    case VoiceProblem::Busy: break;  // no text of their own
    }
    return kWindowProblemUnknown;
}

ProblemFacts FactsOf(const VoiceOutcome& outcome) {
    ProblemFacts f;
    f.problem = outcome.problem;
    f.language = outcome.languageName;
    f.quality = outcome.quality;
    return f;
}

const Language* OfferedLanguageForCode(const std::string& code) {
    const Language* l = FindLanguageByCode(code.substr(0, code.find_first_of("_-")));
    return l && l->offered ? l : nullptr;
}

std::vector<CatalogueEntry> OfferedCatalogue(const std::vector<CatalogueVoice>& voices,
                                             const std::set<std::string>& installedNames) {
    std::vector<CatalogueEntry> out;
    for (const CatalogueVoice& v : voices) {
        const Language* lang = OfferedLanguageForCode(v.languageCode);
        if (!lang) continue;  // a Voice of a Language Lexia does not offer is not listed (ADR-0007)
        CatalogueEntry e;
        e.name = v.name;
        e.language = lang->id;
        e.sizeBytes = v.model.sizeBytes + v.config.sizeBytes;
        e.installed = installedNames.count(v.name) > 0;
        out.push_back(std::move(e));
    }
    return out;
}

VoiceAddedFacts AddedFactsOf(const VoiceOutcome& outcome) {
    const Language* lang = OfferedLanguageForCode(outcome.languageCode);
    return {outcome.voiceName, lang ? lang->id : std::string()};
}

std::string ProblemText(const std::string& interfaceLanguage, const ProblemFacts& facts, ProblemWhere where) {
    // A missing language name reads as "this language", so the sentence is never "Lexia can't read  yet".
    const std::string language =
        facts.language.empty() ? strings::Text(interfaceLanguage, strings::id::kWindowProblemThisLanguage) : facts.language;
    return strings::Format(interfaceLanguage, ProblemTextId(facts, where), {{"language", language}, {"quality", facts.quality}});
}
