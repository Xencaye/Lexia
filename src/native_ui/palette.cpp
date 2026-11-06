#include "native_ui/palette.h"

#include <algorithm>
#include <cmath>

namespace nui {

const Palette& PaletteFor(bool dark) {
    // Light: the owner's D8 design.
    static const Palette kLight{
        {0xF5EFE8}, {0xFBF6F0}, {0x6D6154}, {0xD9CFC4}, {0x6E504A}, {0xFBF6F0}, {0x8A3A30},
        {0xEEE6DE}, {0x856A65}, {0xBEA991},
        {0xBBAD9F}, {0xCBBDB0}, {0xA87A72}, {0xB3A49C}, {0xE3D3C7}, {0x74594F},
        {0x3C3028, 0.45f},
    };
    // TODO(owner): the final dark palette. These are placeholders of the same roles.
    static const Palette kDark{
        {0x2A2420}, {0x342D28}, {0xE8DED3}, {0x574C44}, {0xDABDB4}, {0x2A2420}, {0xF0A79B},
        {0x3A322C}, {0xE6CEC6}, {0xBEA991},
        {0x5C4F46}, {0x5C4F46}, {0xD6B8AF}, {0x7A6A60}, {0x463B34}, {0xB69A90},
        {0x000000, 0.55f},
    };
    return dark ? kDark : kLight;
}

namespace {

// Relative luminance of one sRGB channel (0..255), as WCAG 2 defines it.
double Channel(uint32_t c8) {
    const double c = c8 / 255.0;
    return c <= 0.03928 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}

double Luminance(uint32_t rgb) {
    return 0.2126 * Channel((rgb >> 16) & 0xFF) + 0.7152 * Channel((rgb >> 8) & 0xFF) +
           0.0722 * Channel(rgb & 0xFF);
}

}  // namespace

double ContrastRatio(uint32_t rgbA, uint32_t rgbB) {
    const double a = Luminance(rgbA);
    const double b = Luminance(rgbB);
    return (std::max(a, b) + 0.05) / (std::min(a, b) + 0.05);
}

}  // namespace nui
