#include "svg_recolour.h"

#include <cstddef>
#include <cstdio>

// Recolours the icons of src/res/icons for the Control bar. The files are drawn in one fixed colour; the
// page recolours them with CSS, but in C++ the colour has to be changed in the SVG text before it is
// parsed. Only a hex colour after `stroke` or `fill` (an attribute stroke="#..." or a style
// stroke:#...) is changed; "none" and every other value stay as they are.
namespace {

bool IsNameChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
}

bool IsHex(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

bool IsSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

// If a property `name` (stroke or fill) starts at `pos`, followed by = or : and a hex colour, returns
// where the "#..." begins and ends; otherwise false.
bool FindColourAt(const std::string& s, size_t pos, const char* name, size_t& begin, size_t& end) {
    size_t i = pos;
    for (const char* n = name; *n; ++n, ++i) {
        if (i >= s.size() || s[i] != *n) return false;
    }
    if (pos > 0 && IsNameChar(s[pos - 1])) return false;  // the end of a longer name
    if (i < s.size() && IsNameChar(s[i])) return false;    // the start of a longer name: stroke-width
    while (i < s.size() && IsSpace(s[i])) ++i;
    if (i >= s.size() || (s[i] != '=' && s[i] != ':')) return false;
    ++i;
    while (i < s.size() && IsSpace(s[i])) ++i;
    if (i < s.size() && (s[i] == '"' || s[i] == '\'')) ++i;
    if (i >= s.size() || s[i] != '#') return false;
    begin = i;
    ++i;
    while (i < s.size() && IsHex(s[i])) ++i;
    if (i - begin < 4) return false;  // "#" and at least three digits
    end = i;
    return true;
}

}  // namespace

// rgb is 0xRRGGBB. One pass over the text: at each "s" or "f" that starts a stroke / fill colour,
// copy up to the "#", write the new colour, and jump past the old digits.
std::string RecolourSvg(std::string svg, uint32_t rgb) {
    char colour[8];  // "#RRGGBB" and the closing '\0'
    std::snprintf(colour, sizeof(colour), "#%06X", static_cast<unsigned>(rgb & 0xFFFFFFu));
    std::string out;
    out.reserve(svg.size());
    size_t i = 0;
    while (i < svg.size()) {
        size_t begin = 0, end = 0;
        const char c = svg[i];
        if ((c == 's' && FindColourAt(svg, i, "stroke", begin, end)) ||
            (c == 'f' && FindColourAt(svg, i, "fill", begin, end))) {
            out.append(svg, i, begin - i);  // the name, the = and the quote
            out.append(colour);
            i = end;
            continue;
        }
        out.push_back(c);
        ++i;
    }
    return out;
}
