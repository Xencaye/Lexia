#include "control_bar_layout.h"

#include <algorithm>

// A size measured at 96 dpi, at `dpi` (144 dpi = 150 %: 60 -> 90), rounded to the nearest pixel.
int ScaleForDpi(int base, int dpi) {
    if (dpi <= 0) dpi = 96;
    return (base * dpi + 48) / 96;  // round half up (base is not negative)
}

namespace {

// Right and bottom are outside, as with RECT.
bool Contains(const Rect& r, Point p) {
    return p.x >= r.left && p.x < r.right && p.y >= r.top && p.y < r.bottom;
}

}  // namespace

// The numbers are measured from wireframe/control_bar_large.png (433 x 60) and control_bar_small.png
// (250 x 60), at 96 dpi: the bar is 60 high; the grip's dots are 4 px, 8 px apart across and 9 px
// down; a tile is 28 x 28 with its icon (24) centred in it; Large puts a word 7 px after the tile;
// items are 18 px apart; the divider is a dotted 1 px line; the Help and Size icons are 24 px boxes
// (the drawn lines are about 20 px).
BarLayout LayoutBar(const BarState& st, int dpi) {
    auto s = [dpi](int v) { return ScaleForDpi(v, dpi); };
    const bool large = st.size == BarSize::Large;
    // K13 (owner): Pause, Skip and Stop are greyed only while the Analysis runs (control_bar_*_analysing.png);
    // once it is done they look and work as on (control_bar_*.png), also before anything speaks. A press then
    // does nothing harmful: the Reading queue ignores Pause, Skip and Stop while it is empty.
    const bool active = !st.analysing;

    BarLayout l;
    l.height = s(60);
    const int tile = s(28), box = s(24);
    const int tileTop = (l.height - tile) / 2, boxTop = (l.height - box) / 2;
    const int hitTop = s(8), hitBottom = l.height - s(8);
    const int itemGap = s(18);

    int x = s(20);
    auto addItem = [&](BarItemKind kind, BarButton b, Rect rect, Rect drawn, Rect word, bool enabled) {
        BarItem it;
        it.kind = kind;
        it.button = b;
        it.rect = rect;
        it.tile = drawn;
        it.word = word;
        it.enabled = enabled;
        l.items.push_back(it);
    };

    // The grip: 2 x 3 dots, 12 x 22 px.
    {
        const int w = s(12), h = s(22);
        const Rect r = {x, (l.height - h) / 2, x + w, (l.height - h) / 2 + h};
        addItem(BarItemKind::Grip, BarButton::None, r, r, Rect{}, true);
        x += w + itemGap;
    }

    // Pause, Skip, Stop: a tile, and in Large its word.
    for (BarButton b : {BarButton::Pause, BarButton::Skip, BarButton::Stop}) {
        const Rect t = {x, tileTop, x + tile, tileTop + tile};
        Rect word;
        int right = t.right;
        if (large) {
            const int wordW = st.labelWidth[static_cast<size_t>(b)];
            word = {t.right + s(7), tileTop, t.right + s(7) + wordW, tileTop + tile};
            right = word.right;
        }
        const int ext = s(4);
        addItem(BarItemKind::Button, b, Rect{x - ext, hitTop, right + ext, hitBottom}, t, word, active);
        x = right + itemGap;
    }

    // The divider: a dotted 1 px line, 7 px in from the top and the bottom. In Small the design puts
    // it one pixel nearer the tiles.
    if (!large) x -= s(1);
    {
        const Rect r = {x, s(7), x + std::max(1, s(1)), l.height - s(7)};
        addItem(BarItemKind::Divider, BarButton::None, r, r, Rect{}, true);
        x = r.right + itemGap;
    }

    // Help (Large only), then the size icon. The hit areas are 12 px wider than the 24 px boxes:
    // they touch each other but do not overlap.
    auto iconButton = [&](BarButton b) {
        const Rect icon = {x, boxTop, x + box, boxTop + box};
        const int ext = s(12) / 2;  // never more than half of the 12 px between two icons
        addItem(BarItemKind::Button, b, Rect{x - ext, hitTop, x + box + ext, hitBottom}, icon, Rect{}, true);
        x += box;
    };
    if (large) {
        iconButton(BarButton::Help);
        x += s(12);
    }
    iconButton(BarButton::Size);

    l.width = x + s(20);
    return l;
}

// What is under point `p` (bar coordinates): a button (enabled or not), the grip, or the bar's
// background (a divider counts as background); None outside the bar. A disabled button is still a
// button, so a click on it does nothing.
BarHit HitTest(const BarLayout& layout, Point p) {
    BarHit hit;
    if (p.x < 0 || p.y < 0 || p.x >= layout.width || p.y >= layout.height) return hit;
    hit.kind = BarItemKind::Background;
    for (const BarItem& it : layout.items) {
        if (!Contains(it.rect, p)) continue;
        switch (it.kind) {
        case BarItemKind::Button:
            hit.kind = BarItemKind::Button;
            hit.button = it.button;
            hit.enabled = it.enabled;
            return hit;
        case BarItemKind::Grip:
            hit.kind = BarItemKind::Grip;
            return hit;
        default:
            return hit;  // a Divider: the bar itself
        }
    }
    return hit;
}

// The default place: centred at the top of the monitor, 24 px (at 96 dpi) below its edge.
Point DefaultBarOrigin(const Rect& monitor, int barWidth, int dpi) {
    const int monW = monitor.right - monitor.left;
    return {monitor.left + (monW - barWidth) / 2, monitor.top + ScaleForDpi(24, dpi)};
}

// Keeps the whole bar on the monitor.
Point ClampBarOrigin(Point origin, int barWidth, int barHeight, const Rect& monitor) {
    origin.x = std::max(monitor.left, std::min(origin.x, monitor.right - barWidth));
    origin.y = std::max(monitor.top, std::min(origin.y, monitor.bottom - barHeight));
    return origin;
}

// After a size switch: the new bar has the same centre as the old one (then kept on the monitor).
Point OriginKeepingCentre(Point origin, int oldWidth, int newWidth, int barHeight, const Rect& monitor) {
    origin.x += (oldWidth - newWidth) / 2;
    return ClampBarOrigin(origin, newWidth, barHeight, monitor);
}

// Where a Session's bar opens: the saved place (offsets from the monitor's corner; -1 = never moved),
// kept on the monitor, else the default place.
Point InitialBarOrigin(int savedX, int savedY, const Rect& monitor, int barWidth, int barHeight, int dpi) {
    if (savedX < 0 || savedY < 0) return DefaultBarOrigin(monitor, barWidth, dpi);
    return ClampBarOrigin({monitor.left + savedX, monitor.top + savedY}, barWidth, barHeight, monitor);
}
