#pragma once
// SettingsUi: the whole content of the native Settings window as a widget tree (K.2, ADR-0009). Pure and
// Win32-free, in lexia_core: the window (settings_window.*) and the picture tool (lexia_settingsshot) host the
// same object, the tests drive it with a FakeCanvas.
//
// It builds into a Screen it is given: the Frame (top and bottom links, rules, Master volume, flute) once,
// and below it one page at a time. What it offers the window:
//   ShowPage(page)     switches the page (the link of the frame does the same, locally: which page shows is
//                      the one thing the UI owns). Same page again: nothing happens.
//   SetState(state)    redraws from the host's state. Nothing here keeps a copy of a setting: every widget shows
//                      what the last state said, and a user action is a PageMessage on Screen::Emit (the window
//                      sets the sink to App::HandlePageMessage) that the next state answers.
//   SetDark / Dark     the theme in use. SetState takes it from the state (`dark`). The window picks its
//                      Palette by Dark() when it paints.
//
// The Voice library (K.3): VoiceEvents() is the sink VoiceTab calls with the typed answers of the host
// (catalogue, download progress, add / delete / preset results, voice_events.h). They land in a
// VoiceLibraryState that this object owns, so a download is known while another page shows; the Voices page
// draws it (and opens its sheets) while it shows, and a result that came while it did not is shown when it is built.
// How a later task plugs in without touching the frame:
//   K.4  adds ShowPage(Page, ShowOptions) (select an action, start a capture) next to the one-argument one.
#include <memory>
#include <string>

#include "native_ui/pages/frame.h"
#include "native_ui/pages/page.h"
#include "native_ui/screen.h"
#include "native_ui/voice_library_state.h"

namespace nui {

class SettingsUi {
public:
    // Builds the frame and the Home page into `screen` (800 x 600 DIPs). `screen` must outlive this object.
    // Texts come from strings::Text in `interfaceLanguage` ("en").
    explicit SettingsUi(Screen& screen, std::string interfaceLanguage = "en");
    ~SettingsUi();
    SettingsUi(const SettingsUi&) = delete;
    SettingsUi& operator=(const SettingsUi&) = delete;

    void ShowPage(Page page);
    Page CurrentPage() const { return page_; }
    PageView* Current() const { return view_; }  // the page that shows (never null)
    Frame& GetFrame() { return *frame_; }

    void SetState(const UiState& state);
    bool HasState() const { return hasState_; }
    const UiState& State() const { return state_; }

    void SetDark(bool dark);
    bool Dark() const { return dark_; }

    // The Voice library's answers (VoiceTab calls this sink; the window hands it out). The data lives here, not
    // in the Voices page, so a download is known while another page shows; the page draws it when it shows.
    VoiceEventSink& VoiceEvents() { return library_; }
    VoiceLibraryState& Library() { return library_; }

private:
    std::unique_ptr<PageView> MakePage(Page page);

    Screen& screen_;
    PageEnv env_;
    Frame* frame_ = nullptr;
    Panel* host_ = nullptr;  // the parent of the current page, after the frame in the tree
    PageView* view_ = nullptr;
    Page page_ = Page::Home;
    UiState state_;
    bool hasState_ = false;
    bool dark_ = false;
    VoiceLibraryState library_;  // the Voice download sheet and the results, whichever page shows
};

}  // namespace nui
