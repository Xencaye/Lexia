#pragma once
// A Canvas that draws nothing and writes down what it was asked to draw (K1.b), so the widget tree of the
// native Settings window is tested without a window. Text is measured by counting code points: every
// code point is 0.5 * size wide (times boldFactor in a bold style), a line is 1.35 * size high.
#include "native_ui/canvas.h"

#include <string>
#include <string_view>
#include <vector>

namespace nui {

struct Call {
    enum Kind {
        FillRect, StrokeRect, FillEllipse, StrokeEllipse, Line, Polyline, Arc, Dotted, Text, Icon, Picture,
        PushClip, PopClip, PushOpacity, PopOpacity
    };
    Kind kind = FillRect;
    RectF rect;               // the box of the call (Line: from a to b as x,y,w,h is not kept: see points)
    Colour colour;
    float number = 0;         // stroke width, or the opacity of PushOpacity / Picture
    float radius = 0;
    std::string text;         // the text, or the icon / picture name
    TextStyle style;
    std::vector<PointF> points;
};

class FakeCanvas : public Canvas {
public:
    float boldFactor = 1.0f;  // a bold code point is this much wider (tests of the reserved bold width)
    std::vector<Call> calls;

    void Clear() { calls.clear(); }
    int Count(Call::Kind k) const {
        int n = 0;
        for (const Call& c : calls) n += c.kind == k ? 1 : 0;
        return n;
    }
    std::vector<const Call*> Of(Call::Kind k) const {
        std::vector<const Call*> out;
        for (const Call& c : calls)
            if (c.kind == k) out.push_back(&c);
        return out;
    }
    // The first Text / Icon call with exactly this text (or null).
    const Call* FindText(std::string_view t) const {
        for (const Call& c : calls)
            if (c.kind == Call::Text && c.text == t) return &c;
        return nullptr;
    }
    const Call* FindIcon(std::string_view name) const {
        for (const Call& c : calls)
            if (c.kind == Call::Icon && c.text == name) return &c;
        return nullptr;
    }
    // Is a push of an opacity open (not yet popped) with this value somewhere in the calls?
    bool HasOpacity(float v) const {
        for (const Call& c : calls)
            if (c.kind == Call::PushOpacity && c.number == v) return true;
        return false;
    }

    static int CodePoints(std::string_view s) {
        int n = 0;
        for (unsigned char ch : s) n += (ch & 0xC0) != 0x80 ? 1 : 0;
        return n;
    }

    void FillRect(const RectF& r, Colour c, float radius) override { Add(Call::FillRect, r, c, 0, radius); }
    void StrokeRect(const RectF& r, Colour c, float width, float radius) override {
        Add(Call::StrokeRect, r, c, width, radius);
    }
    void FillEllipse(const RectF& b, Colour c) override { Add(Call::FillEllipse, b, c, 0, 0); }
    void StrokeEllipse(const RectF& b, Colour c, float width) override { Add(Call::StrokeEllipse, b, c, width, 0); }
    void Line(PointF a, PointF b, Colour c, float width) override {
        Call& k = Add(Call::Line, {}, c, width, 0);
        k.points = {a, b};
    }
    void Polyline(const std::vector<PointF>& pts, Colour c, float width) override {
        Call& k = Add(Call::Polyline, {}, c, width, 0);
        k.points = pts;
    }
    void Arc(PointF centre, float, float, float, Colour c, float width) override {
        Call& k = Add(Call::Arc, {}, c, width, 0);
        k.points = {centre};
    }
    void DottedLine(PointF a, PointF b, Colour c) override {
        Call& k = Add(Call::Dotted, {}, c, 1, 0);
        k.points = {a, b};
    }
    void Text(const RectF& box, std::string_view utf8, const TextStyle& style, Colour c) override {
        Call& k = Add(Call::Text, box, c, 0, 0);
        k.text = std::string(utf8);
        k.style = style;
    }
    SizeF MeasureText(std::string_view utf8, const TextStyle& style) override {
        const float per = 0.5f * style.size * (style.weight == FontWeight::Bold ? boldFactor : 1.0f);
        return {per * static_cast<float>(CodePoints(utf8)), 1.35f * style.size};
    }
    void Icon(const RectF& box, std::string_view name, Colour c) override {
        Call& k = Add(Call::Icon, box, c, 0, 0);
        k.text = std::string(name);
    }
    void Picture(const RectF& box, std::string_view name, Colour tint, float opacity) override {
        Call& k = Add(Call::Picture, box, tint, opacity, 0);
        k.text = std::string(name);
    }
    void PushClip(const RectF& r) override { Add(Call::PushClip, r, {}, 0, 0); }
    void PopClip() override { Add(Call::PopClip, {}, {}, 0, 0); }
    void PushOpacity(float o) override { Add(Call::PushOpacity, {}, {}, o, 0); }
    void PopOpacity() override { Add(Call::PopOpacity, {}, {}, 0, 0); }

private:
    Call& Add(Call::Kind k, const RectF& r, Colour c, float number, float radius) {
        Call call;
        call.kind = k;
        call.rect = r;
        call.colour = c;
        call.number = number;
        call.radius = radius;
        calls.push_back(std::move(call));
        return calls.back();
    }
};

}  // namespace nui
