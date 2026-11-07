#include "native_ui/text_layout.h"

#include <algorithm>

namespace nui {

namespace {

// A word: the pieces that are glued together (no space between them), and whether a space came before.
struct Word {
    std::vector<TextRun> parts;
    bool spaceBefore = false;
};

bool IsSpace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

TextStyle StyleFor(const TextStyle& base, bool bold) {
    TextStyle s = base;
    s.weight = bold ? FontWeight::Bold : base.weight;
    s.align = TextAlign::Left;
    return s;
}

std::vector<Word> SplitWords(const std::vector<TextRun>& runs) {
    std::vector<Word> words;
    bool pendingSpace = false;  // whitespace seen since the last word part
    bool open = false;          // the last word can still take parts (no space since)
    for (const TextRun& run : runs) {
        size_t i = 0;
        while (i < run.text.size()) {
            if (IsSpace(run.text[i])) {
                pendingSpace = true;
                open = false;
                ++i;
                continue;
            }
            size_t j = i;
            while (j < run.text.size() && !IsSpace(run.text[j])) ++j;
            TextRun part{run.text.substr(i, j - i), run.bold};
            if (open && !words.empty()) {
                words.back().parts.push_back(std::move(part));
            } else {
                Word w;
                w.spaceBefore = pendingSpace && !words.empty();
                w.parts.push_back(std::move(part));
                words.push_back(std::move(w));
            }
            pendingSpace = false;
            open = true;
            i = j;
        }
    }
    return words;
}

}  // namespace

std::vector<TextRun> ParseTemplate(std::string_view tmpl, const std::map<std::string, std::string>& bold) {
    std::vector<TextRun> runs;
    std::string plain;
    auto flush = [&] {
        if (!plain.empty()) {
            runs.push_back({plain, false});
            plain.clear();
        }
    };
    size_t i = 0;
    while (i < tmpl.size()) {
        if (tmpl[i] == '{') {
            const size_t close = tmpl.find('}', i + 1);
            if (close != std::string_view::npos) {
                const std::string name(tmpl.substr(i + 1, close - i - 1));
                const auto it = bold.find(name);
                if (it != bold.end()) {
                    flush();
                    runs.push_back({it->second, true});
                    i = close + 1;
                    continue;
                }
            }
        }
        plain += tmpl[i];
        ++i;
    }
    flush();
    return runs;
}

float SpaceWidth(Canvas& canvas, const TextStyle& style) {
    return canvas.MeasureText("x x", style).w - canvas.MeasureText("xx", style).w;
}

float LineHeight(Canvas& canvas, const TextStyle& style) { return canvas.MeasureText("Ag", style).h; }

std::vector<WrappedLine> WrapRuns(Canvas& canvas, const std::vector<TextRun>& runs, const TextStyle& base,
                                  float maxWidth, const std::vector<float>& indents) {
    std::vector<WrappedLine> lines;
    const std::vector<Word> words = SplitWords(runs);
    if (words.empty()) return lines;

    const float space = SpaceWidth(canvas, base);
    auto indentOf = [&](size_t line) { return line < indents.size() ? indents[line] : 0.0f; };

    WrappedLine current;
    current.indent = indentOf(0);
    float x = current.indent;
    bool lineEmpty = true;

    for (const Word& w : words) {
        // The word's parts, measured.
        std::vector<LinePiece> pieces;
        float wordWidth = 0;
        for (const TextRun& part : w.parts) {
            LinePiece p;
            p.text = part.text;
            p.bold = part.bold;
            p.width = canvas.MeasureText(part.text, StyleFor(base, part.bold)).w;
            wordWidth += p.width;
            pieces.push_back(std::move(p));
        }
        const float gap = (!lineEmpty && w.spaceBefore) ? space : 0.0f;
        // The line's room runs from its indent to maxWidth: the indent narrows the line from the left,
        // the right edge stays.
        const bool fits = x + gap + wordWidth <= maxWidth + 0.001f;
        if (!lineEmpty && !fits) {
            current.width = x;
            lines.push_back(std::move(current));
            current = WrappedLine{};
            current.indent = indentOf(lines.size());
            x = current.indent;
            lineEmpty = true;
        }
        const float useGap = (!lineEmpty && w.spaceBefore) ? space : 0.0f;
        x += useGap;
        for (LinePiece& p : pieces) {
            p.x = x;
            x += p.width;
            current.pieces.push_back(std::move(p));
        }
        lineEmpty = false;
    }
    current.width = x;
    lines.push_back(std::move(current));
    return lines;
}

}  // namespace nui
