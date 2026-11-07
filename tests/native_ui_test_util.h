#pragma once
// Shared helpers of the tests/native_ui_*_test.cpp files: a Screen with a FakeCanvas, and short
// functions that send mouse and key events the way the window will.
#include "fake_canvas.h"
#include "native_ui/screen.h"

namespace nui::test {

struct Ui {
    Screen screen;
    FakeCanvas canvas;
    Palette pal = PaletteFor(false);
    double now = 0;

    Ui() { screen.SetSize({800, 600}); }

    // One frame: Tick to `now`, paint into a cleared canvas.
    void Frame() {
        screen.Tick(now);
        canvas.Clear();
        screen.Paint(canvas, pal);
    }
    // Moves the clock on and draws a frame.
    void Advance(double seconds) {
        now += seconds;
        Frame();
    }

    bool Move(float x, float y) {
        MouseEvent e;
        e.kind = MouseKind::Move;
        e.pos = {x, y};
        return screen.HandleMouse(e);
    }
    bool Down(float x, float y, MouseButton b = MouseButton::Left) {
        MouseEvent e;
        e.kind = MouseKind::Down;
        e.button = b;
        e.pos = {x, y};
        return screen.HandleMouse(e);
    }
    bool Up(float x, float y, MouseButton b = MouseButton::Left) {
        MouseEvent e;
        e.kind = MouseKind::Up;
        e.button = b;
        e.pos = {x, y};
        return screen.HandleMouse(e);
    }
    bool Click(float x, float y) {
        Move(x, y);
        Down(x, y);
        return Up(x, y);
    }
    bool Wheel(float x, float y, float delta) {
        MouseEvent e;
        e.kind = MouseKind::Wheel;
        e.pos = {x, y};
        e.wheelDelta = delta;
        return screen.HandleMouse(e);
    }
    bool Leave() {
        MouseEvent e;
        e.kind = MouseKind::Leave;
        return screen.HandleMouse(e);
    }
    bool Key(nui::Key k, bool shift = false, bool repeat = false) {
        KeyEvent e;
        e.key = k;
        e.shift = shift;
        e.repeat = repeat;
        return screen.HandleKey(e);
    }
};

// The centre of a widget in window DIPs.
inline PointF Centre(const Widget& w) {
    const RectF r = w.Abs();
    return {r.x + r.w / 2, r.y + r.h / 2};
}

}  // namespace nui::test
