#pragma once
// RecolourSvg: the colour of a line icon is fixed in its SVG file (#6E504A in src/res/icons). The Control
// bar draws each icon in the colour of its state, so it changes the text before it parses it.
// Pure, Win32-free.
#include <cstdint>
#include <string>
#include <string_view>

// `svg` with every stroke and fill colour that is written as a hex colour ("#6E504A", "#fff",
// "#RRGGBBAA"), in an attribute (stroke="#..." fill='#...') or in a style (style="stroke:#...;"),
// replaced by `rgb` (0xRRGGBB). `none`, `currentColor`, `url(...)` and named colours stay as they are,
// and so does everything else (stroke-width, ids, path data).
std::string RecolourSvg(std::string svg, uint32_t rgb);

// The size of the SVG's root element in its own units (K13: the engravings of src/res/img are SVG files with
// width="267" height="231"): the root `width` / `height` (plain numbers, "px" allowed), else the last two numbers
// of its `viewBox`. False when there is no root <svg> tag or no usable size (zero, negative, above 8192 or not a
// number); the canvas then does not draw the picture. Only the root tag is read, so a child's width never counts.
bool SvgRootSize(std::string_view svg, float& width, float& height);
