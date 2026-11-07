#pragma once
// The Settings page (K.4, ADR-0009), ported from old ui/index.html (#page-settings), old ui/js/settings.js and
// old ui/css/pages.css; K9 is the owner's new layout. Left: one row per Key (its name and a KeyChip), then six
// switches (read-along, run at startup, start minimized, run as admin, dark mode, left-handed); no headings.
// Right: the card of the selected row (wine engraving, a read button (the speaker icon) that reads the card
// aloud, the title, what the Key or switch does) and, under it, the hint that shows while a chip waits for a key.
//
// What the page owns (the state message does not carry it, as before): which row is selected (a hover, a focus
// or a press selects it (a switch row too, K9), the card follows; while a chip waits for a key the card stays on that key) and which
// chip waits for a key. Everything else is drawn from the last UiState, and a user action is one PageMessage:
//   a press on a chip        CaptureKey { action }      the host lets go of Lexia's keys
//   a key (with modifiers)   SetKey { action, key }     the host checks and keeps it or not, new state follows
//   Esc, Tab, a press outside or the focus gone while waiting
//                            CancelKeyCapture { action }
//   the read button          ReadKeyCard { action = the card's id: a Key, or a switch name from SwitchCardIds() }
//   the switches             SetRunAtStartup / SetStartMinimised / SetRunAsAdmin / SetTheme / SetLeftHanded / SetReadAlong
// The capture reads the key chip's OnKey: a chip that waits takes every key, Tab and Esc too (Screen), and the
// window gives it the keys DefWindowProc would eat (settings_window.cpp).
#include <string>
#include <utility>
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
    bool AccSelect() override;  // UI Automation SelectionItem.Select: the row's key card shows (K.5)

protected:
    void StateChanged() override;

private:
    std::string action_, label_;
    KeyChip* chip_ = nullptr;
    bool selected_ = false;
    bool wasHovered_ = false;
    std::function<void()> onSelect_;
};

// The key card's read button: the Master volume's speaker icon (24, accent) with no ring, in a 28 x 28 hit
// area, left of the card's title. Hover: the `line` tile of the icon buttons; pressed: accent tile and
// on-accent icon. Every kWigglePeriodSeconds it shakes a little about the icon's centre (animation.h
// `WiggleAngle`), counted from its first paint; not with reduced motion. It asks the Screen for a wake-up at
// the next wiggle instead of keeping a timer running (Screen::NoteWakeAt).
class KeyReadButton : public Clickable {
public:
    KeyReadButton();
    void Paint(Canvas& canvas, const Palette& palette) override;
    AccessInfo Access() const override;

private:
    double start_ = -1;  // Screen time of the first paint; negative = not painted yet
};

// The card: the card colour behind its children (the engraving, the read button, the title, the text).
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
    SwitchRow* SwitchFor(const std::string& cardId) const;  // a switch by its card id ("readAlong" ...)
    bool HasCard(const std::string& cardId) const;
    const std::string& SelectedAction() const { return selected_; }  // the selected card's id: a Key or a switch
    const std::string& CapturingAction() const { return capturing_; }
    Label* CardTitle() const { return cardTitle_; }
    Paragraph* CardBody() const { return cardBody_; }
    KeyReadButton* ReadButton() const { return read_; }
    Label* Hint() const { return hint_; }
    SwitchRow* RunAtStartup() const { return startup_; }
    SwitchRow* StartMinimised() const { return minimised_; }
    SwitchRow* RunAsAdmin() const { return admin_; }
    SwitchRow* DarkMode() const { return dark_; }
    SwitchRow* LeftHanded() const { return left_; }
    SwitchRow* ReadAlong() const { return readAlong_; }

protected:
    void Relayout(Canvas& canvas) override;

private:
    void BuildRows(const UiState& state);
    void Select(const std::string& cardId);
    void ShowSelection();  // bold on the selected row, Keys and switches
    void StartCapture(const std::string& action);
    // Ends the wait. `sendCancel`: tell the host (every end but a heard key). Then the chip shows its key again.
    void EndCapture(bool sendCancel);
    void OnChipKey(const std::string& action, const KeyEvent& ev);
    void ShowChip(KeyRow& row);
    void ShowCard();
    SwitchRow* AddSwitch(const std::string& cardId, const char* labelId);  // the next row of the switch list

    std::vector<KeyRow*> rows_;
    std::vector<std::pair<std::string, SwitchRow*>> switches_;  // card id and row, in SwitchCardIds() order
    std::vector<UiKey> keys_;  // the last state's keys (the language action left out)
    std::string selected_, capturing_;
    bool haveState_ = false;
    UiState last_;  // the last state: what the switches and chips show, and what a switch sends the opposite of

    Panel* keysHost_ = nullptr;  // holds the KeyRows (made with the first state)
    KeyCard* card_ = nullptr;
    Label* cardTitle_ = nullptr;
    KeyReadButton* read_ = nullptr;
    Paragraph* cardBody_ = nullptr;
    Label* hint_ = nullptr;
    SwitchRow* startup_ = nullptr;
    SwitchRow* minimised_ = nullptr;
    SwitchRow* admin_ = nullptr;
    SwitchRow* dark_ = nullptr;
    SwitchRow* left_ = nullptr;
    SwitchRow* readAlong_ = nullptr;
};

}  // namespace nui
