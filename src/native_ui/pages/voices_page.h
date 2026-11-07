#pragma once
// The Voices page (K.3, ADR-0009; ui/index.html `#page-voices`, ui/js/voice.js, ui/css/pages.css): the Voice row
// (a Select of the installed Voices and three icon buttons: Download, Add from file, Delete), the "no voice"
// note, the Speed / Pitch / Volume sliders with a reset button beside a changed one, the otter, and the
// Save preset / Load preset links. It draws what the last `state` says and keeps no copy of a setting: a
// user action is a PageMessage (setVoice, setControl, resetControl, openCatalogue, addFromFile, savePresetFile,
// loadPresetFile, deleteVoice) and the next state moves the control.
//
// The page also owns the three sheets (voice_sheets.h). Their data is the Settings window's VoiceLibraryState,
// which survives a page change; the page tells its DownloadSheet what changed and shows a result that is
// waiting. Delete opens the Delete voice sheet; its Confirm sends deleteVoice.
#include <memory>
#include <string>
#include <vector>

#include "native_ui/controls.h"
#include "native_ui/pages/page.h"
#include "native_ui/pages/voice_sheets.h"
#include "native_ui/select.h"
#include "native_ui/slider.h"
#include "native_ui/voice_library_state.h"

namespace nui {

class VoicesPage : public PageView {
public:
    VoicesPage(const PageEnv& env, VoiceLibraryState& library);
    ~VoicesPage() override;

    void SetState(const UiState& state) override;

    // What the Voice library's state said changed (SettingsUi forwards it while this page shows).
    void OnLibraryChange(VoiceLibraryState::Change change);
    // Shows a result that came while no page was there. Call once the page is in the tree.
    void ShowPendingResult();

    // The Delete icon asks for the Delete voice sheet (when the active Voice may be deleted).
    void OpenDeleteSheet();
    // The download icon: a new sheet, loading, and the host is asked for the catalogue.
    void OpenDownloadSheet();

    // For the tests and the picture tool.
    Select* VoiceSelect() const { return voice_; }
    IconButton* DownloadButton() const { return download_; }
    IconButton* AddFileButton() const { return addFile_; }
    IconButton* DeleteButton() const { return delete_; }
    TextLink* SavePresetLink() const { return save_; }
    TextLink* LoadPresetLink() const { return load_; }
    Paragraph* NoneNote() const { return none_; }
    PictureWidget* Otter() const { return otter_; }
    Slider* SliderFor(const std::string& name) const;       // "speed" | "pitch" | "volume"
    IconButton* ResetFor(const std::string& name) const;
    DownloadSheet* Downloads() const { return downloads_.get(); }  // null until first used
    ResultSheet* Results() const { return results_.get(); }

protected:
    void Relayout(Canvas& canvas) override;

private:
    struct ControlRow {
        std::string name;        // "speed" | "pitch" | "volume"
        float top = 0;
        IconWidget* icon = nullptr;
        Label* label = nullptr;
        Slider* slider = nullptr;
        IconButton* reset = nullptr;
    };
    ControlRow* Row(const std::string& name);
    const ControlRow* Row(const std::string& name) const;
    ControlRow AddControl(const char* name, float top, const char* icon, const char* textId);
    DownloadSheet& EnsureDownloads();
    ResultSheet& EnsureResults();
    bool CanDelete() const;

    VoiceLibraryState& library_;

    IconWidget* voiceIcon_ = nullptr;
    Label* voiceLabel_ = nullptr;
    Select* voice_ = nullptr;
    IconButton* download_ = nullptr;
    IconButton* addFile_ = nullptr;
    IconButton* delete_ = nullptr;
    Paragraph* none_ = nullptr;
    DottedRule* rule_ = nullptr;
    std::vector<ControlRow> rows_;
    PictureWidget* otter_ = nullptr;
    TextLink* save_ = nullptr;
    TextLink* load_ = nullptr;

    // What the last state said (the page draws it; nothing here is a setting).
    std::string language_;                 // state.language: the Language id the choice is for
    std::vector<std::string> installed_;
    std::string active_;
    std::vector<std::string> removable_;
    bool hasVoice_ = false;                // a Voice is installed and active

    std::unique_ptr<DownloadSheet> downloads_;
    std::unique_ptr<ResultSheet> results_;
};

}  // namespace nui
