// The contrast of the palette's colour pairs (K.1a): the 16 pairs that scripts/ui_contrast.ps1 checked for the HTML pages (deleted in K.6),
// for the light and the dark set, with nui::ContrastRatio (WCAG 2). A pair of the code's own colours
// must pass its minimum (text 4.5:1, edges and states 3:1). A pair that is the owner's design
// ("design colour" below) is not changed to pass; its result today is recorded exactly, so a
// change of a colour is noticed here and reported to the owner.
#include <doctest/doctest.h>

#include "native_ui/palette.h"

namespace {

// Which colour of the palette a pair names.
enum class Role { Ink, Paper, Card, Line, Accent, OnAccent, Danger, PowerOffHover, PowerOnHover, Tile };

uint32_t RgbOf(const nui::Palette& p, Role r) {
    switch (r) {
    case Role::Ink: return p.ink.rgb;
    case Role::Paper: return p.paper.rgb;
    case Role::Card: return p.card.rgb;
    case Role::Line: return p.line.rgb;
    case Role::Accent: return p.accent.rgb;
    case Role::OnAccent: return p.onAccent.rgb;
    case Role::Danger: return p.danger.rgb;
    case Role::PowerOffHover: return p.powerOffHover.rgb;
    case Role::PowerOnHover: return p.powerOnHover.rgb;
    case Role::Tile: return p.tile.rgb;
    }
    return 0;
}

struct Pair {
    const char* what;
    Role fg, bg;
    double min;
    bool design;       // the owner's design colour: reported, not changed to pass
    bool passesLight;  // for a design pair: what the script reports today
    bool passesDark;
};

// The pairs and the minimums are the old script's. The colours are in src/native_ui/palette.cpp.
const Pair kPairs[] = {
    {"ink on paper (text)", Role::Ink, Role::Paper, 4.5, false, true, true},
    {"ink on card (the key card, the select)", Role::Ink, Role::Card, 4.5, false, true, true},
    {"on-accent on accent (key chip, enable button, switch knob)", Role::OnAccent, Role::Accent, 4.5, false, true, true},
    {"accent on line (the disable button, the quiet button)", Role::Accent, Role::Line, 4.5, false, true, true},
    {"danger on paper (error text)", Role::Danger, Role::Paper, 4.5, false, true, true},
    {"danger on card (error text in a sheet)", Role::Danger, Role::Card, 4.5, false, true, true},
    {"accent on paper (link icons, slider thumb, switch outline, focus ring)", Role::Accent, Role::Paper, 3.0, false, true, true},
    {"accent on card (focus ring, play button in the card)", Role::Accent, Role::Card, 3.0, false, true, true},
    {"accent on line (an icon button on hover)", Role::Accent, Role::Line, 3.0, false, true, true},
    {"on-accent on accent (an icon button pressed; the On power glyph)", Role::OnAccent, Role::Accent, 3.0, false, true, true},
    {"accent on power-off-hover (the power glyph, Off, hovered)", Role::Accent, Role::PowerOffHover, 3.0, false, true, true},
    {"on-accent on power-on-hover (the power glyph, On, hovered)", Role::OnAccent, Role::PowerOnHover, 3.0, false, true, true},
    // The four design pairs. Light: line/paper, line/card and tile/paper fail; accent/tile passes.
    // Dark: line/paper, line/card and accent/tile fail; tile/paper passes (script run of K.1a).
    {"line on paper (an idle icon button; the slider track)", Role::Line, Role::Paper, 3.0, true, false, false},
    {"line on card (the select border)", Role::Line, Role::Card, 3.0, true, false, false},
    {"accent on tile (an icon on a Control bar tile)", Role::Accent, Role::Tile, 3.0, true, true, false},
    {"tile on paper (the edge of a Control bar tile)", Role::Tile, Role::Paper, 3.0, true, false, true},
};

void CheckTheme(bool dark) {
    const nui::Palette& pal = nui::PaletteFor(dark);
    for (const Pair& p : kPairs) {
        CAPTURE(p.what);
        const double ratio = nui::ContrastRatio(RgbOf(pal, p.fg), RgbOf(pal, p.bg));
        CAPTURE(ratio);
        if (!p.design) {
            CHECK(ratio >= p.min);
        } else if (dark ? p.passesDark : p.passesLight) {
            CHECK(ratio >= p.min);
        } else {
            // The owner's design colour, reported here: it is below the minimum
            // and stays so until the owner changes the colour. If this fails, a colour changed.
            CHECK_FALSE(ratio >= p.min);
        }
    }
}

}  // namespace

TEST_CASE("palette: the light set's pairs") { CheckTheme(false); }
TEST_CASE("palette: the dark set's pairs") { CheckTheme(true); }

TEST_CASE("palette: ContrastRatio is WCAG 2") {
    CHECK(nui::ContrastRatio(0x000000, 0xFFFFFF) == doctest::Approx(21.0));
    CHECK(nui::ContrastRatio(0xFFFFFF, 0xFFFFFF) == doctest::Approx(1.0));
    // Symmetric: the lighter colour is always the numerator.
    CHECK(nui::ContrastRatio(0x6D6154, 0xF5EFE8) == doctest::Approx(nui::ContrastRatio(0xF5EFE8, 0x6D6154)));
    // The values the script prints for ink on paper, light.
    CHECK(nui::ContrastRatio(0x6D6154, 0xF5EFE8) == doctest::Approx(5.27).epsilon(0.005));
}
