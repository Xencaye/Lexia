#pragma once
// The D8 "paper" palette: the one source of the colours of the Settings window and the Control bar
// (K1, ADR-0009). The light set began as the HTML pages' tokens.css (deleted in K.6); both sets are the owner's
// final mockups of Nov 8 (K13, the files of wireframe/: light <page>.png, dark <page>_dark.png). Pure, Win32-free,
// in lexia_core; the contrast of the pairs is checked by tests/palette_test.cpp (text 4.5:1, edges and states 3:1;
// a pair of the owner's design that is below that is recorded there, not changed).
#include <cstdint>

namespace nui {

// A colour as 0xRRGGBB plus an alpha (1 = opaque). Only the scrim is not opaque.
struct Colour {
    uint32_t rgb = 0;
    float alpha = 1.0f;
};

struct Palette {
    Colour paper;          // the window
    Colour card;           // the key card, the select, the sheets
    Colour ink;            // text
    Colour line;           // dotted rules, slider tracks, idle icon buttons, a quiet or disabled fill
    Colour accent;         // the primary button, key chips, slider thumbs, switches, link icons, focus ring
    Colour onAccent;       // text and glyphs on an accent fill
    Colour danger;         // error text
    Colour powerOffHover;  // the power circle, Off, hovered
    Colour powerOnHover;   // the power circle, On, hovered
    Colour tile;           // the Control bar's icon tiles
    Colour windowIcon;     // the window's minimise and close icons, idle (K10; hover and press are accent)
    Colour buttonText;     // the word on an accent button (enable, a sheet's main button)
    Colour quietText;      // the word on a quiet (line) fill: the disable button, a quiet button, an unset key chip
    Colour thumb;          // a slider's thumb (the Voices sliders, the Master volume)
    Colour tintFlute;      // the tints of the engravings (their lines)
    Colour tintBeeOff;
    Colour tintBeeOn;
    Colour tintOtter;
    Colour tintWine;
    Colour tintDog;        // the dog and the wolf
    Colour scrim;          // behind a sheet
};

// The palette of the light or the dark theme.
const Palette& PaletteFor(bool dark);

// WCAG 2 contrast ratio of two opaque colours (1..21).
double ContrastRatio(uint32_t rgbA, uint32_t rgbB);

}  // namespace nui
