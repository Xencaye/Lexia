#pragma once
// How the Overlay (and the read-along layer after it) look (G3): everything is drawn in Lexia's accent colour
// of the current light / dark mode, and the Pick and hover marks are see-through, so the screen under them
// stays readable. Pure and Win32-free (in lexia_core, tested by tests/overlay_highlight_test.cpp); the
// drawing itself (GDI AlphaBlend) is in overlay.cpp.
//
//   - A Picked area (read already): un-dimmed + a soft fill of the accent, no frame.
//   - The Block under the mouse, a pressed Block and the Drag rectangle: un-dimmed + a see-through frame of
//     the accent, no fill.
//   - The read-along bar: the accent, opaque, touching the bottom of the word's line box (no gap).
#include <array>
#include <cstdint>

#include "common.h"

namespace overlay_highlight {

// Opacity as AlphaBlend's SourceConstantAlpha (0 = invisible, 255 = opaque). The fill of a Picked area is
// quiet (about 22%: 0.22 * 255 = 56) because it covers the text the person is reading; the frame is a thin
// line, so it can be stronger (about 60%: 153) and still let the pixels under it show.
inline constexpr int kPickedFillAlpha = 56;
inline constexpr int kFrameAlpha = 153;

// The thickness of the hover / press / Drag frame at 96 dpi; the caller scales it for the monitor
// (control_bar_layout.h ScaleForDpi), like the read-along bar's thickness. The Drag frame uses the same: a
// 1 px line at 60% is faint over a busy screen.
inline constexpr int kFrameThickness = 2;

// The read-along bar: its height at 96 dpi (scaled by the caller) and its gap below the line box. The gap is
// 0: the bar starts exactly at the bottom edge of the box.
inline constexpr int kBarThickness = 3;
inline constexpr int kBarGap = 0;

// Lexia's accent as 0xRRGGBB for the light or the dark theme: the palette's `accent` (native_ui/palette.h),
// so the Overlay changes with the Settings window and the Control bar and has no colour of its own.
uint32_t AccentRgb(bool dark);

// The four strips of a frame of `thickness` px drawn OUTSIDE `inner` (the area it frames stays free): top,
// bottom, left, right. The strips do not overlap, so a see-through blend does not make the corners darker. The
// top and bottom strips run over the corners. A `thickness` below 1 is taken as 1. Nothing is clipped here:
// the caller draws into a bitmap that clips.
std::array<Rect, 4> FrameStrips(const Rect& inner, int thickness);

}  // namespace overlay_highlight
