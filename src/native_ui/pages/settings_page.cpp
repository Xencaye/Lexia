#include "native_ui/pages/settings_page.h"

#include <algorithm>

#include "key_chord.h"
#include "native_ui/animation.h"
#include "native_ui/pages/layout.h"
#include "native_ui/screen.h"
#include "strings/string_ids.h"

namespace nui {

namespace {

constexpr float kReadRadius = 4;  // `--radius` of the old .icon-btn: the hover tile and the focus ring

Colour Clear(Colour c) { return {c.rgb, 0.0f}; }

// The id of a per-action text ("window.keys.read", "window.keys.readRow", "window.keys.readDesc").
std::string KeyId(const std::string& action, const char* suffix = "") {
    return "window.keys." + action + suffix;
}

}  // namespace

// -------------------------------------------------------------------------------- KeyRow ---

KeyRow::KeyRow(std::string action, std::string label) : action_(std::move(action)) {
    SetRole(Role::ListItem);
    chip_ = Add<KeyChip>();
    SetLabel(std::move(label));
}

void KeyRow::SetLabel(std::string label) {
    label_ = std::move(label);
    SetName(label_);
    Invalidate();
}

void KeyRow::SetSelected(bool selected) {
    if (selected == selected_) return;
    selected_ = selected;
    Invalidate();
}

bool KeyRow::OnMouse(const MouseEvent& ev) {
    // A press on the name selects the row (the old `li` click). The press is used, so nothing else reacts.
    if (ev.kind == MouseKind::Down && ev.button == MouseButton::Left) {
        if (onSelect_) onSelect_();
        return true;
    }
    return ev.kind == MouseKind::Up && ev.button == MouseButton::Left;
}

void KeyRow::StateChanged() {
    Widget::StateChanged();
    const bool reached = hovered_ && !wasHovered_;  // the pointer came onto the row's own area
    wasHovered_ = hovered_;
    if (reached && onSelect_) onSelect_();
}

void KeyRow::Paint(Canvas& canvas, const Palette& p) {
    // The name ends `kKeyNameGap` before the chip; the chip is placed by the page (it depends on the key's text).
    const RectF r = Abs();
    const float chipX = chip_->Abs().x;
    TextStyle st;
    st.size = layout::kKeyNameFont;
    st.weight = selected_ ? FontWeight::Bold : FontWeight::Regular;
    st.align = TextAlign::Right;
    canvas.Text({r.x, r.y, std::max(0.0f, chipX - layout::kKeyNameGap - r.x), r.h}, label_, st, p.ink);
}

AccessInfo KeyRow::Access() const {
    AccessInfo a = BaseAccess();
    a.role = Role::ListItem;
    a.selected = selected_;
    a.selectable = true;
    return a;
}

bool KeyRow::AccSelect() {
    if (!onSelect_) return false;
    onSelect_();
    return true;
}

// ---------------------------------------------------------------------------- KeyReadButton ---

KeyReadButton::KeyReadButton() {
    SetRole(Role::Button);
    SetSize(layout::kKeyReadHit, layout::kKeyReadHit);
}

void KeyReadButton::Paint(Canvas& canvas, const Palette& p) {
    const RectF r = Abs();
    const float a = VisualAmount();
    // Idle: the icon alone, in accent, on the card (no tile, no ring). Hover: the `line` tile of the design's
    // icon buttons; pressed: an accent tile and an on-accent icon.
    const Colour tile = Ramp(Clear(p.line), p.line, p.accent, a);
    if (tile.alpha > 0.0f) canvas.FillRect(r, tile, kReadRadius);
    const float off = (r.w - layout::kKeyReadIcon) / 2;
    const RectF icon{r.x + off, r.y + (r.h - layout::kKeyReadIcon) / 2, layout::kKeyReadIcon, layout::kKeyReadIcon};
    const Colour glyph = Ramp(p.accent, p.accent, p.onAccent, a);

    // The wiggle: a short shake about the icon's centre every kWigglePeriodSeconds, counted from the first
    // paint (so the button is still when the page opens). None with reduced motion. Between wiggles nothing
    // ticks: the Screen is asked to wake up at the next start (NoteWakeAt), and only a wiggle that moves asks
    // for frames (NoteAnimationUntil).
    float angle = 0;
    Screen* screen = GetScreen();
    // No wiggle while the card is read: the button is "stop" then, and a shake would ask for a press on "play".
    if (screen && !screen->ReducedMotion() && !reading_) {
        const double now = screen->Now();
        if (start_ < 0) start_ = now;
        const double elapsed = now - start_;
        if (WiggleMoving(elapsed)) {
            angle = WiggleAngle(elapsed);
            screen->NoteAnimationUntil(start_ + WiggleEnd(elapsed));
        }
        screen->NoteWakeAt(start_ + WiggleNextStart(elapsed));
    }
    if (angle != 0.0f) {
        canvas.PushRotation({icon.x + icon.w / 2, icon.y + icon.h / 2}, angle);
        canvas.Icon(icon, IconName(), glyph);
        canvas.PopRotation();
    } else {
        canvas.Icon(icon, IconName(), glyph);
    }
    if (RingVisible()) DrawFocusRing(canvas, p, r, kReadRadius);
}

void KeyReadButton::SetReading(bool reading) {
    if (reading == reading_) return;
    reading_ = reading;
    // The wiggle schedule is counted from the first paint; the next wiggle after "stop" starts from the
    // schedule as it was, which is fine: it is still a quiet button between wiggles.
    Invalidate();
}

AccessInfo KeyReadButton::Access() const {
    AccessInfo a = BaseAccess();
    a.role = Role::Button;
    return a;
}

// ------------------------------------------------------------------------------------ KeyCard ---

KeyCard::KeyCard() {
    SetRole(Role::Group);
    SetBounds(layout::kKeyCard);
}

void KeyCard::Paint(Canvas& canvas, const Palette& p) { canvas.FillRect(Abs(), p.card); }

AccessInfo KeyCard::Access() const {
    AccessInfo a = BaseAccess();
    a.role = Role::Group;
    return a;
}

// ------------------------------------------------------------------------------ SettingsPage ---

SettingsPage::SettingsPage(const PageEnv& env) : PageView(Page::Settings, env) {
    SetBounds({0, 0, layout::kWindowW, layout::kWindowH});
    SetName(T(strings::id::kWindowNavSettings));

    // K9: no headings. Tab order is the old page's order: the key chips, the switches, then the card's play button.
    // The rows come with the first state (their number is the state's); they live in their own panel so a
    // later change of the list keeps them before the switches in the tab order.
    keysHost_ = Add<Panel>();
    keysHost_->SetBounds({0, layout::kKeysY, layout::kKeysW, layout::kKeyRowH});

    // A switch never flips by itself: it sends the opposite of what the last state says (HomePage's rule).
    // The order is the owner's: read-along first, then the start-up ones, then the look and the mouse.
    readAlong_ = AddSwitch("readAlong", strings::id::kWindowSettingsReadAlong);
    readAlong_->SetOnActivate([this] {
        if (!haveState_) return;
        PageMessage m = MakeMessage(PageMessageType::SetReadAlong, "setReadAlong");
        m.on = !last_.readAlong;
        Emit(m);
    });
    startup_ = AddSwitch("runAtStartup", strings::id::kWindowSettingsRunAtStartup);
    startup_->SetOnActivate([this] {
        if (!haveState_) return;
        PageMessage m = MakeMessage(PageMessageType::SetRunAtStartup, "setRunAtStartup");
        m.on = !last_.runAtStartup;
        Emit(m);
    });
    minimised_ = AddSwitch("startMinimised", strings::id::kWindowSettingsStartMinimised);
    minimised_->SetEnabled(false);  // usable only while Run at startup is on (D9)
    minimised_->SetOnActivate([this] {
        if (!haveState_ || !last_.runAtStartup) return;
        PageMessage m = MakeMessage(PageMessageType::SetStartMinimised, "setStartMinimised");
        m.on = !last_.startMinimised;
        Emit(m);
    });
    admin_ = AddSwitch("runAsAdmin", strings::id::kWindowSettingsRunAsAdmin);
    admin_->SetOnActivate([this] {
        if (!haveState_) return;
        PageMessage m = MakeMessage(PageMessageType::SetRunAsAdmin, "setRunAsAdmin");
        m.on = !last_.runAsAdmin;
        Emit(m);
    });
    dark_ = AddSwitch("darkMode", strings::id::kWindowSettingsDarkMode);
    dark_->SetOnActivate([this] {
        if (!haveState_) return;
        // The switch shows the theme in use (`dark`), not the stored choice ("system" may be either): it
        // sends the other of the two.
        PageMessage m = MakeMessage(PageMessageType::SetTheme, "setTheme");
        m.theme = last_.dark ? "light" : "dark";
        Emit(m);
    });
    left_ = AddSwitch("leftHanded", strings::id::kWindowSettingsLeftHanded);
    left_->SetOnActivate([this] {
        if (!haveState_) return;
        PageMessage m = MakeMessage(PageMessageType::SetLeftHanded, "setLeftHanded");
        m.on = !last_.leftHanded;
        Emit(m);
    });

    // The card: the engraving first (the text lies over it), the head, the text.
    card_ = Add<KeyCard>();
    PictureWidget* wine = card_->Add<PictureWidget>("wine", PictureTint::Wine);
    wine->SetBounds(layout::kKeyCardWine);
    cardTitle_ = card_->Add<Label>();
    cardTitle_->SetStyle(layout::kKeyCardTitleFont, FontWeight::Bold);
    cardTitle_->SetAccessHidden(true);  // the card is a group of the same name
    read_ = card_->Add<KeyReadButton>();
    read_->SetAutomationId("settings.card.play");
    card_->SetAutomationId("settings.card");
    read_->SetOnActivate([this] {
        if (selected_.empty()) return;
        PageMessage m = MakeMessage(PageMessageType::ReadKeyCard, "readKeyCard");
        m.action = selected_;  // the host knows the words: the page sends only which card (a Key or a switch)
        Emit(m);
    });
    cardBody_ = card_->Add<Paragraph>();
    cardBody_->SetLineHeight(layout::kBodyLine);
    cardBody_->SetParagraphGap(layout::kKeyCardParaGap);

    hint_ = Add<Label>();
    hint_->SetStyle(layout::kKeyHintFont);
    hint_->SetBounds(layout::kKeyHint);
    hint_->SetLiveRegion(true);  // aria-live="polite": "Press the new key. Esc cancels." is announced
}

// The index-th switch of the list: its row, its automation id ("settings.<cardId>"), and its card (a hover, a
// focus or a press selects it, as for a Key). The card id is also the switch's name in ui_host.h.
SwitchRow* SettingsPage::AddSwitch(const std::string& cardId, const char* labelId) {
    SwitchRow* s = Add<SwitchRow>(T(labelId));
    const float index = static_cast<float>(switches_.size());
    s->SetBounds({0, layout::kMiscY + index * layout::kMiscRowH, layout::kMiscW, layout::kMiscRowH});
    s->SetAutomationId("settings." + cardId);
    s->SetOnReach([this, cardId] { Select(cardId); });
    switches_.push_back({cardId, s});
    return s;
}

KeyRow* SettingsPage::RowFor(const std::string& action) const {
    for (KeyRow* r : rows_) {
        if (r->Action() == action) return r;
    }
    return nullptr;
}

SwitchRow* SettingsPage::SwitchFor(const std::string& cardId) const {
    for (const auto& s : switches_) {
        if (s.first == cardId) return s.second;
    }
    return nullptr;
}

// Does the page have a card of this id: a Key row or a switch?
bool SettingsPage::HasCard(const std::string& cardId) const { return RowFor(cardId) || SwitchFor(cardId); }

// Marks the selected card's row (bold) and no other, Keys and switches alike.
void SettingsPage::ShowSelection() {
    for (KeyRow* row : rows_) row->SetSelected(row->Action() == selected_);
    for (const auto& s : switches_) s.second->SetSelected(s.first == selected_);
}

// Makes the rows again when the state's list of Keys is not the one shown (the first state; later the same).
void SettingsPage::BuildRows(const UiState& state) {
    std::vector<UiKey> keys;
    for (const UiKey& k : state.keys) {
        if (k.action != "language") keys.push_back(k);  // no Language key in 0.5 (ADR-0007)
    }
    bool same = keys.size() == keys_.size();
    for (size_t i = 0; same && i < keys.size(); ++i) same = keys[i].action == keys_[i].action;
    keys_ = std::move(keys);
    if (same && (!rows_.empty() || keys_.empty())) return;

    // Rows are kept while the list stays the same (so the focus stays where it is when a state arrives).
    keysHost_->ClearChildren();
    rows_.clear();
    float y = 0;
    for (const UiKey& k : keys_) {
        const std::string action = k.action;
        KeyRow* row = keysHost_->Add<KeyRow>(action, T(KeyId(action, "Row").c_str()));
        row->SetBounds({0, y, layout::kKeysW, layout::kKeyRowH});
        y += layout::kKeyRowH + layout::kKeyRowGap;
        row->SetOnSelect([this, action] { Select(action); });
        row->SetAutomationId("settings.row." + action);
        KeyChip* chip = row->Chip();
        chip->SetAutomationId("settings.key." + action);
        chip->SetOnActivate([this, action] { StartCapture(action); });
        chip->SetOnReach([this, action] { Select(action); });
        chip->SetOnBlur([this, action] {
            if (capturing_ == action) EndCapture(true);  // the focus left: a press elsewhere, Tab, the window
        });
        chip->SetOnKey([this, action](const KeyEvent& ev) { OnChipKey(action, ev); });
        rows_.push_back(row);
    }
    keysHost_->SetBounds({0, layout::kKeysY, layout::kKeysW, std::max(layout::kKeyRowH, y - layout::kKeyRowGap)});
}

void SettingsPage::SetState(const UiState& state) {
    haveState_ = true;
    last_ = state;
    BuildRows(state);
    // A Key that is gone ends its wait; a selection that points nowhere goes to the first row.
    if (!capturing_.empty() && !RowFor(capturing_)) capturing_.clear();
    if (!HasCard(selected_)) selected_ = rows_.empty() ? std::string() : rows_.front()->Action();
    for (KeyRow* row : rows_) {
        row->SetLabel(T(KeyId(row->Action(), "Row").c_str()));
        ShowChip(*row);
    }
    ShowSelection();
    ShowCard();

    readAlong_->SetChecked(state.readAlong);
    startup_->SetChecked(state.runAtStartup);
    // Start minimised works only while Run at startup is on; off it is drawn lighter and does nothing.
    minimised_->SetChecked(state.startMinimised);
    minimised_->SetEnabled(state.runAtStartup);
    admin_->SetChecked(state.runAsAdmin);
    dark_->SetChecked(state.dark);
    left_->SetChecked(state.leftHanded);
    MarkLayout();
}

void SettingsPage::OnShow(const ShowOptions& options) {
    // The picture tool and the tests may select a Key (read | pause | speech) or a switch (runAtStartup ...).
    if (HasCard(options.selectAction)) {
        selected_ = options.selectAction;
        ShowSelection();
        ShowCard();
    }
    if (RowFor(options.capture)) StartCapture(options.capture);
}

// What the row's chip shows: the Key, "none", or the wait. The accessible name says both the action and the key.
void SettingsPage::ShowChip(KeyRow& row) {
    std::string key;
    for (const UiKey& k : keys_) {
        if (k.action == row.Action()) key = k.key;
    }
    const bool none = key.empty();
    const bool waiting = capturing_ == row.Action();
    const std::string shown = none ? T(strings::id::kKeyNone) : key;
    KeyChip* chip = row.Chip();
    chip->SetText(waiting ? T(strings::id::kWindowKeysCapturing) : shown);
    chip->SetState(waiting ? KeyChip::State::Capturing : none ? KeyChip::State::None : KeyChip::State::Set);
    chip->SetName(F(strings::id::kWindowKeysChipLabel,
                    {{"action", T(KeyId(row.Action()).c_str())}, {"key", shown}}));
    MarkLayout();  // the chip's width follows its text
}

// The card of the selected Key or switch: its name, the play button, and what it does (one paragraph per line).
// The ids come from ui_host.h (CardTitleId / CardDescId), the same ones the host reads aloud from.
void SettingsPage::ShowCard() {
    const std::string full = selected_.empty() ? std::string() : T(CardTitleId(selected_).c_str());
    cardTitle_->SetText(full);
    // K11: the host says which card it reads (UiState::readingCard); the button shows "stop" only on that card.
    // Another selected card shows "read aloud" again, and a press on it starts that card (the host replaces the
    // reading). The name changes with the icon, so a screen reader hears "Stop reading ..." (UIA Name event).
    const bool isSwitch = IsSwitchCard(selected_);
    const bool reading = haveState_ && !selected_.empty() && last_.readingCard == selected_;
    read_->SetReading(reading);
    const char* labelId = reading ? (isSwitch ? strings::id::kWindowSwitchesStopReading : strings::id::kWindowKeysStopReading)
                                  : (isSwitch ? strings::id::kWindowSwitchesReadAloud : strings::id::kWindowKeysReadAloud);
    const std::string readAloud = F(labelId, {{"action", full}});
    read_->SetName(readAloud);
    read_->SetTooltip(readAloud);
    cardBody_->SetText(selected_.empty() ? std::string() : T(CardDescId(selected_).c_str()));
    hint_->SetText(capturing_.empty() ? std::string() : T(strings::id::kWindowKeysCaptureHint));
    card_->SetName(full);
    MarkLayout();
}

void SettingsPage::Select(const std::string& cardId) {
    // The card stays with the key that waits for a key; the same row again changes nothing.
    if (!capturing_.empty() || selected_ == cardId || !HasCard(cardId)) return;
    selected_ = cardId;
    ShowSelection();
    ShowCard();
}

void SettingsPage::StartCapture(const std::string& action) {
    KeyRow* row = RowFor(action);
    if (!row || capturing_ == action) return;
    if (!capturing_.empty()) EndCapture(true);  // one wait at a time: the other chip's ends first
    capturing_ = action;
    selected_ = action;
    ShowSelection();
    for (KeyRow* r : rows_) ShowChip(*r);
    ShowCard();
    row->Chip()->Focus();  // the keys come to the chip (a press already focused it; Enter / Space did too)
    Emit([&] {
        PageMessage m = MakeMessage(PageMessageType::CaptureKey, "captureKey");
        m.action = action;
        return m;
    }());
}

void SettingsPage::EndCapture(bool sendCancel) {
    const std::string action = capturing_;
    if (action.empty()) return;
    capturing_.clear();
    if (KeyRow* row = RowFor(action)) ShowChip(*row);  // the Key of the last state again
    ShowCard();                                        // the hint goes
    if (sendCancel) {
        PageMessage m = MakeMessage(PageMessageType::CancelKeyCapture, "cancelKeyCapture");
        m.action = action;
        Emit(m);
    }
}

// A key came to the chip that waits. The chip hands over every key, Tab and Esc too.
void SettingsPage::OnChipKey(const std::string& action, const KeyEvent& ev) {
    if (capturing_ != action) return;
    if (ev.key == Key::Tab) {
        // The user is moving on: the wait ends, the Key stays as it was, and the Tab goes on as a Tab (the chip
        // no longer captures, so the Screen moves the focus). Reaching the chip again does not select it twice.
        EndCapture(true);
        if (Screen* s = GetScreen()) s->HandleKey(ev);
        return;
    }
    if (ev.key == Key::Escape) {
        EndCapture(true);  // the chip keeps the focus
        return;
    }
    if (IsModifierKey(ev.code)) return;  // wait for the main key
    if (ev.repeat) return;               // a held key is not a second answer
    const std::string chord = KeyChordName(ev.code, ev.ctrl, ev.alt, ev.shift, ev.win);
    if (chord.empty()) return;  // not a key Lexia can use: keep waiting
    capturing_.clear();
    // The chip shows the Key of the state until the host answers with a new one (a refused Key puts the old
    // one back). The chip keeps the focus.
    if (KeyRow* row = RowFor(action)) ShowChip(*row);
    ShowCard();
    PageMessage m = MakeMessage(PageMessageType::SetKey, "setKey");
    m.action = action;
    m.key = chord;
    Emit(m);
}

// Places what depends on text widths: the chips (right-aligned on the list's edge) and the card's head and text.
void SettingsPage::Relayout(Canvas& canvas) {
    for (KeyRow* row : rows_) {
        KeyChip* chip = row->Chip();
        const SizeF s = chip->Measure(canvas, layout::kKeysW);
        chip->SetBounds({layout::kKeysW - s.w, 0, s.w, layout::kKeyRowH});
    }
    // The head: the read button first, then the title. The button's hit area (28) is wider than its icon (24),
    // so it starts 2 before the padding line and the icon itself sits on it; the title follows the icon by 10.
    const float hitPad = (layout::kKeyReadHit - layout::kKeyReadIcon) / 2;
    read_->SetBounds({layout::kKeyCardPadX - hitPad,
                      layout::kKeyCardPadTop + (layout::kKeyCardHeadH - layout::kKeyReadHit) / 2,
                      layout::kKeyReadHit, layout::kKeyReadHit});
    const float titleW = cardTitle_->Measure(canvas, 0).w;
    cardTitle_->SetBounds({layout::kKeyCardPadX + layout::kKeyReadIcon + layout::kKeyReadGap, layout::kKeyCardPadTop,
                           titleW, layout::kKeyCardHeadH});
    const float bodyW = layout::kKeyCard.w - 2 * layout::kKeyCardPadX;
    const float bodyY = layout::kKeyCardPadTop + layout::kKeyCardHeadH + layout::kKeyCardTextGap;
    cardBody_->SetBounds({layout::kKeyCardPadX, bodyY, bodyW, cardBody_->Measure(canvas, bodyW).h});
}

}  // namespace nui
