#pragma once
// Where everything of the Settings window sits (K.2, ADR-0009). Every number is in DIPs of the 800 x 600
// client area and comes from the HTML pages of Lexia 0.5 (ui/css/frame.css and pages.css, deleted in K.6; they are in git
// history before it), measured by the owner on the wireframes at 96 dpi. The HTML reference shots in
// wireframe/html-reference/ are the visual spec. Pure and Win32-free. K.3 (Voices) and K.4 (Settings) add
// their own blocks to this file, so the positions of the pages stay in one place.
#include "native_ui/canvas.h"

namespace nui::layout {

constexpr float kWindowW = 800, kWindowH = 600;

// ---- the frame (frame.css) -------------------------------------------------------------------------
// Top links: centred on the window, vertical centre y = 36, 32 apart; word 24, icon 32, 7 between.
constexpr float kTopNavCentreY = 36, kTopNavGap = 32, kTopNavFont = 24, kTopNavIcon = 32, kTopNavIconGap = 7;
// Bottom-left links: left 30, vertical centre y = 569, 30 apart; word 16, icon 24, 4 between.
constexpr float kFootNavX = 30, kFootNavCentreY = 569, kFootNavGap = 30, kFootNavFont = 16, kFootNavIcon = 24,
                kFootNavIconGap = 4;
// Dotted rules (1 DIP, the `line` colour).
constexpr RectF kRuleTop{190, 74, 420, 1};
constexpr RectF kRuleBottom{30, 538, 280, 1};
// Master volume: speaker icon (accent, 20) at x = 548, track 576..676, centre y = 569.
constexpr float kVolumeIconX = 548, kVolumeIconSize = 20, kVolumeSliderX = 576, kVolumeSliderW = 100,
                kVolumeBoxH = 24, kVolumeBoxTop = 557;
// K10: the window's own minimise and close buttons (there is no Windows title bar), top right of every page.
// Measured on the owner's mockup (Nov 7, 800 x 600): the 24 x 24 icon boxes are at (726, 10) and (760, 10)
// (the X's strokes span x 765..779, y 15..29; the dash runs x 729..746 at y = 21). The hit area is 2 DIPs
// larger on each side (28 x 28, as an IconButton); the two hit areas are 6 apart, so they never overlap.
constexpr float kWindowButtonsY = 10, kMinimiseX = 726, kCloseX = 760, kWindowButtonIcon = 24,
                kWindowButtonHitPad = 2;
// The flute, on every page, bottom right.
constexpr RectF kFlute{692, 444, 100, 150};

// ---- Home (pages.css) --------------------------------------------------------------------------------
constexpr RectF kBee{266, 170, 266, 230};
constexpr float kBeeOpacity = 1.0f;  // K13: opaque, the palette tint is the whole look (the mockups of Nov 8)
constexpr RectF kPower{338, 218, 124, 124};
// The power glyph is the old SVG (viewBox 64, stroke 4.4) at 104 DIPs inside the 124 circle.
constexpr float kPowerGlyphSize = 104, kPowerGlyphViewBox = 64, kPowerGlyphStroke = 4.4f;
constexpr RectF kEnable{331, 421, 138, 40};
constexpr float kEnableFont = 22;

// ---- Settings (pages.css `.keys` `.misc` `.keycard` `.key-hint`, components.css) -----------------------
// K9 (the owner's new page, measured on the mockup at 96 dpi): no headings. The key list: right edge at 253
// (the chips), first row at y = 137, 24 high, 17 between, name 20, 16 before the chip.
constexpr float kKeysW = 253, kKeysY = 137, kKeyRowH = 24, kKeyRowGap = 17, kKeyNameFont = 20, kKeyNameGap = 16;
// The six switches: 250 wide, first at y = 335, 28 apart (the last one ends at 503, above the bottom rule).
constexpr float kMiscW = 250, kMiscY = 335, kMiscRowH = 28;
// The card (of a Key or of a switch): 355 x 393 at (273, 115), padding 18 20 20. The head is 28 high: the read
// button first (the Master volume's speaker icon, 24, in accent, no ring, in a 28 x 28 hit area centred on the
// icon so the icon sits on the 20 padding line), then the title (20 bold) 10 after the icon. The text is 22
// under the head, 20 line, paragraphs 14 apart; the wine 210 wide at (72, 254) inside (25 above the card's bottom edge).
constexpr RectF kKeyCard{273, 115, 355, 393};
constexpr float kKeyCardPadX = 20, kKeyCardPadTop = 18, kKeyCardHeadH = 28, kKeyCardTitleFont = 20,
                kKeyReadHit = 28, kKeyReadIcon = 24, kKeyReadGap = 10, kKeyCardTextGap = 22,
                kKeyCardParaGap = 14;
constexpr RectF kKeyCardWine{72, 254, 210, 114};  // relative to the card; 4 higher than a bottom-anchored guess: matched to the mockup by pixel
// The capture hint under the card: 13 px at (273, 514), 6 under the card.
constexpr RectF kKeyHint{273, 514, 355, 20};
constexpr float kKeyHintFont = 13;

// ---- Report, Help (pages.css) --------------------------------------------------------------------------
constexpr RectF kDog{-13, 124, 406, 394};
constexpr float kDogOpacity = 1.0f;  // K13: opaque, the palette tint is the whole look (the mockups of Nov 8)
constexpr float kBigX = 273, kBigY = 155, kBigFont = 24, kBigLine = 30;  // the Report title (two lines)
constexpr float kBigLetterSpacing = 0.01f * kBigFont;  // `letter-spacing: 0.01em` of `.big` (Report and Help titles)
constexpr float kReportTextX = 273, kReportTextY = 231, kReportTextW = 480, kBodyLine = 20;
constexpr float kReportLinksX = 274, kReportLinksY = 343, kReportLinksH = 26, kReportLinksGap = 29,
                kReportLinkFont = 18, kReportLinkIcon = 26, kReportLinkIconGap = 4;
constexpr float kHelpTitleY = 242, kHelpTextY = 288;  // centred across the window

// ---- About (pages.css) ----------------------------------------------------------------------------------
constexpr RectF kWolf{17, 192, 286, 314};
constexpr float kWolfOpacity = 1.0f;  // K13: opaque, the palette tint is the whole look (the mockups of Nov 8)
constexpr float kAboutHeadX = 273, kAboutHeadY = 105, kAboutHeadFont = 24, kAboutHeadLine = 30;
constexpr float kAboutTopX = 273, kAboutTopY = 152, kAboutTopW = 300, kAboutTopGap = 11;
// The text that goes round the wolf: 14 px, line height 17.5, 330 wide, paragraphs 17 apart. The indent of
// each line is the edge of the picture (the old CSS shape-outside, measured on the owner's mockup).
constexpr float kAboutMoreX = 283, kAboutMoreY = 275, kAboutMoreW = 330, kAboutMoreFont = 14, kAboutMoreLine = 17.5f,
                kAboutMoreGap = 17;
constexpr float kAboutIndents[] = {27, 23, 15, 11, 7, 0};
constexpr float kAboutLinksX = 284, kAboutLinksY = 450, kAboutLinksGap = 12;
constexpr float kAboutLegalX = 283, kAboutLegalY = 494, kAboutLegalFont = 10, kAboutLegalLine = 13;

// ---- Voices (pages.css `.vrow` and the rows below) -------------------------------------------------------
// A row is 30 tall in a grid of 252 | 254 | auto with 21 between: the label ends at x = 252 (right aligned), the
// control is at x = 273 (254 wide), the icon buttons start at x = 548, 6 apart.
constexpr float kVoiceRowH = 30, kVoiceLabelRight = 252, kVoiceControlX = 273, kVoiceControlW = 254,
                kVoiceSideX = 548, kVoiceSideGap = 6;
constexpr float kVoiceRowTop = 135, kSpeedRowTop = 225, kPitchRowTop = 275, kVolumeRowTop = 325;
constexpr float kVoiceLabelFont = 20, kVoiceLabelIcon = 24, kVoiceLabelGap = 8;
// The note that takes the place of the short rule when no Voice is installed (13 px, 254 wide).
constexpr RectF kVoiceNone{273, 170, 254, 36};
constexpr float kVoiceNoneFont = 13, kVoiceNoneLine = 13 * 1.35f;
constexpr RectF kVoiceRule{340, 197, 120, 1};
constexpr RectF kOtter{318, 378, 162, 76};
constexpr float kOtterOpacity = 1.0f;  // K13: opaque, the palette tint is the whole look (the mockups of Nov 8)
// The preset links: left 274, 24 tall from y = 482, 18 apart; word 17, icon 24.
constexpr float kPresetsX = 274, kPresetsY = 482, kPresetsH = 24, kPresetsGap = 18, kPresetsFont = 17,
                kPresetsIcon = 24;

}  // namespace nui::layout
