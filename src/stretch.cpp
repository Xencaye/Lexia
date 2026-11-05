#include "stretch.h"

#include <cstddef>
#include <optional>

#include "codepoint.h"

namespace {

bool IsSpace(char32_t c) {
    return c == U' ' || (c >= 0x09 && c <= 0x0D) || c == 0xA0 || (c >= 0x2000 && c <= 0x200B) ||
           c == 0x202F || c == 0x205F || c == 0x3000;
}

// The Language Lexia speaks a script with: the first in the table that uses it.
const Language* LanguageOf(Script script) {
    for (const Language& l : AllLanguages()) {
        if (l.script == script) return &l;
    }
    return nullptr;
}

// A word with a script: where it is in the text and which script (an index into kScripts).
struct Word {
    size_t begin = 0;
    int script = 0;
};

constexpr Script kScripts[] = {Script::Latin, Script::Cyrillic};
constexpr int kScriptCount = 2;

// The index of a letter's script in kScripts, or -1 when it is no letter of a script a Language uses.
int ScriptIndex(char32_t c) {
    const std::optional<Script> script = ScriptOfLetter(c);
    for (int k = 0; script && k < kScriptCount; ++k) {
        if (kScripts[k] == *script) return k;
    }
    return -1;
}

// Consecutive words of one script.
struct Run {
    int script = 0;
    size_t begin = 0;  // where its first word starts
    int words = 0;
};

}  // namespace

std::vector<Stretch> SplitIntoStretches(const std::string& utf8) {
    if (utf8.empty()) return {};

    // 1. Words that have a script.
    const Language* languages[kScriptCount];
    for (int k = 0; k < kScriptCount; ++k) languages[k] = LanguageOf(kScripts[k]);

    std::vector<Word> words;
    for (size_t i = 0; i < utf8.size();) {
        size_t wordBegin = i;
        char32_t c = NextCodePoint(utf8, i);
        if (IsSpace(c)) continue;
        int letters[kScriptCount] = {0, 0};
        int firstScript = -1;
        for (;;) {
            const int k = ScriptIndex(c);
            if (k >= 0 && languages[k]) {
                ++letters[k];
                if (firstScript < 0) firstScript = k;
            }
            if (i >= utf8.size()) break;
            const size_t before = i;
            c = NextCodePoint(utf8, i);
            if (IsSpace(c)) {
                i = before;
                break;
            }
        }
        if (firstScript < 0) continue;  // no letters of a script a Language uses
        int best = firstScript;  // a tie goes to the first letter's script
        for (int k = 0; k < kScriptCount; ++k) {
            if (letters[k] > letters[best]) best = k;
        }
        words.push_back({wordBegin, best});
    }

    // 2. Runs of one script.
    std::vector<Run> runs;
    for (const Word& w : words) {
        if (!runs.empty() && runs.back().script == w.script) {
            ++runs.back().words;
        } else {
            runs.push_back({w.script, w.begin, 1});
        }
    }

    // 3. Runs of two words start stretches; one-word islands vanish into the stretch before them
    // (or, at the start, after them), because a stretch reaches from its first word to the next
    // stretch's first word.
    struct Cut {
        int script;
        size_t begin;
    };
    std::vector<Cut> cuts;
    for (const Run& r : runs) {
        if (r.words < 2) continue;
        if (!cuts.empty() && cuts.back().script == r.script) continue;  // joins the earlier one
        cuts.push_back({r.script, cuts.empty() ? 0 : r.begin});
    }

    std::vector<Stretch> stretches;
    if (cuts.empty()) {
        // No phrase anywhere: one stretch, in the script with the most words.
        int count[kScriptCount] = {0, 0};
        for (const Word& w : words) ++count[w.script];
        const Language* language = &GuessLanguage(utf8);
        int best = -1;
        bool tie = false;
        for (int k = 0; k < kScriptCount; ++k) {
            if (best < 0 || count[k] > count[best]) {
                best = k;
                tie = false;
            } else if (count[k] == count[best]) {
                tie = true;
            }
        }
        if (best >= 0 && count[best] > 0 && !tie) language = languages[best];
        stretches.push_back({utf8, language});
        return stretches;
    }
    for (size_t n = 0; n < cuts.size(); ++n) {
        const size_t end = n + 1 < cuts.size() ? cuts[n + 1].begin : utf8.size();
        stretches.push_back({utf8.substr(cuts[n].begin, end - cuts[n].begin), languages[cuts[n].script]});
    }
    return stretches;
}
