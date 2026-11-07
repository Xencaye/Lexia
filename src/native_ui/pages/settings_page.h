#pragma once
// The Settings page (K.4, ADR-0009), ported from ui/index.html (#page-settings), ui/js/settings.js and
// ui/css/pages.css. Left: the heading "keybinds" and one row per Key (its name and a KeyChip), the heading
// "misc" and five switches. Right: the key card of the selected Key (wine engraving, title, a play button that
// reads the card aloud, what the Key does) and, under it, the hint that shows while a chip waits for a key.
//
// What the page owns (the state message does not carry it, as before): which row is selected (a hover, a focus
// or a press selects it, the card follows; while a chip waits for a key the card stays on that key) and which
// chip waits for a key. Everything else is drawn from the last UiState, and a user action is one PageMessage:
//   a press on a chip        CaptureKey { action }      the host lets go of Lexia's keys
//   a key (with modifiers)   SetKey { action, key }     the host checks and keeps it or not, new state follows
//   Esc, Tab, a press outside or the focus gone while waiting
//                            CancelKeyCapture { action }
//   the play button          ReadKeyCard { action }
//   the switches             SetRunAtStartup / SetStartMinimised / SetRunAsAdmin / SetTheme / SetLeftHanded
// The capture reads the key chip's OnKey: a chip that waits takes every key, Tab and Esc too (Screen), and the
// window gives it the keys DefWindowProc would eat (settings_window.cpp).
#include <string>
#include <vector>

#include "native_ui/controls.h"
#include "native_ui/display.h"
#include "native_ui/pages/page.h"

namespace nui {

// One line of the key list: the Key's name, right-aligned, bold while selected, and its KeyChip at the right
// end. The row itself is the target of a press or a hover on the name (the old `li`), so the pointer anywhere
// on the row selects it.
class KeyRow : public Widget {
public:
    KeyRow(std::string action, std::string label);
    const std::string& Action() const { return action_; }
    KeyChip* Chip() const { return chip_; }
    void SetLabel(std::string label);
    void SetSelected(bool selected);
    bool Selected() const { return selected_; }
    void SetOnSelect(std::function<void()> fn) { onSelect_ = std::move(fn); }
    bool OnMouse(const MouseEvent& ev) override;
    void Paint(Canvas& canvas, const Palette& palette) override;
    AccessInfo Access() const override;

protected:
    void StateChanged() override;

private:
    std::string action_, label_;
    KeyChip* chip_ = nullptr;
    bool selected_ = false;
    bool wasHovered_ = false;
    std::function<void()> onSelect_;
};

// The key card's play button: 24 x 24, a 2 DIP accent outline, radius 4, the play icon (14) in accent. Hover:
// `line` fill; pressed: accent fill and on-accent icon.
class KeyPlayButton : public Clickable {
public:
    KeyPlayButton();
    void Paint(Canvas& canvas, const Palette& palette) override;
    AccessInfo Access() const override;
};

// The card: the card colour behind its children (the engraving, the title, the play button, the text).
class KeyCard : public Widget {
public:
    KeyCard();
    void Paint(Canvas& canvas, const Palette& palette) override;
    AccessInfo Access() const override;
};

class SettingsPage : public PageView {
public:
    explicit SettingsPage(const PageEnv& env);
    void SetState(const UiState& state) override;
    void OnShow(const ShowOptions& options) override;

    // For the picture tool and the tests.
    const std::vector<KeyRow*>& Rows() const { return rows_; }
    KeyRow* RowFor(const std::string& action) const;
    const std::string& SelectedAction() const { return selected_; }
    const std::string& CapturingAction() const { return capturing_; }
    Label* CardTitle() const { return cardTitle_; }
    Paragraph* CardBody() const { return cardBody_; }
    KeyPlayButton* PlayButton() const { return play_; }
    Label* Hint() const { return hint_; }
    SwitchRow* RunAtStartup() const { return startup_; }
    SwitchRow* StartMinimised() const { return minimised_; }
    SwitchRow* RunAsAdmin() const { return admin_; }
    SwitchRow* DarkMode() const { return dark_; }
    SwitchRow* LeftHanded() const { return left_; }

protected:
    void Relayout(Canvas& canvas) override;

private:
    void BuildRows(const UiState& state);
    void Select(const std::string& action);
    void StartCapture(const std::string& action);
    // Ends the wait. `sendCancel`: tell the host (every end but a heard key). Then the chip shows its key again.
    void EndCapture(bool sendCancel);
    void OnChipKey(const std::string& action, const KeyEvent& ev);
    void ShowChip(KeyRow& row);
    void ShowCard();
    SwitchRow* AddSwitch(const char* labelId, float index);  // the index-th row of the "misc" list

    std::vector<KeyRow*> rows_;
    std::vector<UiKey> keys_;  // the last state's keys (the language action left out)
    std::string selected_, capturing_;
    bool haveState_ = false;
    UiState last_;  // the last state: what the switches and chips show, and what a switch sends the opposite of

    Label* keysHeading_ = nullptr;
    Panel* keysHost_ = nullptr;  // holds the KeyRows (made with the first state)
    Label* miscHeading_ = nullptr;
    KeyCard* card_ = nullptr;
    Label* cardTitle_ = nullptr;
    KeyPlayButton* play_ = nullptr;
    Paragraph* cardBody_ = nullptr;
    Label* hint_ = nullptr;
    SwitchRow* startup_ = nullptr;
    SwitchRow* minimised_ = nullptr;
    SwitchRow* admin_ = nullptr;
    SwitchRow* dark_ = nullptr;
    SwitchRow* left_ = nullptr;
};

}  // namespace nui
