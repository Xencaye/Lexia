#include "card_reading.h"

#include "common.h"  // Trim (common.h is Win32-free)

std::vector<std::string> CardReadingBlocks(const std::string& title, const std::string& description) {
    std::vector<std::string> blocks;
    // The title first, so the person hears which card it is before what it does (owner, Nov 11).
    const std::string head = Trim(title);
    if (!head.empty()) blocks.push_back(head);
    // Then the description, one Block per line: the string table writes a paragraph break as '\n'.
    size_t start = 0;
    while (start <= description.size()) {
        const size_t end = description.find('\n', start);
        const std::string line =
            Trim(description.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (!line.empty()) blocks.push_back(line);
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return blocks;
}

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
