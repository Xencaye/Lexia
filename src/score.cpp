#include "score.h"

#include <algorithm>
#include <cstdlib>

namespace {

bool IsSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v'; }

// Trims, and turns every run of white space into one space.
std::string CollapseWhitespace(const std::string& s) {
    std::string out;
    bool pendingSpace = false;
    for (char c : s) {
        if (IsSpace(c)) {
            pendingSpace = !out.empty();
            continue;
        }
        if (pendingSpace) out += ' ';
        pendingSpace = false;
        out += c;
    }
    return out;
}

// Normalised Blocks joined with a blank line, with the empty ones dropped.
std::string JoinBlocks(const std::vector<std::string>& blocks, int* count) {
    std::string joined;
    *count = 0;
    for (const std::string& b : blocks) {
        const std::string n = CollapseWhitespace(b);
        if (n.empty()) continue;
        if (*count > 0) joined += "\n\n";
        joined += n;
        ++*count;
    }
    return joined;
}

// UTF-8 to code points. Lenient: a byte that does not start a well-formed sequence counts as one
// code point of its own, so bad input still gets a score.
std::vector<char32_t> DecodeUtf8(const std::string& s) {
    std::vector<char32_t> out;
    out.reserve(s.size());
    const size_t n = s.size();
    for (size_t i = 0; i < n;) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        size_t len = 1;
        char32_t cp = c;
        if (c >= 0xC0 && c < 0xE0) { len = 2; cp = c & 0x1Fu; }
        else if (c >= 0xE0 && c < 0xF0) { len = 3; cp = c & 0x0Fu; }
        else if (c >= 0xF0 && c < 0xF8) { len = 4; cp = c & 0x07u; }
        bool ok = len > 1 && i + len <= n;
        for (size_t k = 1; ok && k < len; ++k) {
            const unsigned char cc = static_cast<unsigned char>(s[i + k]);
            if ((cc & 0xC0) != 0x80) ok = false;
            else cp = (cp << 6) | (cc & 0x3Fu);
        }
        if (!ok) { len = 1; cp = c; }
        out.push_back(cp);
        i += len;
    }
    return out;
}

// Lower case for Latin and Cyrillic: a Fixture is what a person expects to hear, and case is not
// heard. shortcut: other scripts keep their case; add them with their Language.
std::vector<char32_t> FoldCase(std::vector<char32_t> s) {
    for (char32_t& c : s) {
        if (c >= U'A' && c <= U'Z') c += 0x20;
        else if (c >= 0x0410 && c <= 0x042F) c += 0x20;  // А..Я
        else if (c >= 0x0400 && c <= 0x040F) c += 0x50;  // Ѐ..Џ (Ё)
    }
    return s;
}

// Levenshtein distance (insert, delete, substitute; each 1) in two rows of memory.
size_t EditDistance(const std::vector<char32_t>& a, const std::vector<char32_t>& b) {
    const std::vector<char32_t>& shorter = a.size() <= b.size() ? a : b;
    const std::vector<char32_t>& longer = a.size() <= b.size() ? b : a;
    std::vector<size_t> prev(shorter.size() + 1), cur(shorter.size() + 1);
    for (size_t j = 0; j <= shorter.size(); ++j) prev[j] = j;
    for (size_t i = 1; i <= longer.size(); ++i) {
        cur[0] = i;
        for (size_t j = 1; j <= shorter.size(); ++j) {
            const size_t sub = prev[j - 1] + (longer[i - 1] == shorter[j - 1] ? 0 : 1);
            cur[j] = std::min({sub, prev[j] + 1, cur[j - 1] + 1});
        }
        std::swap(prev, cur);
    }
    return prev[shorter.size()];
}

}  // namespace

std::vector<std::string> ParseExpectedBlocks(const std::string& utf8) {
    std::string text = utf8;
    if (text.compare(0, 3, "\xEF\xBB\xBF") == 0) text.erase(0, 3);

    std::vector<std::string> blocks;
    std::string current;  // the Block's lines so far, joined with spaces
    auto flush = [&] {
        if (!current.empty()) blocks.push_back(std::move(current));
        current.clear();
    };
    size_t pos = 0;
    while (pos <= text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos) end = text.size();
        const std::string line = CollapseWhitespace(text.substr(pos, end - pos));
        if (line.empty()) {
            flush();
        } else {
            if (!current.empty()) current += ' ';
            current += line;
        }
        pos = end + 1;
    }
    flush();
    return blocks;
}

std::vector<std::string> BlockTexts(const std::vector<TextBlock>& blocks) {
    std::vector<std::string> out;
    out.reserve(blocks.size());
    for (const TextBlock& b : blocks) out.push_back(b.text);
    return out;
}

LayoutScore ScoreLayout(const std::vector<std::string>& expected,
                        const std::vector<std::string>& produced) {
    LayoutScore s;
    const std::vector<char32_t> e = FoldCase(DecodeUtf8(JoinBlocks(expected, &s.expectedBlocks)));
    const std::vector<char32_t> p = FoldCase(DecodeUtf8(JoinBlocks(produced, &s.producedBlocks)));
    s.blockCountError = std::abs(s.producedBlocks - s.expectedBlocks);
    s.editDistance = EditDistance(e, p);
    const size_t longest = std::max(e.size(), p.size());
    s.distance = longest == 0 ? 0.0 : static_cast<double>(s.editDistance) / static_cast<double>(longest);
    return s;
}
