#pragma once
// The Voice library's answers to the Settings window, as typed values (K.3, ADR-0009). They replace the
// JSON messages of the old page (`catalogue`, `downloadProgress`, `voiceAdded`, `presetSaved` ... in
// ui_host.*): VoiceTab calls a VoiceEventSink instead of building JSON text, and the native pages (the
// Voice download sheet, the result sheet) take the values as they are. Pure and Win32-free, in lexia_core.
//
// Where a problem is shown decides some of its words (a network failure of the catalogue reads differently
// from one of a download), so the text of a problem is picked with `ProblemTextId(facts, where)`; the facts
// (the language's English name, the quality) go into the text as {language} and {quality}. The ids and the
// rule are those of the old ui/js/voice_library.js (`problemText`).
#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "language.h"
#include "tts/voice_catalogue.h"
#include "tts/voice_library.h"
#include "ui_host.h"

// One downloadable Voice of the catalogue, as the sheet lists it.
struct CatalogueEntry {
    std::string name;
    std::string language;     // a Lexia Language id ("eng")
    uint64_t sizeBytes = 0;   // model + config
    bool installed = false;   // the Voice is in the Voice library already
};

// A refusal or a failure with what the text needs: the problem and, where the outcome carries them, the
// language's English name ("German") and the quality ("low").
struct ProblemFacts {
    VoiceProblem problem = VoiceProblem::None;
    std::string language;
    std::string quality;
};

// Where a problem is shown: the list of the Voice download sheet, one row of it, or a result sheet (Add from file).
enum class ProblemWhere { Catalogue, Download, File };

// The string id (strings/) of a problem's text in a place. A problem with no text of its own (Busy, None,
// Cancelled, a code the page does not know) gives `window.problem.unknown`.
const char* ProblemTextId(const ProblemFacts& facts, ProblemWhere where);

// The facts of a refused or failed outcome.
ProblemFacts FactsOf(const VoiceOutcome& outcome);

// The Lexia Language for a Voice language code ("en" or the catalogue's "en_GB"); nullptr if none is offered
// (ADR-0007).
const Language* OfferedLanguageForCode(const std::string& code);

// The downloadable Voices for the Voice download sheet: only those of offered Languages, in the order of the
// catalogue. `installedNames` marks the ones that are installed already.
std::vector<CatalogueEntry> OfferedCatalogue(const std::vector<CatalogueVoice>& voices,
                                             const std::set<std::string>& installedNames);

// What a Voice added from a file says: its name and the Lexia Language id it speaks ("" if none is offered).
struct VoiceAddedFacts {
    std::string voice;
    std::string language;
};
VoiceAddedFacts AddedFactsOf(const VoiceOutcome& outcome);  // outcome.ok()

// The receiver of the Voice library's answers: the native Settings window. Every call is on the UI thread.
// A sink that has nobody to tell (no window open) is simply not called: VoiceTab asks its Host for the sink
// each time and does nothing when it gets none.
class VoiceEventSink {
public:
    virtual ~VoiceEventSink() = default;

    virtual void CatalogueShown(std::vector<CatalogueEntry> voices) = 0;
    virtual void CatalogueFailed(VoiceProblem problem) = 0;
    virtual void DownloadProgress(const std::string& name, uint64_t done, uint64_t total) = 0;
    virtual void DownloadDone(const std::string& name) = 0;
    // `facts.problem == Cancelled` is the answer to a cancel: no error is shown for it.
    virtual void DownloadFailed(const std::string& name, const ProblemFacts& facts) = 0;
    virtual void VoiceAdded(const VoiceAddedFacts& added) = 0;
    virtual void VoiceRefused(const ProblemFacts& facts) = 0;
    virtual void VoiceDeleteFailed(const std::string& voice, VoiceProblem problem) = 0;
    virtual void PresetSaved(const std::string& fileName) = 0;
    virtual void PresetLoaded(const std::string& fileName) = 0;
    virtual void PresetFailed(PresetProblem problem) = 0;
};

// The words of a problem in the Interface language `interfaceLanguage`: the text of ProblemTextId with {language}
// (the facts' language, or "this language" when none came) and {quality} filled in.
std::string ProblemText(const std::string& interfaceLanguage, const ProblemFacts& facts, ProblemWhere where);
