#include "native_ui/voice_library_state.h"

#include "strings/string_ids.h"
#include "voice_names.h"

namespace nui {

ResultTexts DescribeResult(const std::string& lang, const ResultOutcome& o) {
    namespace id = strings::id;
    ResultTexts t;
    switch (o.kind) {
    case ResultKind::VoiceAdded:
        t.ok = true;
        t.title = strings::Text(lang, id::kWindowAddFileAddedTitle);
        t.text = strings::Format(
            lang, id::kWindowAddFileAdded,
            {{"voice", VoiceLabelFromName(lang, o.voice)}, {"language", o.language.empty() ? std::string() : strings::LanguageName(lang, o.language)}});
        break;
    case ResultKind::VoiceRefused:
        t.again = true;  // the person may choose another file at once
        t.title = strings::Text(lang, id::kWindowAddFileRefusedTitle);
        t.text = ProblemText(lang, o.facts, ProblemWhere::File);
        break;
    case ResultKind::DeleteFailed:
        t.title = strings::Text(lang, id::kWindowDeleteVoiceFailedTitle);
        t.text = strings::Format(lang, id::kWindowDeleteVoiceFailed, {{"voice", VoiceLabelFromName(lang, o.voice)}});
        break;
    case ResultKind::PresetSaved:
        t.ok = true;
        t.title = strings::Text(lang, id::kWindowPresetSavedTitle);
        t.text = strings::Format(lang, id::kWindowPresetSaved, {{"name", o.name}});
        break;
    case ResultKind::PresetLoaded:
        t.ok = true;
        t.title = strings::Text(lang, id::kWindowPresetLoadedTitle);
        // A Voice that is not the active one gets its controls saved, but the sound does not change yet.
        t.text = strings::Format(lang, o.voiceIsActive ? id::kWindowPresetLoaded : id::kWindowPresetLoadedOther,
                                 {{"voice", VoiceLabelFromName(lang, o.voice)}});
        break;
    case ResultKind::PresetFailed:
        t.title = strings::Text(lang, id::kWindowPresetFailedTitle);
        if (o.preset == PresetProblem::VoiceNotInstalled) {
            t.text = strings::Format(lang, id::kWindowPresetVoiceNotInstalled, {{"voice", o.voice}});
        } else {
            t.text = strings::Text(lang, o.preset == PresetProblem::CannotWrite ? id::kWindowPresetCannotWrite
                                         : o.preset == PresetProblem::NoVoice   ? id::kWindowPresetNoVoice
                                                                                : id::kWindowPresetBadFile);
        }
        break;
    }
    return t;
}

// ------------------------------------------------------------------------------ the person ---

void VoiceLibraryState::BeginSheet() {
    // A new sheet starts from nothing: no old note, no old failure, no catalogue. A download that runs stays
    // (closing the sheet does not stop it; the sheet shows it again).
    note_ = Note::None;
    failedRow_.reset();
    failure_.reset();
    voices_.clear();
    haveVoices_ = false;
    loading_ = true;
    installedNow_.clear();
}

void VoiceLibraryState::Reload() {
    failure_.reset();
    loading_ = true;
    voices_.clear();
    haveVoices_ = false;
}

bool VoiceLibraryState::StartDownload(const CatalogueEntry& entry) {
    if (download_) return false;  // one at a time (the host refuses a second as well)
    failedRow_.reset();
    note_ = Note::None;
    download_ = Download{entry.name, 0, entry.sizeBytes, false};
    return true;
}

bool VoiceLibraryState::RequestCancel() {
    if (!download_ || download_->cancelling) return false;
    download_->cancelling = true;
    return true;
}

std::string VoiceLibraryState::TakeReveal() {
    std::string r;
    r.swap(reveal_);
    return r;
}

std::optional<ResultOutcome> VoiceLibraryState::TakeResult() {
    std::optional<ResultOutcome> r;
    r.swap(result_);
    return r;
}

// ------------------------------------------------------------------------- the host's answers ---

void VoiceLibraryState::CatalogueShown(std::vector<CatalogueEntry> voices) {
    voices_ = std::move(voices);
    haveVoices_ = true;
    loading_ = false;
    failure_.reset();
    Tell(Change::Catalogue);
}

void VoiceLibraryState::CatalogueFailed(VoiceProblem problem) {
    loading_ = false;
    voices_.clear();
    haveVoices_ = false;
    failure_ = problem;
    Tell(Change::Catalogue);
}

void VoiceLibraryState::DownloadProgress(const std::string& name, uint64_t done, uint64_t total) {
    const bool same = download_ && download_->name == name;
    // The cancelling flag is the person's; a message of the host never clears it.
    download_ = Download{name, done, total, same ? download_->cancelling : false};
    failedRow_.reset();
    // The first message of a download (or one this sheet did not start) changes which row is drawn how; the
    // later ones only move the bar.
    Tell(same ? Change::Progress : Change::Rows);
}

void VoiceLibraryState::DownloadDone(const std::string& name) {
    download_.reset();
    failedRow_.reset();
    installedNow_.insert(name);
    note_ = Note::Done;
    noteName_ = name;
    reveal_ = name;
    Tell(Change::Rows);
}

void VoiceLibraryState::DownloadFailed(const std::string& name, const ProblemFacts& facts) {
    download_.reset();
    if (facts.problem == VoiceProblem::Cancelled) {
        failedRow_.reset();  // a cancel is no error: a plain note says so
        note_ = Note::Cancelled;
        noteName_ = name;
    } else {
        note_ = Note::None;
        reveal_ = name;  // the error is in its row, which may be out of sight
        failedRow_ = FailedRow{name, facts};
    }
    Tell(Change::Rows);
}

void VoiceLibraryState::PostResult(ResultOutcome outcome) {
    result_ = std::move(outcome);
    Tell(Change::Result);
}

void VoiceLibraryState::VoiceAdded(const VoiceAddedFacts& added) {
    ResultOutcome o;
    o.kind = ResultKind::VoiceAdded;
    o.voice = added.voice;
    o.language = added.language;
    PostResult(std::move(o));
}

void VoiceLibraryState::VoiceRefused(const ProblemFacts& facts) {
    ResultOutcome o;
    o.kind = ResultKind::VoiceRefused;
    o.facts = facts;
    PostResult(std::move(o));
}

void VoiceLibraryState::VoiceDeleteFailed(const std::string& voice, VoiceProblem) {
    // One text for every reason: the host's refusal of a Voice that was never offered for deleting is Busy,
    // and the sheet has no other sentence for it ("check that no other program uses its files").
    ResultOutcome o;
    o.kind = ResultKind::DeleteFailed;
    o.voice = voice;
    PostResult(std::move(o));
}

void VoiceLibraryState::PresetSaved(const std::string& fileName) {
    ResultOutcome o;
    o.kind = ResultKind::PresetSaved;
    o.name = fileName;
    PostResult(std::move(o));
}

void VoiceLibraryState::PresetLoaded(const std::string& voice, bool voiceIsActive) {
    ResultOutcome o;
    o.kind = ResultKind::PresetLoaded;
    o.voice = voice;
    o.voiceIsActive = voiceIsActive;
    PostResult(std::move(o));
}

void VoiceLibraryState::PresetFailed(PresetProblem problem, const std::string& voice) {
    ResultOutcome o;
    o.kind = ResultKind::PresetFailed;
    o.preset = problem;
    o.voice = voice;
    PostResult(std::move(o));
}

}  // namespace nui
