#pragma once
// RecolourSvg: the colour of a line icon is fixed in its SVG file (#6E504A in src/res/icons). The Control
// bar draws each icon in the colour of its state, so it changes the text before it parses it.
// Pure, Win32-free.
#include <cstdint>
#include <string>

// `svg` with every stroke and fill colour that is written as a hex colour ("#6E504A", "#fff",
// "#RRGGBBAA"), in an attribute (stroke="#..." fill='#...') or in a style (style="stroke:#...;"),
// replaced by `rgb` (0xRRGGBB). `none`, `currentColor`, `url(...)` and named colours stay as they are,
// and so does everything else (stroke-width, ids, path data).
std::string RecolourSvg(std::string svg, uint32_t rgb);
