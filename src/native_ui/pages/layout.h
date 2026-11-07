#pragma once
// Where everything of the Settings window sits (K.2, ADR-0009). Every number is in DIPs of the 800 x 600
// client area and comes from the old ui/css/frame.css and ui/css/pages.css (measured by the owner on the
// wireframes at 96 dpi); the CSS is the spec until ui/ is deleted (K.6), the HTML reference shots in
// wireframe/html-reference/ are the spec after. Pure and Win32-free. K.3 (Voices) and K.4 (Settings) add
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
// The flute, on every page, bottom right.
constexpr RectF kFlute{692, 444, 100, 150};

// ---- Home (pages.css) --------------------------------------------------------------------------------
constexpr RectF kBee{266, 170, 266, 230};
constexpr float kBeeOpacity = 0.9f;
constexpr RectF kPower{338, 218, 124, 124};
// The power glyph is the old SVG (viewBox 64, stroke 4.4) at 104 DIPs inside the 124 circle.
constexpr float kPowerGlyphSize = 104, kPowerGlyphViewBox = 64, kPowerGlyphStroke = 4.4f;
constexpr RectF kEnable{331, 421, 138, 40};
constexpr float kEnableFont = 22;

// ---- Settings (pages.css `.heading` `.keys` `.misc` `.keycard` `.key-hint`, components.css) -----------------
// Headings: right edge at 252, 24 bold, 28 high; keys at y = 120, misc at y = 335.
constexpr float kHeadingW = 252, kHeadingH = 28, kHeadingFont = 24, kHeadingKeysY = 120, kHeadingMiscY = 335;
// The key list: right edge at 253 (the chips), first row at y = 179, 24 high, 17 between, name 20, 16 before the chip.
constexpr float kKeysW = 253, kKeysY = 179, kKeyRowH = 24, kKeyRowGap = 17, kKeyNameFont = 20, kKeyNameGap = 16;
// The five switches: 250 wide, first at y = 377, 28 apart.
constexpr float kMiscW = 250, kMiscY = 377, kMiscRowH = 28;
// The key card: 355 x 342 at (273, 160), padding 18 20 20; title 20 bold in a 28 high head; the play button
// 24 x 24 (2 border, radius 4, icon 14) 12 after the title; the text 22 under the head, 20 line, paragraphs 14
// apart; the wine at (72, 207) inside, 210 wide.
constexpr RectF kKeyCard{273, 160, 355, 342};
constexpr float kKeyCardPadX = 20, kKeyCardPadTop = 18, kKeyCardHeadH = 28, kKeyCardTitleFont = 20,
                kKeyPlaySize = 24, kKeyPlayBorder = 2, kKeyPlayIcon = 14, kKeyPlayGap = 12, kKeyCardTextGap = 22,
                kKeyCardParaGap = 14;
constexpr RectF kKeyCardWine{72, 207, 210, 114};  // relative to the card
// The capture hint under the card: 13 px at (273, 508).
constexpr RectF kKeyHint{273, 508, 355, 20};
constexpr float kKeyHintFont = 13;

// ---- Report, Help (pages.css) --------------------------------------------------------------------------
constexpr RectF kDog{-13, 124, 406, 394};
constexpr float kDogOpacity = 0.66f;
constexpr float kBigX = 273, kBigY = 155, kBigFont = 24, kBigLine = 30;  // the Report title (two lines)
constexpr float kBigLetterSpacing = 0.01f * kBigFont;  // `letter-spacing: 0.01em` of `.big` (Report and Help titles)
constexpr float kReportTextX = 273, kReportTextY = 231, kReportTextW = 480, kBodyLine = 20;
constexpr float kReportLinksX = 274, kReportLinksY = 343, kReportLinksH = 26, kReportLinksGap = 29,
                kReportLinkFont = 18, kReportLinkIcon = 26, kReportLinkIconGap = 4;
constexpr float kHelpTitleY = 242, kHelpTextY = 288;  // centred across the window

// ---- About (pages.css) ----------------------------------------------------------------------------------
constexpr RectF kWolf{17, 192, 286, 314};
constexpr float kWolfOpacity = 0.72f;
constexpr float kAboutHeadX = 273, kAboutHeadY = 105, kAboutHeadFont = 24, kAboutHeadLine = 30;
constexpr float kAboutTopX = 273, kAboutTopY = 152, kAboutTopW = 300, kAboutTopGap = 11;
// The text that goes round the wolf: 14 px, line height 17.5, 330 wide, paragraphs 17 apart. The indent of
// each line is the edge of the picture (the old CSS shape-outside, measured on the owner's mockup).
constexpr float kAboutMoreX = 283, kAboutMoreY = 275, kAboutMoreW = 330, kAboutMoreFont = 14, kAboutMoreLine = 17.5f,
                kAboutMoreGap = 17;
constexpr float kAboutIndents[] = {27, 23, 15, 11, 7, 0};
constexpr float kAboutLinksX = 284, kAboutLinksY = 450, kAboutLinksGap = 12;
constexpr float kAboutLegalX = 283, kAboutLegalY = 494, kAboutLegalFont = 10, kAboutLegalLine = 13;

}  // namespace nui::layout
