#pragma once
// VoiceLibraryState: what the Voice download sheet and the result sheet show, kept by the Settings window
// (SettingsUi owns one) and not by the Voices page, so a download goes on being known while another page shows
// and the page can be built again (K.3, ADR-0009). Pure and Win32-free, in lexia_core, tested.
//
// It is the port of the state of old ui/js/voice_library.js: which catalogue came, the download that runs, the
// last failure of a row, the note over the list, the Voices installed since the catalogue came, the row to
// scroll into view. It is not a setting (the installed Voices are drawn from the host's `state`); it is the
// sheets' own progress. It is the VoiceEventSink the host (VoiceTab) calls, and it tells whoever listens
// (the Voices page, when it shows) what kind of change came.
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "strings/strings.h"
#include "voice_events.h"

namespace nui {

// What a result sheet says (Add from file, Delete a Voice, a Preset file): the event, kept until a sheet shows it.
enum class ResultKind { VoiceAdded, VoiceRefused, DeleteFailed, PresetSaved, PresetLoaded, PresetFailed };

struct ResultOutcome {
    ResultKind kind = ResultKind::VoiceAdded;
    std::string voice;      // VoiceAdded, DeleteFailed, PresetLoaded; PresetFailed (VoiceNotInstalled): the name from the file
    std::string language;   // VoiceAdded: a Lexia Language id
    std::string name;       // PresetSaved: the file's name
    bool voiceIsActive = true;  // PresetLoaded: the Voice is the active one
    ProblemFacts facts;     // VoiceRefused
    PresetProblem preset = PresetProblem::BadFile;  // PresetFailed
};

// The words of a result sheet, in an Interface language.
struct ResultTexts {
    bool ok = false;      // a "Done" notice (else "Problem")
    bool again = false;   // "Choose another file" is offered
    std::string title;
    std::string text;
};
ResultTexts DescribeResult(const std::string& interfaceLanguage, const ResultOutcome& outcome);

class VoiceLibraryState : public VoiceEventSink {
public:
    struct Download {
        std::string name;
        uint64_t done = 0, total = 0;
        bool cancelling = false;
    };
    struct FailedRow {
        std::string name;
        ProblemFacts facts;
    };
    enum class Note { None, Done, Cancelled };

    // What a listener is told. Catalogue: the catalogue or its failure came (open the sheet if it is closed,
    // draw it again). Rows: the rows change (a download ended, started, failed): draw the list again.
    // Progress: only the bytes of the running download changed. Result: a result is waiting (TakeResult).
    enum class Change { Catalogue, Rows, Progress, Result };
    void SetOnChange(std::function<void(Change)> fn) { onChange_ = std::move(fn); }

    // ---- what the sheets draw
    bool Loading() const { return loading_; }
    const std::optional<VoiceProblem>& Failure() const { return failure_; }
    bool HaveVoices() const { return haveVoices_; }
    const std::vector<CatalogueEntry>& Voices() const { return voices_; }
    const std::optional<Download>& CurrentDownload() const { return download_; }
    const std::optional<FailedRow>& Failed() const { return failedRow_; }
    Note NoteKind() const { return note_; }
    const std::string& NoteName() const { return noteName_; }
    bool IsInstalled(const CatalogueEntry& v) const { return v.installed || installedNow_.count(v.name) > 0; }
    // The row to scroll into view when the list is drawn next; reading it clears it.
    std::string TakeReveal();
    // A result that came and no sheet has shown yet.
    std::optional<ResultOutcome> TakeResult();
    bool HasResult() const { return result_.has_value(); }

    // ---- what the person does
    // The download icon: a new sheet starts loading (the host is asked for the catalogue by the page).
    void BeginSheet();
    // Try again after the catalogue failed.
    void Reload();
    void ClearNote() { note_ = Note::None; }
    // Download (or Try again) on a row; false when a download runs already (one at a time).
    bool StartDownload(const CatalogueEntry& entry);
    // Cancel on the running download; false when none runs or it is cancelling already.
    bool RequestCancel();

    // ---- VoiceEventSink: the host's answers
    void CatalogueShown(std::vector<CatalogueEntry> voices) override;
    void CatalogueFailed(VoiceProblem problem) override;
    void DownloadProgress(const std::string& name, uint64_t done, uint64_t total) override;
    void DownloadDone(const std::string& name) override;
    void DownloadFailed(const std::string& name, const ProblemFacts& facts) override;
    void VoiceAdded(const VoiceAddedFacts& added) override;
    void VoiceRefused(const ProblemFacts& facts) override;
    void VoiceDeleteFailed(const std::string& voice, VoiceProblem problem) override;
    void PresetSaved(const std::string& fileName) override;
    void PresetLoaded(const std::string& voice, bool voiceIsActive) override;
    void PresetFailed(PresetProblem problem, const std::string& voice) override;

private:
    void Tell(Change c) const {
        if (onChange_) onChange_(c);
    }
    void PostResult(ResultOutcome outcome);  // keeps it for a sheet and says Change::Result

    bool loading_ = false;
    std::optional<VoiceProblem> failure_;
    bool haveVoices_ = false;
    std::vector<CatalogueEntry> voices_;
    std::optional<Download> download_;
    std::optional<FailedRow> failedRow_;
    Note note_ = Note::None;
    std::string noteName_;
    std::set<std::string> installedNow_;  // Voices that finished downloading since the catalogue came
    std::string reveal_;
    std::optional<ResultOutcome> result_;
    std::function<void(Change)> onChange_;
};

}  // namespace nui
