#include "native_ui/pages/voice_sheets.h"

#include <algorithm>

#include "native_ui/pages/layout.h"
#include "native_ui/pages/voice_text.h"
#include "strings/string_ids.h"
#include "voice_names.h"

namespace nui {

namespace {

namespace sid = strings::id;

// ---- the numbers of the sheets (old ui/css/voice_library.css and components.css; DIPs) ---------------------
constexpr float kNameFont = 16, kSizeFont = 13, kLineFactor = 1.35f;       // `line-height: 1.35`
constexpr float kRowPadY = 9, kRowGap = 8, kInfoGap = 12;                   // row padding, progress / error gap, name to action
constexpr float kActionMinH = 30;                                           // `.voice-action { min-height: 30px }`
constexpr float kBarGap = 12;                                               // the bar to its words
constexpr float kMarkSize = 18, kMarkGap = 6;                               // the "Installed" check
constexpr float kRetryGap = 12;                                             // `.catalogue-state .btn { margin-top: 12px }`
constexpr float kLoadingMinH = 80;                                          // `.catalogue-body[data-view=loading]`
constexpr float kNetworkGap = 12;                                           // under the network note and the note

PageMessage Message(PageMessageType type, const char* typeText) { return MakeMessage(type, typeText); }

// A bold 16 px label, or a 13 px one, with a fixed line height (the design's 1.35).
Label* StyledLabel(Widget& parent, const std::string& text, float size, FontWeight weight) {
    Label* l = parent.Add<Label>(text);
    l->SetStyle(size, weight);
    return l;
}

// The check in a ring of the "Installed" word (the old inline SVG `MARK_OK`, 18 DIPs).
class InstalledMark : public Widget {
public:
    InstalledMark() { SetRole(Role::Image); }
    void Paint(Canvas& canvas, const Palette& p) override {
        const RectF r = Abs();
        const float s = r.w / 24.0f;  // the SVG's viewBox is 24
        canvas.StrokeEllipse({r.x + 2 * s, r.y + 2 * s, 20 * s, 20 * s}, p.accent, 2.4f * s);
        canvas.Polyline({{r.x + 7.5f * s, r.y + 12.5f * s}, {r.x + 10.7f * s, r.y + 15.7f * s}, {r.x + 16.5f * s, r.y + 9.3f * s}},
                        p.accent, 2.4f * s);
    }
};

// One row of the list. Its children are made by the DownloadSheet (they carry its callbacks); the row only
// places them: the name over the size on the left, the action cell on the right, and under them the progress
// (bar + words) or the notice of a failure. Measure lays the children out for the width and gives the height.
class VoiceRow : public Widget {
public:
    VoiceRow() { SetRole(Role::ListItem); }

    bool first = false;
    Label* name = nullptr;
    Label* size = nullptr;
    // The action cell: a button, or the mark + word of an installed Voice.
    Button* button = nullptr;
    InstalledMark* mark = nullptr;
    Label* installedWord = nullptr;
    ProgressBar* bar = nullptr;
    Label* barText = nullptr;
    Notice* error = nullptr;
    DottedRule* rule = nullptr;

    SizeF Measure(Canvas& canvas, float width) override {
        float y = first ? 0.0f : 1.0f;  // the dotted rule is the 1 DIP border-top of every row but the first
        if (rule) rule->SetBounds({0, 0, width, 1});
        y += kRowPadY;

        // The action cell, as wide as its content.
        SizeF action{0, kActionMinH};
        if (button) {
            const SizeF b = button->Measure(canvas, width);
            action = {b.w, std::max(kActionMinH, b.h)};
        } else if (mark) {
            const SizeF w = installedWord->Measure(canvas, width);
            action = {kMarkSize + kMarkGap + w.w, std::max(kActionMinH, std::max(kMarkSize, w.h))};
        }

        const float nameH = kNameFont * kLineFactor, sizeH = kSizeFont * kLineFactor;
        const float infoH = nameH + sizeH;
        const float contentH = std::max(infoH, action.h);
        const float infoW = std::max(0.0f, width - action.w - kInfoGap);
        name->SetBounds({0, y + (contentH - infoH) / 2, infoW, nameH});
        size->SetBounds({0, y + (contentH - infoH) / 2 + nameH, infoW, sizeH});
        const float ay = y + (contentH - action.h) / 2;
        if (button) {
            const SizeF b = button->Measure(canvas, width);
            button->SetBounds({width - b.w, ay + (action.h - b.h) / 2, b.w, b.h});
        } else if (mark) {
            const SizeF w = installedWord->Measure(canvas, width);
            const float x = width - action.w;
            mark->SetBounds({x, ay + (action.h - kMarkSize) / 2, kMarkSize, kMarkSize});
            installedWord->SetBounds({x + kMarkSize + kMarkGap, ay + (action.h - w.h) / 2, w.w, w.h});
        }
        y += contentH;

        if (bar) {
            y += kRowGap;
            const float lineH = kNameFont * kLineFactor;
            const SizeF t = barText->Measure(canvas, width);
            barText->SetBounds({width - t.w, y, t.w, lineH});
            bar->SetBounds({0, y + (lineH - 6) / 2, std::max(0.0f, width - t.w - kBarGap), 6});
            y += lineH;
        }
        if (error) {
            y += kRowGap;
            const float h = error->Measure(canvas, width).h;
            error->SetBounds({0, y, width, h});
            y += h;
        }
        y += kRowPadY;
        return {width, y};
    }
};

}  // namespace

// ================================================================================ Delete voice ====

Sheet* OpenDeleteVoiceSheet(Screen& screen, const PageEnv& env, const std::string& voice) {
    auto sheet = std::make_unique<Sheet>();
    sheet->SetTitle(strings::Format(env.interfaceLanguage, sid::kWindowDeleteVoiceTitle, {{"voice", VoiceLabelFromName(env.interfaceLanguage, voice)}}));
    // The body text of a sheet has the page's `line-height: 1.35` (21.6 for 16 px).
    Paragraph* text = sheet->AddBodyOf<Paragraph>(strings::Text(env.interfaceLanguage, sid::kWindowDeleteVoiceText));
    text->SetLineHeight(16 * kLineFactor);

    Sheet* raw = sheet.get();
    Screen* scr = &screen;
    auto confirm = std::make_unique<Button>(strings::Text(env.interfaceLanguage, sid::kWindowDeleteVoiceConfirm));
    confirm->SetOnActivate([raw, scr, voice] {
        // The host deletes the files and answers with a new state (or a failure sheet). Closing first: the
        // sheet is gone when the answer comes.
        PageMessage m = Message(PageMessageType::DeleteVoice, "deleteVoice");
        m.voice = voice;
        raw->Close();
        scr->Emit(m);
    });
    sheet->AddAction(std::move(confirm));
    auto cancel = std::make_unique<Button>(strings::Text(env.interfaceLanguage, sid::kWindowDeleteVoiceCancel));
    cancel->SetQuiet(true);
    cancel->SetOnActivate([raw] { raw->Close(); });
    Button* cancelRaw = sheet->AddAction(std::move(cancel));
    sheet->SetInitialFocus(cancelRaw);  // the safe choice has the focus, as in the old dialog
    return screen.OpenSheet(std::move(sheet));
}

// ================================================================================== Result sheet ====

ResultSheet::ResultSheet(Screen& screen, const PageEnv& env) : screen_(screen), env_(env) {}

ResultSheet::~ResultSheet() { *alive_ = false; }

void ResultSheet::Fill(const ResultOutcome& outcome) {
    const ResultTexts t = DescribeResult(env_.interfaceLanguage, outcome);
    sheet_->SetTitle(t.title);
    notice_->SetKind(t.ok ? Notice::Kind::Ok : Notice::Kind::Bad);
    notice_->SetLead(strings::Text(env_.interfaceLanguage, t.ok ? sid::kWindowResultDone : sid::kWindowResultProblem));
    notice_->SetText(t.text);
    again_->SetVisible(t.again);
}

void ResultSheet::Show(const ResultOutcome& outcome) {
    if (sheet_) {  // one sheet at a time: a new result replaces the words of the open one
        Fill(outcome);
        return;
    }
    auto sheet = std::make_unique<Sheet>();
    notice_ = sheet->AddBodyOf<Notice>();
    Screen* scr = &screen_;
    Sheet* raw = sheet.get();

    auto again = std::make_unique<Button>(strings::Text(env_.interfaceLanguage, sid::kWindowAddFileAgain));
    again->SetOnActivate([raw, scr] {
        raw->Close();
        scr->Emit(MakeMessage(PageMessageType::AddFromFile, "addFromFile"));  // the file dialog again
    });
    again_ = sheet->AddAction(std::move(again));
    auto close = std::make_unique<Button>(strings::Text(env_.interfaceLanguage, sid::kWindowAddFileClose));
    close->SetOnActivate([raw] { raw->Close(); });
    Button* closeRaw = sheet->AddAction(std::move(close));
    sheet->SetInitialFocus(closeRaw);

    std::weak_ptr<bool> alive = alive_;
    sheet->SetOnClose([this, alive] {
        if (alive.expired()) return;
        sheet_ = nullptr;
        notice_ = nullptr;
        again_ = nullptr;
    });
    sheet_ = screen_.OpenSheet(std::move(sheet));
    Fill(outcome);
}

// ====================================================================================== the body ====

CatalogueBody::CatalogueBody() { SetRole(Role::List); }

void CatalogueBody::Reset() {
    ClearChildren();
    items_.clear();
    reveal_ = nullptr;
    pendingFocus_ = nullptr;
    dirty_ = true;
    Invalidate();
}

void CatalogueBody::AddItem(Widget* w, float gapBefore, bool fullWidth) {
    items_.push_back({w, gapBefore, fullWidth});
    dirty_ = true;
}

void CatalogueBody::Place(Canvas& canvas, float width) {
    // When the list scrolls, its rows leave room for the scrollbar on the right, so the thumb never lies over
    // the "Installed" word or the progress text.
    const float rowWidth = reserve_ ? width - kScrollbarZone : width;
    float y = 0;
    bool firstItem = true;
    for (const Item& it : items_) {
        if (!firstItem) y += it.gapBefore;
        firstItem = false;
        const SizeF s = it.widget->Measure(canvas, rowWidth);
        it.widget->SetBounds({0, y, it.fullWidth ? rowWidth : s.w, s.h});
        y += s.h;
    }
    SetContentHeight(y);  // clamps the offset, never resets it
    placedWidth_ = width;
    dirty_ = false;
}

SizeF CatalogueBody::Measure(Canvas& canvas, float maxWidth) {
    if (dirty_ || placedWidth_ != maxWidth) Place(canvas, maxWidth);
    return {maxWidth, std::max(ContentHeight(), minH_)};
}

void CatalogueBody::PrepareLayout(Canvas& canvas) {
    Widget::PrepareLayout(canvas);
    // The sheet has set this body's bounds by now (it may be taller or shorter than when the items were
    // placed), so the offset is held to the real viewport, and the places on screen are known.
    SetContentHeight(ContentHeight());
    if (CanScroll() != reserve_) {  // the list started or stopped scrolling: its rows change width
        reserve_ = CanScroll();
        Place(canvas, Bounds().w);
    }
    if (pendingFocus_) {
        ScrollIntoView(pendingFocus_);
        pendingFocus_ = nullptr;
    }
    if (reveal_) {
        ScrollIntoView(reveal_);
        reveal_ = nullptr;
    }
}

void CatalogueBody::OnDescendantFocused(Widget* w) {
    if (dirty_) {
        pendingFocus_ = w;
        return;
    }
    ScrollArea::OnDescendantFocused(w);
}

// =================================================================================== the sheet ====

DownloadSheet::DownloadSheet(Screen& screen, const PageEnv& env, VoiceLibraryState& library)
    : screen_(screen), env_(env), library_(library) {}

DownloadSheet::~DownloadSheet() { *alive_ = false; }

void DownloadSheet::Open() {
    if (sheet_) return;
    auto sheet = std::make_unique<Sheet>(560.0f);  // `.sheet-wide`: 560 wide, 520 tall at most
    sheet->SetTitleGap(6);
    sheet->SetMaxHeight(520);
    sheet->SetTitle(T(sid::kWindowDownloadTitle));

    auto network = std::make_unique<Paragraph>();
    network->SetText(T(sid::kWindowDownloadNetwork));
    network->SetStyle(kSizeFont);
    network->SetLineHeight(kSizeFont * kLineFactor);
    sheet->AddBody(std::move(network), kNetworkGap);

    auto note = std::make_unique<Notice>();
    note_ = note.get();
    note_->SetVisible(false);
    sheet->AddBody(std::move(note), kNetworkGap);

    auto body = std::make_unique<CatalogueBody>();
    body_ = body.get();
    body_->SetName(T(sid::kWindowDownloadTitle));
    sheet->AddBody(std::move(body), 0);
    sheet->SetFlexBody(body_);

    Sheet* raw = sheet.get();
    auto close = std::make_unique<Button>(T(sid::kWindowDownloadClose));
    close->SetOnActivate([raw] { raw->Close(); });
    close_ = sheet->AddAction(std::move(close));
    sheet->SetInitialFocus(close_);

    // Closing the sheet does not stop a download: the library state goes on, and the sheet shows it again.
    std::weak_ptr<bool> alive = alive_;
    sheet->SetOnClose([this, alive] {
        if (alive.expired()) return;
        sheet_ = nullptr;
        note_ = nullptr;
        body_ = nullptr;
        close_ = nullptr;
        progressRow_ = nullptr;
        bar_ = nullptr;
        barText_ = nullptr;
        focusKeys_.clear();
    });
    sheet_ = screen_.OpenSheet(std::move(sheet));
    Rebuild();
}

void DownloadSheet::OnChange(VoiceLibraryState::Change change) {
    using Change = VoiceLibraryState::Change;
    switch (change) {
    case Change::Catalogue:
        // The catalogue (or its failure) came: the sheet opens if it is closed, with no old note.
        if (!sheet_) {
            library_.ClearNote();
            Open();
        } else {
            Refresh();
        }
        break;
    case Change::Rows:
        if (sheet_) Refresh();
        break;
    case Change::Progress:
        if (!sheet_) break;
        if (progressRow_) DrawProgress();  // the row is drawn as downloading: only its bar moves
        else Refresh();
        break;
    case Change::Result: break;  // the result sheet's
    }
}

// The list is built again after the event that asked for it is over: a click on a row's button changes the
// state, and the button must not be removed from inside its own callback.
void DownloadSheet::Refresh() {
    if (!sheet_ || refreshQueued_) return;
    refreshQueued_ = true;
    std::weak_ptr<bool> alive = alive_;
    screen_.Defer([this, alive] {
        if (alive.expired()) return;
        refreshQueued_ = false;
        Rebuild();
    });
}

Button* DownloadSheet::AddRowButton(Widget& row, const std::string& label, const std::string& aria,
                                    const std::string& key, bool disabled, std::function<void()> onClick) {
    Button* b = row.Add<Button>(label);
    b->SetSmall(true);
    b->SetName(aria);  // "Download en_US-amy-medium": the word alone says nothing to a screen reader
    b->SetEnabled(!disabled);
    b->SetOnActivate(std::move(onClick));
    focusKeys_[b] = key;
    return b;
}

void DownloadSheet::StartDownload(const CatalogueEntry& entry) {
    if (!library_.StartDownload(entry)) return;
    PageMessage m = Message(PageMessageType::Download, "download");
    m.name = entry.name;
    screen_.Emit(m);
    Refresh();
}

void DownloadSheet::Cancel() {
    const auto& d = library_.CurrentDownload();
    if (!d || d->cancelling) return;
    const std::string name = d->name;
    if (!library_.RequestCancel()) return;
    PageMessage m = Message(PageMessageType::CancelDownload, "cancelDownload");
    m.name = name;
    screen_.Emit(m);
    Refresh();
}

void DownloadSheet::Retry() {
    library_.Reload();
    screen_.Emit(Message(PageMessageType::OpenCatalogue, "openCatalogue"));
    Refresh();
}

// The list of one state: built from nothing each time, as the old renderSheet did.
void DownloadSheet::Rebuild() {
    if (!sheet_ || !body_) return;

    // The control that has the focus, by its key, so it can be given back to its successor.
    std::string focusedKey;
    if (const Widget* f = screen_.Focused()) {
        const auto it = focusKeys_.find(f);
        if (it != focusKeys_.end()) focusedKey = it->second;
    }
    focusKeys_.clear();
    progressRow_ = nullptr;
    bar_ = nullptr;
    barText_ = nullptr;

    // The note over the list: the last download done, or cancelled. Not while a list is got or has failed.
    const bool noteShown = library_.NoteKind() != VoiceLibraryState::Note::None && !library_.Failure() && !library_.Loading();
    note_->SetVisible(noteShown);
    if (noteShown) {
        if (library_.NoteKind() == VoiceLibraryState::Note::Done) {
            note_->SetKind(Notice::Kind::Ok);
            note_->SetLead(T(sid::kWindowResultDone));
            note_->SetText(strings::Format(env_.interfaceLanguage, sid::kWindowDownloadDone, {{"voice", VoiceLabelFromName(env_.interfaceLanguage, library_.NoteName())}}));
        } else {
            note_->SetKind(Notice::Kind::Plain);
            note_->SetLead(std::string());
            note_->SetText(T(sid::kWindowDownloadCancelled));
        }
    }

    body_->Reset();
    body_->SetMinHeight(0);
    const std::string reveal = library_.TakeReveal();
    Widget* revealRow = nullptr;

    if (library_.Failure()) {
        view_ = View::Failed;
        Notice* text = body_->Add<Notice>();
        text->SetKind(Notice::Kind::Bad);
        text->SetLead(T(sid::kWindowResultProblem));
        text->SetText(ProblemText(env_.interfaceLanguage, ProblemFacts{*library_.Failure(), {}, {}}, ProblemWhere::Catalogue));
        body_->AddItem(text, 0, true);
        Button* retry = body_->Add<Button>(T(sid::kWindowDownloadRetry));
        retry->SetOnActivate([this] { Retry(); });
        focusKeys_[retry] = "retry";
        body_->AddItem(retry, kRetryGap, false);
    } else if (library_.Loading() || !library_.HaveVoices()) {
        view_ = View::Loading;
        Label* wait = body_->Add<Label>(T(sid::kWindowDownloadLoading));
        wait->SetLiveRegion(true);  // "Getting the list of voices" is announced
        body_->AddItem(wait, 0, true);
        body_->SetMinHeight(kLoadingMinH);
    } else if (library_.Voices().empty()) {
        view_ = View::Empty;
        Label* none = body_->Add<Label>(T(sid::kWindowDownloadEmpty));
        body_->AddItem(none, 0, true);
    } else {
        view_ = View::List;
        bool first = true;
        const auto& running = library_.CurrentDownload();
        const auto& failed = library_.Failed();
        for (const CatalogueEntry& v : library_.Voices()) {
            VoiceRow* row = body_->Add<VoiceRow>();
            row->first = first;
            // Shown as "LJ Speech (Medium)"; the file name is the tooltip of the name (owner, Nov 11).
            const std::string shown = VoiceLabelFromName(env_.interfaceLanguage, v.name);
            row->SetName(shown);
            if (!first) row->rule = row->Add<DottedRule>();
            row->name = StyledLabel(*row, shown, kNameFont, FontWeight::Bold);
            row->name->SetTooltip(v.name);
            row->size = StyledLabel(*row, DownloadSizeText(env_.interfaceLanguage, v.sizeBytes), kSizeFont, FontWeight::Regular);

            const bool isRunning = running && running->name == v.name;
            const bool isFailed = !isRunning && failed && failed->name == v.name;
            const bool otherRuns = running.has_value();  // one download at a time: the other rows wait

            if (isRunning) {
                const bool cancelling = running->cancelling;
                row->button = AddRowButton(
                    *row, T(cancelling ? sid::kWindowDownloadCancelling : sid::kWindowDownloadCancel),
                    strings::Format(env_.interfaceLanguage, sid::kWindowDownloadCancelFor, {{"voice", shown}}), v.name,
                    cancelling, [this] { Cancel(); });
                row->bar = row->Add<ProgressBar>();
                row->bar->SetName(strings::Format(env_.interfaceLanguage, sid::kWindowDownloadProgressLabel, {{"voice", shown}}));
                row->barText = StyledLabel(*row, std::string(), kNameFont, FontWeight::Bold);
                progressRow_ = row;
                bar_ = row->bar;
                barText_ = row->barText;
            } else if (isFailed) {
                const CatalogueEntry entry = v;
                row->button = AddRowButton(
                    *row, T(sid::kWindowDownloadRetry),
                    strings::Format(env_.interfaceLanguage, sid::kWindowDownloadRetryFor, {{"voice", shown}}), v.name,
                    otherRuns, [this, entry] { StartDownload(entry); });
                row->error = row->Add<Notice>();
                row->error->SetKind(Notice::Kind::Bad);
                row->error->SetLead(T(sid::kWindowResultProblem));
                row->error->SetText(ProblemText(env_.interfaceLanguage, failed->facts, ProblemWhere::Download));
            } else if (library_.IsInstalled(v)) {
                row->mark = row->Add<InstalledMark>();
                row->installedWord = StyledLabel(*row, T(sid::kWindowDownloadInstalled), kNameFont, FontWeight::Bold);
            } else {
                const CatalogueEntry entry = v;
                row->button = AddRowButton(
                    *row, T(sid::kWindowDownloadGet),
                    strings::Format(env_.interfaceLanguage, sid::kWindowDownloadGetFor, {{"voice", shown}}), v.name,
                    otherRuns, [this, entry] { StartDownload(entry); });
            }
            body_->AddItem(row, 0, true);
            if (v.name == reveal) revealRow = row;
            first = false;
        }
    }
    if (revealRow) body_->Reveal(revealRow);

    // The focus stays on "the same" control, also when it became another one (Download -> Cancel -> Try
    // again). When it cannot (that button is gone or disabled) it goes to Close, not to nowhere.
    if (!focusedKey.empty()) {
        Widget* again = nullptr;
        for (const auto& [w, key] : focusKeys_) {
            if (key == focusedKey && w->FocusableNow()) again = const_cast<Widget*>(w);
        }
        (again ? again : static_cast<Widget*>(close_))->Focus();
    }
    DrawProgress();
    screen_.Invalidate();
}

// The values that change often are set apart: a progress message moves the bar and its words and rebuilds
// nothing, so the focus stays on Cancel.
void DownloadSheet::DrawProgress() {
    const auto& d = library_.CurrentDownload();
    if (!d || !bar_ || !barText_) return;
    const uint64_t total = d->total > 0 ? d->total : 1;
    const uint64_t done = std::min(d->done, total);
    bar_->SetFraction(static_cast<double>(done) / static_cast<double>(total));
    const std::string text = DownloadProgressText(env_.interfaceLanguage, done, total);
    bar_->SetValueText(text);
    if (barText_->Text() != text) {
        barText_->SetText(text);
        // The words have their own width ("9.9 of 63 MB" is wider than "9 of 63 MB"), which the bar gives
        // way to: the body lays its rows out again at the next frame.
        if (body_) body_->MarkLayout();
    }
}

}  // namespace nui
