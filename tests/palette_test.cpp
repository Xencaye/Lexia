// The contrast of the palette's colour pairs (K.1a; K13 for the owner's final palette): the pairs that scripts/ui_contrast.ps1 checked for the HTML pages (deleted in K.6) and the K13 roles,
// for the light and the dark set, with nui::ContrastRatio (WCAG 2). A pair of the code's own colours
// must pass its minimum (text 4.5:1, edges and states 3:1). A pair that is the owner's design
// ("design colour" below) is not changed to pass; its result today is recorded exactly, so a
// change of a colour is noticed here and reported to the owner.
#include <doctest/doctest.h>

#include "native_ui/palette.h"

namespace {

// Which colour of the palette a pair names.
enum class Role { Ink, Paper, Card, Line, Accent, OnAccent, Danger, PowerOffHover, PowerOnHover, Tile, WindowIcon, ButtonText, QuietText, Thumb };

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
    case Role::WindowIcon: return p.windowIcon.rgb;
    case Role::ButtonText: return p.buttonText.rgb;
    case Role::QuietText: return p.quietText.rgb;
    case Role::Thumb: return p.thumb.rgb;
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
    {"danger on paper (error text)", Role::Danger, Role::Paper, 4.5, false, true, true},
    {"danger on card (error text in a sheet)", Role::Danger, Role::Card, 4.5, false, true, true},
    {"accent on paper (link icons, switch outline, focus ring)", Role::Accent, Role::Paper, 3.0, false, true, true},
    {"accent on card (focus ring, play button in the card)", Role::Accent, Role::Card, 3.0, false, true, true},
    {"thumb on paper (a slider's thumb)", Role::Thumb, Role::Paper, 3.0, false, true, true},
    {"on-accent on accent (an icon button pressed; the On power glyph)", Role::OnAccent, Role::Accent, 3.0, false, true, true},
    {"accent on power-off-hover (the power glyph, Off, hovered)", Role::Accent, Role::PowerOffHover, 3.0, false, true, true},
    {"on-accent on power-on-hover (the power glyph, On, hovered)", Role::OnAccent, Role::PowerOnHover, 3.0, false, true, true},
    // The owner's design pairs (the K13 mockups of Nov 8). Each records whether it passes today in light and
    // in dark; a pair that is below its minimum is reported to the owner, not changed here.
    // Light fails: line/paper 1.35, line/card 1.43, tile/paper 1.61, window icon/paper 1.35.
    // Dark fails: on-accent/accent text 4.17, button text/accent 2.83, quiet text/line 4.40, accent/line 2.41,
    // line/paper 1.73, line/card 1.45, accent/tile 2.41, tile/paper 1.73.
    {"on-accent on accent (key chip, switch knob)", Role::OnAccent, Role::Accent, 4.5, true, true, false},
    {"button text on accent (the enable button, a sheet's main button)", Role::ButtonText, Role::Accent, 4.5, true, true, false},
    {"quiet text on line (the disable button, a quiet button, an unset key chip)", Role::QuietText, Role::Line, 4.5, true, true, false},
    {"accent on line (an icon button on hover)", Role::Accent, Role::Line, 3.0, true, true, false},
    {"line on paper (an idle icon button; the slider track)", Role::Line, Role::Paper, 3.0, true, false, false},
    {"line on card (the select border)", Role::Line, Role::Card, 3.0, true, false, false},
    {"accent on tile (an icon on a Control bar tile)", Role::Accent, Role::Tile, 3.0, true, true, false},
    {"tile on paper (the edge of a Control bar tile)", Role::Tile, Role::Paper, 3.0, true, false, false},
    {"window icon on paper (minimise and close, idle)", Role::WindowIcon, Role::Paper, 3.0, true, false, true},
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
