# Training fonts for the English recogniser (task L)

The fonts that the synthetic training lines of `rec_en.onnx` are drawn in. The fonts and the lines are **not** in git:
`training/rec_en/fetch_fonts.py` downloads each font from the pinned URL and checks its SHA-256. This list is the
record of what the model was trained on.

Rules (plan, task L0):

- Allowed: SIL OFL 1.1, CC0, public domain, CC BY (credit given in `licenses/components/`), MIT, Apache-2.0,
  BSD-style, WTFPL, the Ubuntu Font Licence.
- Not allowed: a non-commercial (NC) or no-derivatives (ND) clause, "personal use only", "no AI / machine learning",
  no stated licence, a font taken out of a game's files.
- A font site's label is not enough. Before use, the agent reads the licence file in the download, fills in the URL,
  version and SHA-256, and sets **Status** to `checked`. `to check` = the licence below is what the author's page or
  a listing says; it is not yet read from the download.
- The model weights do not contain the font files; the fonts are only used to draw images. A font whose terms
  forbid that is left out.

## Pixel and bitmap fonts

| Font | Author | Licence | Source | Status | Why |
| --- | --- | --- | --- | --- | --- |
| Pixel Operator (and Pixel Operator 8, Mono, HB, SC) | Jayvee Enaguas | CC0 1.0 (from version 2018.10.04-1; earlier "8-bit Operator") | dafont.com/pixel-operator.font | to check | Closest free match to Undertale's menu font |
| monogram (and monogram extended) | datagoblin | CC0 1.0 | datagoblin.itch.io/monogram | to check | Small monospace game font |
| m5x7 | Daniel Linssen | CC0 1.0 (itch.io metadata; the page also says "attribution appreciated") | managore.itch.io/m5x7 | to check | Very common in indie games |
| m6x11 | Daniel Linssen | "free to use with attribution" (no named licence) | managore.itch.io/m6x11 | to check: use only if the download names a licence | Common in indie games |
| Departure Mono | Helena Zhang | SIL OFL 1.1 | github.com/rektdeckard/departure-mono | to check | Modern pixel monospace |
| Press Start 2P | CodeMan38 | SIL OFL 1.1 | fonts.google.com | to check | 8 px arcade capitals |
| Silkscreen | Jason Kottke | SIL OFL 1.1 | fonts.google.com | to check | Tiny 5 px font |
| VT323 | Peter Hull | SIL OFL 1.1 | fonts.google.com | to check | Terminal look |
| Pixelify Sans | Stefie Justprince | SIL OFL 1.1 | fonts.google.com | to check | Proportional pixel font |
| DotGothic16 | Fontworks | SIL OFL 1.1 | fonts.google.com | to check | JRPG-style dot font |
| Tiny5 | Stefan Schmidt | SIL OFL 1.1 | fonts.google.com | to check | Tiny proportional |
| Jersey 10, 15, 20, 25 | Sarah Cadigan-Fried | SIL OFL 1.1 | fonts.google.com | to check | Bold pixel text |
| Micro 5 | Sarah Cadigan-Fried | SIL OFL 1.1 | fonts.google.com | to check | Very small text |
| Bytesized | Baltdev | SIL OFL 1.1 | fonts.google.com | to check | Tiny pixel |
| Sixtyfour | Jens Kutilek | SIL OFL 1.1 | fonts.google.com | to check | Home-computer look |
| Handjet | Rosetta | SIL OFL 1.1 | fonts.google.com | to check | Dot-matrix, many weights |
| Workbench | Jens Kutilek | SIL OFL 1.1 | fonts.google.com | to check | Scan-line pixel |
| Pixeloid Sans, Mono | GGBotNet | SIL OFL 1.1 | ggbot.itch.io/pixeloid-font | to check | Clean 9 px game font |
| Kenney Pixel, Mini, Mini Square, High, Blocks | Kenney | CC0 1.0 | kenney.nl/assets/kenney-fonts | to check | Free game-asset fonts |
| Fusion Pixel 12 px (Latin) | TakWolf | SIL OFL 1.1 | github.com/TakWolf/fusion-pixel-font | to check | 8, 10, 12 px |
| Ark Pixel (Latin) | TakWolf | SIL OFL 1.1 | github.com/TakWolf/ark-pixel-font | to check | 10, 12, 16 px |
| Cozette | slavfox | MIT | github.com/slavfox/Cozette | to check | Small bitmap |
| Spleen | Frederic Cambus | BSD-2-Clause | github.com/fcambus/spleen | to check | 5x8 to 32x64 bitmap |
| Terminus | Dimitar Zhekov | SIL OFL 1.1 | terminus-font.sourceforge.net | to check | Bitmap terminal font |
| GNU Unifont | Roman Czyborra and others | SIL OFL 1.1 (or GPL-2.0+ with the font exception) | unifoundry.com | to check | 8x16 bitmap |
| Gohufont | Hugo Chargois | WTFPL | font.gohu.org | to check | 11 / 14 px bitmap |
| Pixel Code | Qwerasd | SIL OFL 1.1 | github.com/qwerasd205/PixelCode | to check | Pixel monospace |

**Left out:** Undertale's own fonts (game assets); "8bitoperator JVE" (listings disagree: CC BY-NC-SA 3.0 or public
domain; use only if its download shows an allowed licence); "Determination Mono" / "Determination Sans" (fan fonts,
listed as CC BY-NC-ND; some versions "All Rights Reserved"); the Ultimate Oldschool PC Font Pack (CC BY-SA 4.0: the
share-alike terms on a trained model are unclear).

## Normal fonts (so that other text does not get worse)

All from Google Fonts or the author's own repository; all SIL OFL 1.1 unless the row says otherwise.

| Font | Licence | Why |
| --- | --- | --- |
| Noto Sans, Noto Serif, Noto Sans Mono | OFL | Wide coverage, common in apps |
| Liberation Sans, Serif, Mono | OFL | Metric match of Arial, Times, Courier |
| DejaVu Sans, Serif, Sans Mono | Bitstream Vera licence (permissive) + public domain changes | Common on Linux and in games |
| Atkinson Hyperlegible Next | OFL | Lexia's own UI font |
| Source Sans 3, Source Serif 4, Source Code Pro | OFL | Adobe's open families |
| Roboto, Roboto Mono, Roboto Slab | OFL (Apache-2.0 for older versions) | Android and the web |
| Open Sans | OFL | The web |
| Lato | OFL | The web |
| Inter | OFL | Modern UI |
| Fira Sans, Fira Mono | OFL | UI and code |
| IBM Plex Sans, Serif, Mono | OFL | UI |
| Ubuntu | Ubuntu Font Licence 1.0 | Ubuntu desktop |
| Merriweather | OFL | Serif on screens |
| Montserrat | OFL | Game menus and titles |
| Oswald | OFL | Condensed titles |
| Comic Neue | OFL | Comic and casual game text |
| Bangers | OFL | Comic capitals |
| Exo 2, Orbitron | OFL | Sci-fi game UI |
| Cinzel | OFL | Fantasy game titles |
| Nunito | OFL | Rounded UI |

## Sources used for this list

- Pixel Operator licence and history: https://www.dafont.com/pixel-operator.font
- monogram: https://datagoblin.itch.io/monogram
- m5x7 / m6x11: https://managore.itch.io/m5x7, https://managore.itch.io/m6x11
- Departure Mono: https://departuremono.com/
- 8bitoperator JVE (conflicting listings): https://online-fonts.com/fonts/8bitoperator-jve
- Determination Mono: https://www.fontspace.com/determination-mono-web-font-f23209
- The Google Fonts rows: each family's page on https://fonts.google.com (the licence is on the family's "License"
  tab and in its `OFL.txt`).
