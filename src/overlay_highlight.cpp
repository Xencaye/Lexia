#include "overlay_highlight.h"

#include <algorithm>

#include "native_ui/palette.h"

namespace overlay_highlight {

uint32_t AccentRgb(bool dark) { return nui::PaletteFor(dark).accent.rgb; }

std::array<Rect, 4> FrameStrips(const Rect& inner, int thickness) {
    const int t = std::max(thickness, 1);
    std::array<Rect, 4> s;
    // Top and bottom span the corners (inner.left - t .. inner.right + t), so the left and right strips only
    // need the height of `inner`: that way no pixel belongs to two strips.
    s[0] = Rect{inner.left - t, inner.top - t, inner.right + t, inner.top};
    s[1] = Rect{inner.left - t, inner.bottom, inner.right + t, inner.bottom + t};
    s[2] = Rect{inner.left - t, inner.top, inner.left, inner.bottom};
    s[3] = Rect{inner.right, inner.top, inner.right + t, inner.bottom};
    return s;
}

}  // namespace overlay_highlight
