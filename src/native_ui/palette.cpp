#include "native_ui/palette.h"

#include <algorithm>
#include <cmath>

namespace nui {

const Palette& PaletteFor(bool dark) {
    // Both sets are read from the owner's mockups of Nov 8 (K13). In the field order of Palette:
    // paper, card, ink, line, accent, onAccent, danger,
    // powerOffHover, powerOnHover, tile, windowIcon, buttonText, quietText, thumb,
    // the six engraving tints (flute, bee Off, bee On, otter, wine, dog and wolf), scrim.
    // The engravings are drawn opaque in their tint: in light the line colour (the bee in the accent while On),
    // the wine in the card as the paper colour; in dark all of them in the line colour, the wine as paper.
    static const Palette kLight{
        {0xF5EFE8}, {0xFBF6F0}, {0x6D6154}, {0xD9CFC4}, {0x6E504A}, {0xFBF6F0}, {0x8A3A30},
        {0xEEE6DE}, {0x897069}, {0xCBBDAB}, {0xD9CFC4}, {0xFBF6F0}, {0x6E504A}, {0x6E504A},
        {0xD9CFC4}, {0xD9CFC4}, {0x6E504A}, {0xD9CFC4}, {0xF5EFE8}, {0xD9CFC4},
        {0x3C3028, 0.45f},
    };
    // The dark set: neutral greys with a sea-green accent. The mockups show no error text, so `danger` is the
    // earlier dark value (it reads on both paper and card); the scrim is unchanged.
    static const Palette kDark{
        {0x262626}, {0x333333}, {0xB8B8B8}, {0x4B4B4B}, {0x708C87}, {0x262626}, {0xF0A79B},
        {0x2B2C2C}, {0x617874}, {0x4B4B4B}, {0xE3E3E3}, {0xE3E3E3}, {0xB8B8B8}, {0xB8B8B8},
        {0x4B4B4B}, {0x4B4B4B}, {0x4B4B4B}, {0x4B4B4B}, {0x262626}, {0x4B4B4B},
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
