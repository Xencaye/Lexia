#pragma once
// Which Settings-page card, if any, Lexia is reading aloud now (K11): the state behind the card's read button,
// which shows a "stop" icon while its card is read and the speaker again when it is not. Pure (the Speaker
// is asked through a function), so the rules are tested without Win32 or audio.
//
// The rules (the owner's, Nov 7):
//   - a press on the button of a card that is being read stops that reading;
//   - a press on any other button (or on this one when its reading is over) starts the card's reading, and a
//     reading that was going is replaced (the Speaker's append = false);
//   - a reading that ends by itself, or is stopped from somewhere else (tray Off, Control bar Stop, the Speech
//     key), is noticed by Update() and the button turns back.
// App owns one and shows `Card()` in UiState::readingCard; it is not thread-safe (UI thread only).
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// The Blocks that the card's read button speaks, in order: the card's title first (as the card shows it), then
// each non-empty line of its description, trimmed. Each is one Block, so Skip moves one line and the title is
// heard on its own. An empty title is left out; empty strings give no Blocks (then nothing is queued).
std::vector<std::string> CardReadingBlocks(const std::string& title, const std::string& description);

class CardReading {
public:
    // Is the Reading with this id still queued, being spoken or paused? (Speaker::HasReading)
    using IsActive = std::function<bool(uint64_t readingId)>;

    enum class Press { Start, Stop };

    // What a press on `card`'s button must do. Const: nothing changes until Started() / Clear().
    Press OnPress(const std::string& card, const IsActive& isActive) const;

    // `card`'s Reading was queued with this id. Id 0 means the Speaker queued nothing (no text, or no
    // engine): then nothing is being read and the state is cleared.
    void Started(const std::string& card, uint64_t readingId);

    // Forgets the reading (the caller has stopped it, or the window it was shown in is gone).
    void Clear();

    // Called when the queue may have changed. If the reading this holds is no longer active, forgets it
    // and returns true (the caller shows the new state); else false.
    bool Update(const IsActive& isActive);

    bool Reading() const { return id_ != 0; }
    // The card being read, or "" when none (UiState::readingCard).
    const std::string& Card() const { return card_; }
    uint64_t Id() const { return id_; }

private:
    std::string card_;
    uint64_t id_ = 0;  // 0 = none; ids of the Reading queue start at 1
};
