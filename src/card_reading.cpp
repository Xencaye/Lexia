#include "card_reading.h"

CardReading::Press CardReading::OnPress(const std::string& card, const IsActive& isActive) const {
    // Stop only a reading that is really still going: if it ended a moment ago and the timer has not noticed
    // yet, the press means "read it again", not "stop" (nothing would be stopped and the person would hear
    // nothing).
    if (id_ != 0 && card == card_ && isActive && isActive(id_)) return Press::Stop;
    return Press::Start;
}

void CardReading::Started(const std::string& card, uint64_t readingId) {
    if (readingId == 0) {
        Clear();
        return;
    }
    card_ = card;
    id_ = readingId;
}

void CardReading::Clear() {
    card_.clear();
    id_ = 0;
}

bool CardReading::Update(const IsActive& isActive) {
    if (id_ == 0) return false;
    if (isActive && isActive(id_)) return false;
    Clear();
    return true;
}
