#pragma once
// The Control bar's geometry (the spec is old ui/dev/controlbar.html and wireframe/control_bar_*.png, in px
// at 96 dpi): where each part of the bar is for a size, a state and a DPI; which buttons work; what a
// mouse position is on; and where the bar sits on the monitor. Pure, Win32-free; the drawing and the
// windows are in controlbar.cpp. All results are device pixels. A bar's rectangles start at 0,0 (its
// own top left corner).
#include <array>
#include <vector>

#include "common.h"

enum class BarSize { Large, Small };

// Pause is Resume while paused. Help is in the Large bar only; Size is the minimise / maximise icon.
enum class BarButton { Pause, Skip, Stop, Help, Size, None };

struct BarState {
    BarSize size = BarSize::Large;
    bool analysing = false;  // the Analysis job is running: Pause, Skip and Stop are disabled (only then, K13)
    bool speaking = false;   // the Reading queue holds something and is not paused
    bool paused = false;
    // Width in device pixels of the word each of Pause, Skip and Stop shows now (Pause: Resume while
    // paused). Every item then has the same 18 px gap after it, as in the design; the bar gets wider
    // or narrower when Pause becomes Resume. 0 = no word.
    // Large only; Help and Size have no word.
    std::array<int, 5> labelWidth = {0, 0, 0, 0, 0};  // by BarButton
};

enum class BarItemKind { None, Grip, Button, Divider, Background };

struct BarItem {
    BarItemKind kind = BarItemKind::None;
    BarButton button = BarButton::None;  // for a Button
    // What the mouse hits. For a Button it is taller (and a little wider) than what is drawn.
    Rect rect;
    // What is drawn: a Button's tile (Pause, Skip, Stop) or its icon box (Help, Size); the grip's
    // dots; the divider's line.
    Rect tile;
    // A Button's word box (Large, Pause, Skip and Stop only; empty otherwise).
    Rect word;
    bool enabled = true;  // a Button: does it work now
};

struct BarLayout {
    int width = 0, height = 0;
    // Left to right: Grip, Pause, Skip, Stop, Divider, [Help,] Size.
    std::vector<BarItem> items;
};

// `base` px at 96 dpi as device pixels, to the nearest one. A dpi of 0 counts as 96.
int ScaleForDpi(int base, int dpi);

BarLayout LayoutBar(const BarState& state, int dpi);

struct BarHit {
    BarItemKind kind = BarItemKind::None;  // None = outside the bar; Background = the bar itself
    BarButton button = BarButton::None;
    bool enabled = false;  // for a Button
};

// What `p` (relative to the bar) is on. Only a Button is pressed; a Grip, a Divider and the bare bar
// all drag it. A disabled Button is still a Button (it eats the click) but `enabled` is false.
BarHit HitTest(const BarLayout& layout, Point p);

// Where the bar goes when nothing is saved: top centre of `monitor`, 24 px (scaled) from the top.
Point DefaultBarOrigin(const Rect& monitor, int barWidth, int dpi);

// `origin` moved the least that puts the whole bar on `monitor` (a bar wider than the monitor is
// put at its left edge).
Point ClampBarOrigin(Point origin, int barWidth, int barHeight, const Rect& monitor);

// The origin of a bar that changes width and keeps its centre (a size switch), kept on the monitor.
Point OriginKeepingCentre(Point origin, int oldWidth, int newWidth, int barHeight, const Rect& monitor);

// The origin for a new Session: settings.json "controlBar.x" / "controlBar.y" are offsets from the
// monitor's top left corner, -1 = not set; if either is missing the default place is used. Clamped
// to the monitor.
Point InitialBarOrigin(int savedX, int savedY, const Rect& monitor, int barWidth, int barHeight, int dpi);
