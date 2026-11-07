// Unit tests for control_bar_layout.h: the Control bar's rectangles, which buttons work, hit-testing
// and keeping the bar on the monitor. All in device pixels; the numbers are measured from
// wireframe/control_bar_large.png (433 x 60) and control_bar_small.png (250 x 60) at 96 dpi.
#include <doctest/doctest.h>

#include <vector>

#include "control_bar_layout.h"

namespace {

BarState Idle(BarSize size = BarSize::Large) {
    BarState s;
    s.size = size;
    return s;
}
BarState Speaking(BarSize size = BarSize::Large) {
    BarState s = Idle(size);
    s.speaking = true;
    return s;
}
// The words of the design, as wide as they are drawn there (51, 37 and 37 px).
BarState WithWords(BarState s) {
    s.labelWidth[static_cast<int>(BarButton::Pause)] = 51;
    s.labelWidth[static_cast<int>(BarButton::Skip)] = 37;
    s.labelWidth[static_cast<int>(BarButton::Stop)] = 37;
    return s;
}

const BarItem* Find(const BarLayout& l, BarItemKind kind, BarButton b = BarButton::None) {
    for (const BarItem& i : l.items) {
        if (i.kind == kind && (kind != BarItemKind::Button || i.button == b)) return &i;
    }
    return nullptr;
}

int W(const Rect& r) { return r.right - r.left; }
int H(const Rect& r) { return r.bottom - r.top; }

bool Overlap(const Rect& a, const Rect& b) {
    return a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom;
}

}  // namespace

TEST_CASE("ScaleForDpi: 96 dpi is 1:1, 144 is one and a half, rounding to the nearest pixel") {
    CHECK(ScaleForDpi(56, 96) == 56);
    CHECK(ScaleForDpi(56, 144) == 84);
    CHECK(ScaleForDpi(10, 120) == 13);  // 12.5 rounds up
    CHECK(ScaleForDpi(56, 0) == 56);    // an unknown DPI is 96
}

TEST_CASE("LayoutBar Large with the design's words is the 433 x 60 bar of the wireframe") {
    const BarLayout l = LayoutBar(WithWords(Speaking()), 96);
    CHECK(l.width == 433);
    CHECK(l.height == 60);
    const BarItem* pause = Find(l, BarItemKind::Button, BarButton::Pause);
    const BarItem* skip = Find(l, BarItemKind::Button, BarButton::Skip);
    const BarItem* stop = Find(l, BarItemKind::Button, BarButton::Stop);
    const BarItem* help = Find(l, BarItemKind::Button, BarButton::Help);
    const BarItem* size = Find(l, BarItemKind::Button, BarButton::Size);
    REQUIRE((pause && skip && stop && help && size));
    // Tiles: 28 x 28 at x = 50, 154, 244 and y = 16.
    CHECK(pause->tile.left == 50);
    CHECK(skip->tile.left == 154);
    CHECK(stop->tile.left == 244);
    for (const BarItem* b : {pause, skip, stop}) {
        CHECK(W(b->tile) == 28);
        CHECK(H(b->tile) == 28);
        CHECK(b->tile.top == 16);
        CHECK(b->word.left == b->tile.right + 7);
    }
    // The divider at x = 334; the Help icon box (24) at 353; the size icon box at 389.
    CHECK(Find(l, BarItemKind::Divider)->rect.left == 334);
    CHECK(help->tile.left == 353);
    CHECK(W(help->tile) == 24);
    CHECK(size->tile.left == 389);
    CHECK(H(size->tile) == 24);
    // The grip's dots: x = 20..31, y = 19..40.
    const BarItem* grip = Find(l, BarItemKind::Grip);
    REQUIRE(grip);
    CHECK(grip->tile.left == 20);
    CHECK(grip->tile.right == 32);
    CHECK(grip->tile.top == 19);
    CHECK(grip->tile.bottom == 41);
}

TEST_CASE("LayoutBar Large: the order is grip, Pause, Skip, Stop, divider, Help, Size; no Close") {
    const BarLayout l = LayoutBar(WithWords(Speaking()), 96);
    REQUIRE(l.items.size() == 7);
    CHECK(l.items[0].kind == BarItemKind::Grip);
    CHECK(l.items[1].button == BarButton::Pause);
    CHECK(l.items[2].button == BarButton::Skip);
    CHECK(l.items[3].button == BarButton::Stop);
    CHECK(l.items[4].kind == BarItemKind::Divider);
    CHECK(l.items[5].button == BarButton::Help);
    CHECK(l.items[6].button == BarButton::Size);
    for (size_t i = 1; i < l.items.size(); ++i) CHECK(l.items[i - 1].rect.right <= l.items[i].rect.left);
}

TEST_CASE("LayoutBar Small with the design's measures is the 250 x 60 bar of the wireframe") {
    const BarLayout l = LayoutBar(Speaking(BarSize::Small), 96);
    CHECK(l.width == 250);
    CHECK(l.height == 60);
    CHECK(Find(l, BarItemKind::Button, BarButton::Help) == nullptr);  // Help is in the Large bar only
    int left = 50;
    for (BarButton b : {BarButton::Pause, BarButton::Skip, BarButton::Stop}) {
        const BarItem* it = Find(l, BarItemKind::Button, b);
        REQUIRE(it);
        CHECK(it->tile.left == left);
        CHECK(W(it->tile) == 28);
        CHECK(H(it->tile) == 28);
        CHECK(W(it->word) == 0);  // no words in Small
        left += 46;
    }
    CHECK(Find(l, BarItemKind::Divider)->rect.left == 187);
    const BarItem* size = Find(l, BarItemKind::Button, BarButton::Size);
    REQUIRE(size);
    CHECK(size->tile.left == 206);
    CHECK(W(size->tile) == 24);
    CHECK(l.items.size() == 6);
}

TEST_CASE("LayoutBar: a button is hit on a taller area than its tile") {
    const BarLayout l = LayoutBar(WithWords(Speaking()), 96);
    const BarItem* skip = Find(l, BarItemKind::Button, BarButton::Skip);
    CHECK(H(skip->rect) == 44);
    CHECK(skip->rect.top < skip->tile.top);
    CHECK(skip->rect.left <= skip->tile.left);
    CHECK(skip->rect.right >= skip->word.right);  // the word is part of the button
}

TEST_CASE("LayoutBar: nothing overlaps and everything is inside the bar, which starts at 0,0") {
    for (BarSize size : {BarSize::Large, BarSize::Small}) {
        for (int dpi : {96, 144, 192}) {
            BarState st = WithWords(Speaking(size));
            st.analysing = true;
            const BarLayout l = LayoutBar(st, dpi);
            CHECK(l.width > 0);
            CHECK(l.height > 0);
            for (size_t i = 0; i < l.items.size(); ++i) {
                const Rect& a = l.items[i].rect;
                CHECK(a.left >= 0);
                CHECK(a.top >= 0);
                CHECK(a.right <= l.width);
                CHECK(a.bottom <= l.height);
                for (size_t j = i + 1; j < l.items.size(); ++j) CHECK_FALSE(Overlap(a, l.items[j].rect));
            }
        }
    }
}

TEST_CASE("LayoutBar: everything scales with the DPI") {
    const BarLayout a = LayoutBar(Speaking(BarSize::Small), 96);
    const BarLayout b = LayoutBar(Speaking(BarSize::Small), 192);
    CHECK(W(Find(b, BarItemKind::Button, BarButton::Skip)->tile) == 56);
    CHECK(b.height == 120);
    CHECK(b.height == 2 * a.height);
    CHECK(b.width >= 2 * a.width - 4);
    CHECK(b.width <= 2 * a.width + 4);
}

TEST_CASE("LayoutBar: Pause and Resume are one width, so the bar does not move") {
    BarState speaking = WithWords(Speaking());
    BarState paused = WithWords(Idle());
    paused.paused = true;
    CHECK(LayoutBar(speaking, 96).width == LayoutBar(paused, 96).width);
}

TEST_CASE("LayoutBar: a longer word widens its button and so the bar") {
    BarState st = WithWords(Speaking());
    const int plain = LayoutBar(st, 96).width;
    st.labelWidth[static_cast<int>(BarButton::Skip)] = 137;  // 100 px more
    const BarLayout l = LayoutBar(st, 96);
    CHECK(l.width == plain + 100);
    CHECK(W(Find(l, BarItemKind::Button, BarButton::Skip)->word) == 137);
}

TEST_CASE("LayoutBar: once the Analysis is done Pause, Skip and Stop work, also before anything speaks") {
    // K13 (owner): only the Analysis greys them; the idle bar after it looks like the speaking one.
    const BarLayout idle = LayoutBar(Idle(), 96);
    for (BarButton b : {BarButton::Pause, BarButton::Skip, BarButton::Stop}) {
        CHECK(Find(idle, BarItemKind::Button, b)->enabled);
    }
    CHECK(Find(idle, BarItemKind::Button, BarButton::Help)->enabled);
    CHECK(Find(idle, BarItemKind::Button, BarButton::Size)->enabled);

    for (int which = 0; which < 2; ++which) {
        BarState st = Idle();
        (which == 0 ? st.speaking : st.paused) = true;
        const BarLayout l = LayoutBar(st, 96);
        for (BarButton b : {BarButton::Pause, BarButton::Skip, BarButton::Stop}) {
            CHECK(Find(l, BarItemKind::Button, b)->enabled);
        }
    }
}

TEST_CASE("LayoutBar: while the Analysis runs Pause, Skip and Stop are disabled; Help and Size are not") {
    for (BarSize size : {BarSize::Large, BarSize::Small}) {
        for (bool speaking : {false, true}) {
            BarState st = Idle(size);
            st.analysing = true;
            st.speaking = speaking;
            const BarLayout l = LayoutBar(st, 96);
            for (BarButton b : {BarButton::Pause, BarButton::Skip, BarButton::Stop}) {
                CHECK_FALSE(Find(l, BarItemKind::Button, b)->enabled);
            }
            CHECK(Find(l, BarItemKind::Button, BarButton::Size)->enabled);
            if (size == BarSize::Large) CHECK(Find(l, BarItemKind::Button, BarButton::Help)->enabled);
        }
    }
}

TEST_CASE("LayoutBar: the Analysis does not change the bar's size (there is no status well)") {
    BarState st = WithWords(Speaking());
    const int plain = LayoutBar(st, 96).width;
    st.analysing = true;
    CHECK(LayoutBar(st, 96).width == plain);
}

TEST_CASE("HitTest: a point on a button is that button, on the grip is the grip") {
    const BarLayout l = LayoutBar(WithWords(Speaking()), 96);
    const Rect skip = Find(l, BarItemKind::Button, BarButton::Skip)->rect;
    const BarHit onSkip = HitTest(l, {(skip.left + skip.right) / 2, (skip.top + skip.bottom) / 2});
    CHECK(onSkip.kind == BarItemKind::Button);
    CHECK(onSkip.button == BarButton::Skip);
    CHECK(onSkip.enabled);
    const Rect grip = Find(l, BarItemKind::Grip)->rect;
    CHECK(HitTest(l, {grip.left + 1, grip.top + 1}).kind == BarItemKind::Grip);
}

TEST_CASE("HitTest: a disabled button is still a button, but not enabled") {
    BarState st = WithWords(Idle());
    st.analysing = true;  // the only time a tile is disabled (K13)
    const BarLayout l = LayoutBar(st, 96);
    const Rect stop = Find(l, BarItemKind::Button, BarButton::Stop)->tile;
    const BarHit hit = HitTest(l, {stop.left + 2, stop.top + 2});
    CHECK(hit.kind == BarItemKind::Button);
    CHECK(hit.button == BarButton::Stop);
    CHECK_FALSE(hit.enabled);
}

TEST_CASE("HitTest: a button's right edge is outside it, like a RECT") {
    const BarLayout l = LayoutBar(WithWords(Speaking()), 96);
    const Rect skip = Find(l, BarItemKind::Button, BarButton::Skip)->rect;
    const int midY = (skip.top + skip.bottom) / 2;
    CHECK(HitTest(l, {skip.left, midY}).button == BarButton::Skip);
    CHECK(HitTest(l, {skip.right - 1, midY}).button == BarButton::Skip);
    CHECK(HitTest(l, {skip.right, midY}).button != BarButton::Skip);
}

TEST_CASE("HitTest: Help and Size are the icons and the room round them") {
    const BarLayout l = LayoutBar(WithWords(Speaking()), 96);
    for (BarButton b : {BarButton::Help, BarButton::Size}) {
        const BarItem* it = Find(l, BarItemKind::Button, b);
        REQUIRE(it);
        const Rect c = it->tile;
        CHECK(HitTest(l, {(c.left + c.right) / 2, (c.top + c.bottom) / 2}).button == b);
        CHECK(HitTest(l, {c.left - 3, (c.top + c.bottom) / 2}).button == b);
    }
}

TEST_CASE("HitTest: the empty part of the bar drags it, outside the bar is nothing") {
    const BarLayout l = LayoutBar(WithWords(Speaking()), 96);
    CHECK(HitTest(l, {0, 0}).kind == BarItemKind::Background);  // the padding
    CHECK(HitTest(l, {l.width, 5}).kind == BarItemKind::None);
    CHECK(HitTest(l, {-1, 5}).kind == BarItemKind::None);
    CHECK(HitTest(l, {5, l.height}).kind == BarItemKind::None);
}

TEST_CASE("HitTest: a divider is the bar's background, not a button") {
    const BarLayout l = LayoutBar(WithWords(Speaking()), 96);
    const Rect div = Find(l, BarItemKind::Divider)->rect;
    CHECK(HitTest(l, {div.left, div.top + 10}).kind == BarItemKind::Background);
}

TEST_CASE("DefaultBarOrigin: top centre of the monitor, 24 px from the top edge") {
    const Rect mon = {0, 0, 1920, 1080};
    const Point p = DefaultBarOrigin(mon, 600, 96);
    CHECK(p.x == 660);
    CHECK(p.y == 24);
}

TEST_CASE("DefaultBarOrigin: on a monitor that does not start at 0,0 and at 150 % DPI") {
    const Rect mon = {-1920, 100, 0, 1180};
    const Point p = DefaultBarOrigin(mon, 600, 144);
    CHECK(p.x == -1920 + (1920 - 600) / 2);
    CHECK(p.y == 100 + 36);
}

TEST_CASE("ClampBarOrigin: a bar fully on the monitor stays where it is") {
    const Rect mon = {0, 0, 1920, 1080};
    const Point p = ClampBarOrigin({100, 50}, 600, 60, mon);
    CHECK(p.x == 100);
    CHECK(p.y == 50);
}

TEST_CASE("ClampBarOrigin: a bar off an edge is pushed back in") {
    const Rect mon = {0, 0, 1920, 1080};
    Point p = ClampBarOrigin({-50, -20}, 600, 60, mon);
    CHECK(p.x == 0);
    CHECK(p.y == 0);
    p = ClampBarOrigin({1800, 1070}, 600, 60, mon);
    CHECK(p.x == 1320);
    CHECK(p.y == 1020);
}

TEST_CASE("ClampBarOrigin: a saved place from a bigger monitor ends up on a smaller one") {
    const Rect mon = {0, 0, 1280, 720};
    const Point p = ClampBarOrigin({3000, 2000}, 600, 60, mon);
    CHECK(p.x == 680);
    CHECK(p.y == 660);
}

TEST_CASE("ClampBarOrigin: a bar wider than the monitor starts at the left edge") {
    const Rect mon = {0, 0, 400, 300};
    const Point p = ClampBarOrigin({50, 50}, 600, 60, mon);
    CHECK(p.x == 0);
}

TEST_CASE("OriginKeepingCentre: a wider bar grows to both sides, then is kept on the monitor") {
    const Rect mon = {0, 0, 1920, 1080};
    Point p = OriginKeepingCentre({500, 40}, 400, 600, 60, mon);
    CHECK(p.x == 400);
    CHECK(p.y == 40);
    p = OriginKeepingCentre({10, 40}, 400, 600, 60, mon);  // would go to -90
    CHECK(p.x == 0);
}

TEST_CASE("InitialBarOrigin: -1 means the default place, anything else is clamped") {
    const Rect mon = {0, 0, 1920, 1080};
    Point p = InitialBarOrigin(-1, -1, mon, 600, 60, 96);
    CHECK(p.x == 660);
    CHECK(p.y == 24);
    p = InitialBarOrigin(5000, 10, mon, 600, 60, 96);
    CHECK(p.x == 1320);
    CHECK(p.y == 10);
    p = InitialBarOrigin(200, -1, mon, 600, 60, 96);  // only one of the two is set: the default place
    CHECK(p.x == 660);
    CHECK(p.y == 24);
}

TEST_CASE("InitialBarOrigin: the saved place is an offset from the monitor's top left corner") {
    const Rect mon = {-1920, 100, 0, 1180};
    const Point p = InitialBarOrigin(300, 50, mon, 600, 60, 96);
    CHECK(p.x == -1620);
    CHECK(p.y == 150);
}
