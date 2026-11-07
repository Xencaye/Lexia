#include "native_ui/pages/page.h"

#include <cstring>

#include "native_ui/screen.h"

namespace nui {

namespace {
constexpr const char* kNames[] = {"home", "voices", "settings", "report", "help", "about"};
}

const char* PageName(Page page) { return kNames[static_cast<int>(page)]; }

bool ParsePageName(std::string_view name, Page& out) {
    for (int i = 0; i < 6; ++i) {
        if (name == kNames[i]) {
            out = static_cast<Page>(i);
            return true;
        }
    }
    return false;
}

// ------------------------------------------------------------------------------------ Panel ---

Widget* Panel::HitTest(PointF p) {
    if (!Visible() || !Abs().Contains(p)) return nullptr;
    // Later children are painted on top, so they are asked first (as Widget does) ...
    for (auto it = Children().rbegin(); it != Children().rend(); ++it) {
        if (Widget* hit = (*it)->HitTest(p)) return hit;
    }
    // ... but the panel itself is nothing to point at. Without this a page that fills the window would
    // sit on top of the frame's links and swallow every press.
    return nullptr;
}

void Panel::PrepareLayout(Canvas& canvas) {
    if (layoutDirty_) {
        layoutDirty_ = false;  // first: Relayout may ask for another pass (it does not, today)
        Relayout(canvas);
    }
    Widget::PrepareLayout(canvas);
}

void Panel::Relayout(Canvas&) {}

// ------------------------------------------------------------------------------- DottedRule ---

void DottedRule::Paint(Canvas& canvas, const Palette& p) {
    const RectF r = Abs();
    canvas.DottedLine({r.x, r.y}, {r.Right(), r.y}, p.line);
}

// ---------------------------------------------------------------------------------- PageView ---

PageView::PageView(Page kind, const PageEnv& env) : env_(env), kind_(kind) {
    SetRole(Role::Pane);
}

void PageView::Emit(const PageMessage& message) const {
    if (Screen* s = GetScreen()) s->Emit(message);
}

PageMessage MakeMessage(PageMessageType type, const char* typeText) {
    PageMessage m;
    m.type = type;
    m.typeText = typeText;
    return m;
}

}  // namespace nui
