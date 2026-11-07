#pragma once
// The three sheets of the Voices page (K.3, ADR-0009; the old ui/index.html dialogs `delete-voice`,
// `catalogue` and `file-result`, and old ui/js/voice_library.js): Delete voice, Voice download, and the Result
// sheet of Add from file, a failed delete and the Preset files. They are Sheets of the Screen (modal, Esc
// closes, the focus goes in and comes back), built on demand. The Voices page owns one DownloadSheet and one
// ResultSheet; the data they draw is the VoiceLibraryState of the Settings window.
//
// A user action is a PageMessage on Screen::Emit; nothing here changes a setting.
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "native_ui/display.h"
#include "native_ui/pages/page.h"
#include "native_ui/screen.h"
#include "native_ui/scroll_area.h"
#include "native_ui/sheet.h"
#include "native_ui/voice_library_state.h"

namespace nui {

// "Delete {voice}?": Delete (sends deleteVoice) and Keep it; the focus starts on Keep it. Returns the sheet.
Sheet* OpenDeleteVoiceSheet(Screen& screen, const PageEnv& env, const std::string& voice);

// The result of Add from file, a failed delete and the Preset files: a title, a Done / Problem notice and
// Close; a refused file also has "Choose another file" (sends addFromFile, after the sheet is closed).
class ResultSheet {
public:
    ResultSheet(Screen& screen, const PageEnv& env);
    ~ResultSheet();
    ResultSheet(const ResultSheet&) = delete;
    ResultSheet& operator=(const ResultSheet&) = delete;

    // Opens the sheet with this result; a result sheet that is open shows the new result instead of a second one.
    void Show(const ResultOutcome& outcome);
    bool IsOpen() const { return sheet_ != nullptr; }
    Sheet* GetSheet() const { return sheet_; }

private:
    void Fill(const ResultOutcome& outcome);

    Screen& screen_;
    const PageEnv env_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
    Sheet* sheet_ = nullptr;
    Notice* notice_ = nullptr;
    Button* again_ = nullptr;
};

// The scrolling body of the download sheet: items stacked down the content (rows of the list, or the one
// line / notice + button of a state), each as tall as it measures for the width. It lays them out when they or
// the width change, reports its content's height as the height it wants (the Sheet makes it shorter when the
// card would be taller than its maximum), and scrolls a row into view once the sheet has set its bounds.
class CatalogueBody : public ScrollArea {
public:
    CatalogueBody();
    // Removes every item (the scroll offset stays: a list that is drawn again does not jump to the top).
    void Reset();
    // `w` is a child of this body already. `fullWidth`: as wide as the body, else as wide as it measures.
    void AddItem(Widget* w, float gapBefore, bool fullWidth);
    void SetMinHeight(float h) { minH_ = h; }  // the wanted height is at least this (the loading view)
    void Reveal(const Widget* w) { reveal_ = w; }  // scrolled into view at the next layout
    void MarkLayout() { dirty_ = true; Invalidate(); }  // an item changed its size: place the items again

    SizeF Measure(Canvas& canvas, float maxWidth) override;
    void PrepareLayout(Canvas& canvas) override;
    // A focus that lands before the rows are placed (the list was just built again) cannot be scrolled to yet: it
    // is remembered and scrolled to after the layout, so a rebuild never throws the position away.
    void OnDescendantFocused(Widget* w) override;

private:
    struct Item {
        Widget* widget;
        float gapBefore;
        bool fullWidth;
    };
    void Place(Canvas& canvas, float width);

    std::vector<Item> items_;
    float minH_ = 0;
    float placedWidth_ = -1;
    bool dirty_ = true;
    const Widget* reveal_ = nullptr;
    Widget* pendingFocus_ = nullptr;
    bool reserve_ = false;  // the rows leave room for the scrollbar
};


// The Voice download sheet: the network note, a note about the last download, and the list of Voices
// (a ScrollArea) in one of four views: loading, failed (+ Try again), empty or the list (a row each: name, size,
// and Download / Cancel / Cancelling / Try again + notice / Installed). One download at a time. The list is
// built again when the data changes, but the scroll position and the focus (by the Voice's name) are kept, and a
// progress message only moves the bar of its row.
class DownloadSheet {
public:
    DownloadSheet(Screen& screen, const PageEnv& env, VoiceLibraryState& library);
    ~DownloadSheet();
    DownloadSheet(const DownloadSheet&) = delete;
    DownloadSheet& operator=(const DownloadSheet&) = delete;

    // Makes the sheet and opens it (the caller has begun the library state: loading).
    void Open();
    bool IsOpen() const { return sheet_ != nullptr; }
    Sheet* GetSheet() const { return sheet_; }
    // What the library state says changed (VoiceLibraryState::SetOnChange, through the page).
    void OnChange(VoiceLibraryState::Change change);

    // For the tests and the picture tool.
    Button* CloseButton() const { return close_; }
    CatalogueBody* Body() const { return body_; }
    Notice* NoteNotice() const { return note_; }

private:
    enum class View { Loading, Failed, Empty, List };
    void Refresh();       // the list again, after the event that asked is over
    void Rebuild();
    void DrawProgress();  // the bar and the words of the running download
    void StartDownload(const CatalogueEntry& entry);
    void Cancel();
    void Retry();
    Button* AddRowButton(Widget& row, const std::string& label, const std::string& aria, const std::string& key,
                         bool disabled, std::function<void()> onClick);
    std::string T(const char* id) const { return strings::Text(env_.interfaceLanguage, id); }

    Screen& screen_;
    const PageEnv env_;
    VoiceLibraryState& library_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
    Sheet* sheet_ = nullptr;
    Notice* note_ = nullptr;
    CatalogueBody* body_ = nullptr;
    Button* close_ = nullptr;
    View view_ = View::Loading;
    // The widgets of the rows by the focus key of their button ("<Voice name>", or "retry"), so the focus can
    // come back to "the same" control after the rows were built again (Download -> Cancel -> Try again).
    std::map<const Widget*, std::string> focusKeys_;
    // The row that draws the running download (null when none): its bar and text move without a rebuild.
    Widget* progressRow_ = nullptr;
    ProgressBar* bar_ = nullptr;
    Label* barText_ = nullptr;
    bool refreshQueued_ = false;
};

}  // namespace nui
