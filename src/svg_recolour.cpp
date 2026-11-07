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

namespace {

constexpr float kMaxSvgSide = 8192.0f;  // a larger picture is not one of ours

// The value of the attribute `name` in the tag text `tag` (between its quotes), or empty. The name must stand
// alone: "width" does not match "stroke-width".
std::string_view AttributeValue(std::string_view tag, std::string_view name) {
    size_t pos = 0;
    while ((pos = tag.find(name, pos)) != std::string_view::npos) {
        const size_t after = pos + name.size();
        const bool alone = (pos == 0 || !IsNameChar(tag[pos - 1])) && (after >= tag.size() || !IsNameChar(tag[after]));
        size_t i = after;
        while (i < tag.size() && IsSpace(tag[i])) ++i;
        if (alone && i < tag.size() && tag[i] == '=') {
            ++i;
            while (i < tag.size() && IsSpace(tag[i])) ++i;
            if (i < tag.size() && (tag[i] == '"' || tag[i] == '\'')) {
                const char quote = tag[i];
                const size_t end = tag.find(quote, i + 1);
                if (end == std::string_view::npos) return {};
                return tag.substr(i + 1, end - i - 1);
            }
            return {};
        }
        pos = after;
    }
    return {};
}

// Reads one number (digits, one '.', no sign: a size is never negative) from `s` at `i`; false if none.
bool ReadNumber(std::string_view s, size_t& i, float& out) {
    while (i < s.size() && (IsSpace(s[i]) || s[i] == ',')) ++i;
    const size_t start = i;
    bool dot = false;
    double value = 0.0, scale = 1.0;
    for (; i < s.size(); ++i) {
        const char c = s[i];
        if (c >= '0' && c <= '9') {
            if (dot) {
                scale /= 10.0;
                value += (c - '0') * scale;
            } else {
                value = value * 10.0 + (c - '0');
            }
        } else if (c == '.' && !dot) {
            dot = true;
        } else {
            break;
        }
        if (value > kMaxSvgSide * 10.0) return false;  // no need to read a huge number to the end
    }
    if (i == start || (i == start + 1 && dot)) return false;
    out = static_cast<float>(value);
    return true;
}

bool UsableSide(float v) { return v > 0.0f && v <= kMaxSvgSide; }

// A width or height attribute: a number, then nothing or "px".
bool ReadLength(std::string_view value, float& out) {
    size_t i = 0;
    if (!ReadNumber(value, i, out)) return false;
    std::string_view rest = value.substr(i);
    while (!rest.empty() && IsSpace(rest.back())) rest.remove_suffix(1);
    return (rest.empty() || rest == "px") && UsableSide(out);
}

}  // namespace

bool SvgRootSize(std::string_view svg, float& width, float& height) {
    const size_t open = svg.find("<svg");
    if (open == std::string_view::npos) return false;
    const size_t close = svg.find('>', open);
    if (close == std::string_view::npos) return false;
    const std::string_view tag = svg.substr(open + 4, close - open - 4);
    float w = 0, h = 0;
    if (ReadLength(AttributeValue(tag, "width"), w) && ReadLength(AttributeValue(tag, "height"), h)) {
        width = w;
        height = h;
        return true;
    }
    // viewBox="minX minY width height": the two last numbers. A negative min (a '-') makes it unusable here,
    // which is fine: Lexia's own files start at 0 0.
    const std::string_view box = AttributeValue(tag, "viewBox");
    float v[4] = {};
    size_t i = 0;
    for (float& n : v) {
        if (!ReadNumber(box, i, n)) return false;
    }
    if (!UsableSide(v[2]) || !UsableSide(v[3])) return false;
    width = v[2];
    height = v[3];
    return true;
}
