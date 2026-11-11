# Training fonts for the English recogniser (task L)

The fonts that the synthetic training lines of `rec_en.onnx` are drawn in. The fonts and the lines are **not** in git:
`training/rec_en/fetch_sources.py` downloads each font from the URL in `training/rec_en/sources.json` and checks it
against the SHA-256 in `training/rec_en/sources.lock.json`. This list is the record of what the model was trained on.

Rules (plan, task L0):

- Allowed: SIL OFL 1.1, CC0, public domain, CC BY (credit given in `licenses/components/`), MIT, Apache-2.0,
  BSD-style, WTFPL, the Ubuntu Font Licence.
- Not allowed: a non-commercial (NC) or no-derivatives (ND) clause, "personal use only", "no AI / machine learning",
  no stated licence, a font taken out of a game's files.
- A font site's label is not enough: `fetch_sources.py` uses a font only if the licence file in its download (for
  Google Fonts the `OFL.txt` / `UFL.txt` beside it, at the pinned commit of `github.com/google/fonts`) contains the
  expected licence; otherwise the run stops. **checked** below = that check passed on 2026-11-10.
- The model weights do not contain the font files; the fonts are only used to draw images. A font whose terms
  forbid that is left out.

## Pixel and bitmap fonts (used)

| Font | Author | Licence | Source | Status |
| --- | --- | --- | --- | --- |
| Pixel Operator (Regular, Bold, 8, Mono, HB, SC: 15 files) | Jayvee Enaguas | CC0 1.0 | dafont.com/pixel-operator.font | checked (closest free match to Undertale's menu font) |
| Departure Mono 1.500 | Helena Zhang | SIL OFL 1.1 | github.com/rektdeckard/departure-mono | checked |
| Press Start 2P | CodeMan38 | SIL OFL 1.1 | Google Fonts | checked |
| Silkscreen (Regular, Bold) | Jason Kottke | SIL OFL 1.1 | Google Fonts | checked |
| VT323 | Peter Hull | SIL OFL 1.1 | Google Fonts | checked |
| Pixelify Sans | Stefie Justprince | SIL OFL 1.1 | Google Fonts | checked |
| DotGothic16 | Fontworks | SIL OFL 1.1 | Google Fonts | checked |
| Tiny5 | Stefan Schmidt | SIL OFL 1.1 | Google Fonts | checked |
| Jersey 10, 15, 20, 25 | Sarah Cadigan-Fried | SIL OFL 1.1 | Google Fonts | checked |
| Micro 5 | Sarah Cadigan-Fried | SIL OFL 1.1 | Google Fonts | checked |
| Bytesized | Baltdev | SIL OFL 1.1 | Google Fonts | checked |
| Handjet | Rosetta | SIL OFL 1.1 | Google Fonts | checked |
| Fusion Pixel 8, 10, 12 px (Latin) 2026.11.10 | TakWolf | SIL OFL 1.1 | github.com/TakWolf/fusion-pixel-font | checked |
| Ark Pixel 12 px (Latin) 2026.11.10 | TakWolf | SIL OFL 1.1 | github.com/TakWolf/ark-pixel-font | checked |
| Pixel Code 2.2 (Regular, Bold) | Qwerasd | SIL OFL 1.1 | github.com/qwerasd205/PixelCode | checked (the release zip has no licence file: the `LICENSE` of the same tag is fetched and pinned) |
| Spleen 2.2.0 (6x12 to 32x64) | Frederic Cambus | BSD-2-Clause | github.com/fcambus/spleen | checked |
| Doto | Óliver Lalan | SIL OFL 1.1 | Google Fonts | checked 2026-11-11 (round 2: dot-matrix) |
| Jacquard 12 | Sarah Cadigan-Fried | SIL OFL 1.1 | Google Fonts | checked 2026-11-11 (round 2: pixel blackletter of fantasy games) |
| Rubik Pixels | NaN, Luke Prowse | SIL OFL 1.1 | Google Fonts | checked 2026-11-11 (round 2) |
| Bitcount Single | Petr van Blokland | SIL OFL 1.1 | Google Fonts | checked 2026-11-11 (round 2) |

## Normal fonts (used, so that other text does not get worse)

All from Google Fonts at the pinned commit; all SIL OFL 1.1 except Ubuntu (Ubuntu Font Licence 1.0); all checked:
Noto Sans (and Italic), Noto Serif, Noto Sans Mono, Atkinson Hyperlegible Next, Source Sans 3 (and Italic),
Source Serif 4, Source Code Pro, Roboto (and Italic), Roboto Mono, Open Sans, Lato (Regular, Bold, Italic, Black),
Inter, Fira Sans (Regular, Bold, Italic, Light), Fira Mono (Regular, Bold), IBM Plex Sans, IBM Plex Serif (Regular,
Bold), IBM Plex Mono (Regular, Bold), Ubuntu (Regular, Bold, Italic, Medium), Merriweather, Montserrat, Oswald,
Comic Neue (Regular, Bold), Bangers, Exo 2, Orbitron, Cinzel, Nunito.

Added in round 2 (2026-11-11), for comic lettering, wide monospace UIs and game titles; all SIL OFL 1.1 except
Luckiest Guy and Permanent Marker (Apache-2.0, checked against the `LICENSE.txt` beside them); all checked:
Patrick Hand, Luckiest Guy, Permanent Marker, Kalam (Regular, Bold), Space Mono (Regular, Bold), Share Tech Mono,
Courier Prime (Regular, Bold), Cousine (Regular, Bold), Chakra Petch (Regular, Bold), Rajdhani (Regular, Bold), Teko,
Russo One, Audiowide, Bebas Neue, Anton, Lexend, PT Sans (Regular, Bold, Italic), Arimo (and Italic).

## Not used

| Font | Why |
| --- | --- |
| Undertale's own fonts | Game assets |
| 8bitoperator JVE | Listings disagree (CC BY-NC-SA 3.0, or public domain); use only if its own download shows an allowed licence |
| Determination Mono / Sans | Fan fonts, listed as CC BY-NC-ND; some versions "All Rights Reserved" |
| Ultimate Oldschool PC Font Pack | CC BY-SA 4.0: the share-alike terms on a trained model are unclear |
| Sixtyfour, Workbench (OFL) | Tried: their scan-line effect makes them unreadable when drawn small |
| monogram (CC0), m5x7 (CC0), m6x11 (attribution, no named licence), Pixeloid (OFL) | Only on itch.io, whose downloads a script cannot pin; can be added by hand later |
| Kenney Fonts (CC0), Cozette (MIT), Terminus (OFL), GNU Unifont (OFL), Gohufont (WTFPL) | No release file a script can pin as TTF / OTF (bitmap formats only, or a page that needs a browser) |

## Texts

Six public-domain books from Project Gutenberg (eBooks #11, #35, #84, #120, #1342, #1661), pinned by SHA-256;
`gen_lines.py` cuts off Project Gutenberg's header and licence footer.

## Sources used for this list

- Pixel Operator licence and history: https://www.dafont.com/pixel-operator.font
- monogram: https://datagoblin.itch.io/monogram
- m5x7 / m6x11: https://managore.itch.io/m5x7, https://managore.itch.io/m6x11
- Departure Mono: https://departuremono.com/
- 8bitoperator JVE (conflicting listings): https://online-fonts.com/fonts/8bitoperator-jve
- Determination Mono: https://www.fontspace.com/determination-mono-web-font-f23209
- Google Fonts: https://github.com/google/fonts (each family's `OFL.txt` / `UFL.txt`)
