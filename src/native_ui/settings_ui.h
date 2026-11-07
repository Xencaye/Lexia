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
// How the later tasks plug in without touching the frame:
//   K.3  adds typed events (catalogue, download progress, add / delete / preset results) as methods of
//        VoicesPage, and a SettingsUi accessor `VoicesPage* Voices()` that returns the page while it shows
//        (events for a page that is not shown are kept by the window layer of K.3, not here).
//   K.4  adds ShowPage(Page, ShowOptions) (select an action, start a capture) next to the one-argument one.
// Both fill voices_page.* / settings_page.* and layout.h; pages.cpp's MakePage already makes them.
#include <memory>
#include <string>

#include "native_ui/pages/frame.h"
#include "native_ui/pages/page.h"
#include "native_ui/screen.h"

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

private:
    std::unique_ptr<PageView> MakePage(Page page) const;

    Screen& screen_;
    PageEnv env_;
    Frame* frame_ = nullptr;
    Panel* host_ = nullptr;  // the parent of the current page, after the frame in the tree
    PageView* view_ = nullptr;
    Page page_ = Page::Home;
    UiState state_;
    bool hasState_ = false;
    bool dark_ = false;
};

}  // namespace nui
