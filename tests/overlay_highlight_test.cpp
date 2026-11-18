// The Overlay's look (G3): the accent of the two themes, the opacities and the frame strips.
#include <doctest/doctest.h>

#include "native_ui/palette.h"
#include "overlay_highlight.h"

using namespace overlay_highlight;

namespace {

// Number of strips that contain pixel (x, y).
int CoveredBy(const std::array<Rect, 4>& strips, int x, int y) {
    int n = 0;
    for (const Rect& r : strips) {
        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) ++n;
    }
    return n;
}

}  // namespace

TEST_CASE("Overlay highlight: the accent is the palette's accent of the theme") {
    CHECK(AccentRgb(false) == nui::PaletteFor(false).accent.rgb);
    CHECK(AccentRgb(true) == nui::PaletteFor(true).accent.rgb);
    CHECK(AccentRgb(false) == 0x6E504A);  // the owner's light accent
    CHECK(AccentRgb(true) == 0x708C87);   // the owner's dark accent
    CHECK(AccentRgb(false) != AccentRgb(true));
}

TEST_CASE("Overlay highlight: the opacities are see-through and the bar touches the box") {
    CHECK(kPickedFillAlpha > 0);
    CHECK(kPickedFillAlpha < kFrameAlpha);  // the fill is the quieter of the two
    CHECK(kFrameAlpha < 255);
    CHECK(kFrameThickness == 2);
    CHECK(kBarGap == 0);
}

TEST_CASE("Overlay highlight: a frame is four strips outside the area, none over it") {
    const Rect inner{10, 20, 50, 40};
    const std::array<Rect, 4> s = FrameStrips(inner, 2);
    CHECK(s[0].left == 8);  CHECK(s[0].top == 18);  CHECK(s[0].right == 52);  CHECK(s[0].bottom == 20);  // top
    CHECK(s[1].left == 8);  CHECK(s[1].top == 40);  CHECK(s[1].right == 52);  CHECK(s[1].bottom == 42);  // bottom
    CHECK(s[2].left == 8);  CHECK(s[2].top == 20);  CHECK(s[2].right == 10);  CHECK(s[2].bottom == 40);  // left
    CHECK(s[3].left == 50); CHECK(s[3].top == 20);  CHECK(s[3].right == 52);  CHECK(s[3].bottom == 40);  // right

    // Every pixel of the frame is in exactly one strip; the framed area and the outside are in none.
    for (int y = 10; y < 50; ++y) {
        for (int x = 0; x < 60; ++x) {
            const bool inside = x >= inner.left && x < inner.right && y >= inner.top && y < inner.bottom;
            const bool ring = x >= 8 && x < 52 && y >= 18 && y < 42 && !inside;
            CHECK(CoveredBy(s, x, y) == (ring ? 1 : 0));
        }
    }
}

TEST_CASE("Overlay highlight: the strip thickness is at least one pixel") {
    const std::array<Rect, 4> s = FrameStrips(Rect{5, 5, 9, 9}, 0);
    CHECK(s[0].bottom - s[0].top == 1);
    CHECK(s[2].right - s[2].left == 1);
    const std::array<Rect, 4> n = FrameStrips(Rect{5, 5, 9, 9}, -3);
    CHECK(n[1].bottom - n[1].top == 1);
}

TEST_CASE("Overlay highlight: a frame at the Shot's edge reaches past it (the bitmap clips)") {
    const std::array<Rect, 4> s = FrameStrips(Rect{0, 0, 10, 10}, 4);
    CHECK(s[0].left == -4);
    CHECK(s[0].top == -4);
    CHECK(s[2].left == -4);
}
