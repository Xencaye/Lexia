#include "native_ui/access_tree.h"

#include "native_ui/scroll_area.h"
#include "native_ui/sheet.h"

#include <algorithm>
#include <cmath>

namespace nui {

namespace {

// ---- the roots of the projection

// The widgets whose subtrees make up the window: the page and the layers, or, with a modal sheet open, the
// top sheet and what is above it.
std::vector<Widget*> TopLevel(const Screen& screen) {
    const std::vector<Screen::LayerView> layers = screen.Layers();
    size_t firstLayer = 0;
    bool page = true;
    for (size_t i = layers.size(); i-- > 0;) {
        if (layers[i].modal) {
            firstLayer = i;
            page = false;  // the page and the layers under the top sheet are not offered
            break;
        }
    }
    std::vector<Widget*> out;
    if (page) out.push_back(screen.PageRoot());
    for (size_t i = firstLayer; i < layers.size(); ++i) out.push_back(layers[i].widget);
    return out;
}

// Adds the offered widgets of the subtree of `w` (w included) to `out`: w itself when it is offered, else
// the offered widgets under it, so a skipped panel does not hide what it holds.
void Collect(Widget* w, std::vector<Widget*>& out) {
    if (!w->Visible()) return;
    if (IsAccessible(*w)) {
        out.push_back(w);
        return;
    }
    for (const auto& c : w->Children()) Collect(c.get(), out);
}

// Depth-first search of every visible widget under `w` (w included) for the offered one with this id. The id
// is compared first: it is cheap, and Access() builds strings.
Widget* FindIn(Widget* w, int id) {
    if (!w->Visible()) return nullptr;
    if (w->Id() == id && IsAccessible(*w)) return w;
    for (const auto& c : w->Children()) {
        if (Widget* hit = FindIn(c.get(), id)) return hit;
    }
    return nullptr;
}

const char* RoleToken(Role role) {
    switch (role) {
        case Role::None: return "none";
        case Role::Button: return "button";
        case Role::Link: return "link";
        case Role::Switch: return "switch";
        case Role::Slider: return "slider";
        case Role::ComboBox: return "combo";
        case Role::ListItem: return "item";
        case Role::List: return "list";
        case Role::Text: return "text";
        case Role::Image: return "image";
        case Role::Group: return "group";
        case Role::Dialog: return "dialog";
        case Role::ProgressBar: return "progress";
        case Role::KeyChip: return "key";
        case Role::Edit: return "edit";
        case Role::Pane: return "pane";
    }
    return "none";
}

RectF Intersect(const RectF& a, const RectF& b) {
    const float x0 = std::max(a.x, b.x), y0 = std::max(a.y, b.y);
    const float x1 = std::min(a.Right(), b.Right()), y1 = std::min(a.Bottom(), b.Bottom());
    return {x0, y0, std::max(0.0f, x1 - x0), std::max(0.0f, y1 - y0)};
}

}  // namespace

// ---------------------------------------------------------------------------- the projection ---

bool IsAccessible(const Widget& w) {
    if (w.AccessHidden()) return false;
    const AccessInfo a = w.Access();
    switch (a.role) {
        case Role::None:
            return false;
        case Role::Group:
        case Role::Pane:
        case Role::Image:
            return !a.name.empty();  // only when it is called something: a bare panel or decoration is skipped
        case Role::Text:
            return !a.name.empty() || !a.value.empty();
        default:
            return true;  // a control is always offered; a missing name is a bug the tests look for
    }
}

std::vector<Widget*> AccessChildren(const Screen& screen, const Widget* parent) {
    std::vector<Widget*> out;
    if (!parent) {
        for (Widget* top : TopLevel(screen)) Collect(top, out);
        return out;
    }
    for (const auto& c : parent->Children()) Collect(c.get(), out);
    return out;
}

Widget* AccessParent(const Screen&, const Widget& w) {
    for (Widget* p = w.Parent(); p; p = p->Parent()) {
        if (IsAccessible(*p)) return p;
    }
    return nullptr;
}

Widget* FindAccessible(const Screen& screen, int id) {
    for (Widget* top : TopLevel(screen)) {
        if (Widget* hit = FindIn(top, id)) return hit;
    }
    return nullptr;
}

Widget* AccessFocus(const Screen& screen) {
    // An open list: its highlighted row has the focus (ListRow::Access says so).
    if (Widget* popup = screen.Popup()) {
        std::vector<Widget*> rows = AccessChildren(screen, popup);
        for (Widget* row : rows) {
            if (row->Access().focused) return row;
        }
    }
    Widget* f = screen.Focused();
    for (Widget* w = f; w; w = w->Parent()) {
        if (IsAccessible(*w)) return FindAccessible(screen, w->Id()) == w ? w : nullptr;
    }
    return nullptr;
}

Widget* AccessibleAt(const Screen& screen, PointF p) {
    Widget* hit = screen.WidgetAt(p);
    for (Widget* w = hit; w; w = w->Parent()) {
        if (IsAccessible(*w)) return FindAccessible(screen, w->Id()) == w ? w : nullptr;
    }
    return nullptr;
}

std::string AccessName(const AccessInfo& a) {
    if (a.role != Role::Text) return a.name;
    // A text is read by its words. A notice has a lead word and a text: "Done. The voice is added."
    if (a.name.empty()) return a.value;
    if (a.value.empty() || a.value == a.name) return a.name;  // a Label has its text as both
    return a.name + ". " + a.value;
}

std::string AutomationIdOf(const Screen& screen, const Widget& w) {
    if (!w.AutomationId().empty()) return w.AutomationId();
    // Made from the path: for each step up, "<role>[<n>]", n counting the offered siblings of the same role.
    std::vector<std::string> parts;
    std::string prefix;
    for (const Widget* cur = &w; cur;) {
        const Widget* parent = AccessParent(screen, *cur);
        const Role role = cur->Access().role;
        int index = 0;
        for (const Widget* sib : AccessChildren(screen, parent)) {
            if (sib == cur) break;
            if (sib->Access().role == role) ++index;
        }
        parts.push_back(std::string(RoleToken(role)) + "[" + std::to_string(index) + "]");
        if (parent && !parent->AutomationId().empty()) {
            prefix = parent->AutomationId();
            break;  // an ancestor with its own id is a good start
        }
        cur = parent;
    }
    std::string out = prefix;
    for (size_t i = parts.size(); i-- > 0;) {
        if (!out.empty()) out += '/';
        out += parts[i];
    }
    return out;
}

bool InScrollArea(const Widget& w) {
    for (const Widget* a = w.Parent(); a; a = a->Parent()) {
        if (dynamic_cast<const ScrollArea*>(a)) return true;
    }
    return false;
}

AccessBounds BoundsOf(const Screen& screen, const Widget& w) {
    AccessBounds b;
    b.rect = w.Abs();
    RectF shown = b.rect;
    for (const Widget* a = w.Parent(); a; a = a->Parent()) {
        // A ScrollArea clips its children to its own box.
        if (dynamic_cast<const ScrollArea*>(a)) shown = Intersect(shown, a->Abs());
    }
    shown = Intersect(shown, {0, 0, screen.Size().w, screen.Size().h});
    b.offscreen = shown.w <= 0 || shown.h <= 0;
    if (!b.offscreen) b.rect = shown;
    return b;
}

// ---------------------------------------------------------------------------------- actions ---

namespace {

AccessResult Check(const Widget& w, const AccessInfo& a, const AccessRequest& r) {
    switch (r.action) {
        case AccessAction::Invoke:
            if (a.role != Role::Button && a.role != Role::Link && a.role != Role::KeyChip) {
                return AccessResult::NotSupported;
            }
            return a.enabled ? AccessResult::Done : AccessResult::NotEnabled;
        case AccessAction::Toggle:
            if (a.toggle == Toggle::None) return AccessResult::NotSupported;
            return a.enabled ? AccessResult::Done : AccessResult::NotEnabled;
        case AccessAction::SetValue:
            if (a.role != Role::Slider || !a.hasRange) return AccessResult::NotSupported;
            if (!a.enabled) return AccessResult::NotEnabled;
            if (!std::isfinite(r.value) || r.value < a.min || r.value > a.max) return AccessResult::BadValue;
            return AccessResult::Done;
        case AccessAction::Expand:
        case AccessAction::Collapse:
            if (a.expanded == Tri::Unset) return AccessResult::NotSupported;
            return a.enabled ? AccessResult::Done : AccessResult::NotEnabled;
        case AccessAction::Select:
            if (!a.selectable) return AccessResult::NotSupported;
            return a.enabled ? AccessResult::Done : AccessResult::NotEnabled;
        case AccessAction::ScrollIntoView:
            return InScrollArea(w) ? AccessResult::Done : AccessResult::NotSupported;
        case AccessAction::Close:
            return dynamic_cast<const Sheet*>(&w) ? AccessResult::Done : AccessResult::NotSupported;
        case AccessAction::SetFocus:
            return a.focusable && w.FocusableNow() ? AccessResult::Done : AccessResult::NotSupported;
    }
    return AccessResult::NotSupported;
}

}  // namespace

AccessResult CheckAccess(const Screen& screen, int id, const AccessRequest& request) {
    const Widget* w = FindAccessible(screen, id);
    if (!w) return AccessResult::Gone;
    return Check(*w, w->Access(), request);
}

AccessResult PerformAccess(Screen& screen, int id, const AccessRequest& request) {
    Widget* w = FindAccessible(screen, id);
    if (!w) return AccessResult::Gone;
    const AccessResult checked = Check(*w, w->Access(), request);
    if (checked != AccessResult::Done) return checked;

    bool ok = false;
    // As an event: a callback that removes this widget, its page or its sheet is deferred until fn is over.
    screen.Perform([&] {
        switch (request.action) {
            case AccessAction::Invoke:
            case AccessAction::Toggle: ok = w->AccInvoke(); break;
            case AccessAction::SetValue: ok = w->AccSetValue(request.value); break;
            case AccessAction::Expand: ok = w->AccExpand(true); break;
            case AccessAction::Collapse: ok = w->AccExpand(false); break;
            case AccessAction::Select: ok = w->AccSelect(); break;
            case AccessAction::ScrollIntoView:
                for (Widget* a = w->Parent(); a && !ok; a = a->Parent()) {
                    if (auto* area = dynamic_cast<ScrollArea*>(a)) {
                        area->ScrollIntoView(w);
                        ok = true;
                    }
                }
                break;
            case AccessAction::Close:
                static_cast<Sheet*>(w)->Close();  // Check made sure it is a Sheet; Close is deferred by the Screen
                ok = true;
                break;
            case AccessAction::SetFocus:
                w->Focus();
                ok = true;
                break;
        }
    });
    return ok ? AccessResult::Done : AccessResult::NotEnabled;
}

// ---------------------------------------------------------------------------------- changes ---

unsigned DiffAccess(const AccessInfo& a, const AccessInfo& b) {
    unsigned changes = 0;
    if (a.name != b.name) changes |= kChangeName;
    if (a.value != b.value) changes |= kChangeValue;
    if (a.toggle != b.toggle) changes |= kChangeToggle;
    if (a.enabled != b.enabled) changes |= kChangeEnabled;
    if (a.expanded != b.expanded) changes |= kChangeExpanded;
    if (a.hasRange != b.hasRange || a.min != b.min || a.max != b.max || a.current != b.current) {
        changes |= kChangeRange;
    }
    if (a.selected != b.selected) changes |= kChangeSelected;
    if (a.help != b.help) changes |= kChangeHelp;
    return changes;
}

void AccessTracker::TakeChildren(Widget* parent, int parentId, Snapshot& out, std::vector<int>& childIds) const {
    std::vector<Widget*> kids = AccessChildren(screen_, parent);
    for (Widget* kid : kids) {
        Entry e;
        e.info = kid->Access();
        e.parent = parentId;
        childIds.push_back(kid->Id());
        TakeChildren(kid, kid->Id(), out, e.children);
        out.entries[kid->Id()] = std::move(e);
    }
}

AccessTracker::Snapshot AccessTracker::Take() const {
    Snapshot s;
    for (Widget* top : AccessChildren(screen_, nullptr)) {
        Entry e;
        e.info = top->Access();
        e.parent = 0;
        s.rootChildren.push_back(top->Id());
        TakeChildren(top, top->Id(), s, e.children);
        s.entries[top->Id()] = std::move(e);
    }
    const Widget* f = AccessFocus(screen_);
    s.focus = f ? f->Id() : 0;
    return s;
}

void AccessTracker::Forget() {
    last_ = Snapshot();
    primed_ = false;
}

void AccessTracker::Update() {
    if (updating_) return;
    Snapshot now = Take();
    if (!primed_) {
        last_ = std::move(now);
        primed_ = true;
        return;
    }

    // Collect first, report after: the observer may call back into the tree (a provider reads properties).
    struct Prop {
        int id;
        unsigned changes;
        AccessInfo before, after;
    };
    std::vector<Prop> props;
    std::vector<int> structure, live;
    if (now.rootChildren != last_.rootChildren) structure.push_back(0);
    for (const auto& [id, entry] : now.entries) {
        const auto old = last_.entries.find(id);
        if (old == last_.entries.end()) {
            // A new widget: its parent's list changed (reported there). A live region that comes with text is news.
            if (entry.info.liveRegion && !AccessName(entry.info).empty()) live.push_back(id);
            continue;
        }
        if (old->second.children != entry.children) structure.push_back(id);
        const unsigned changes = DiffAccess(old->second.info, entry.info);
        if (changes) {
            props.push_back({id, changes, old->second.info, entry.info});
            if (entry.info.liveRegion && (changes & (kChangeName | kChangeValue)) && !AccessName(entry.info).empty()) {
                live.push_back(id);
            }
        }
    }
    const int oldFocus = last_.focus;
    last_ = std::move(now);

    if (!observer_) return;
    updating_ = true;
    for (int id : structure) observer_->StructureChanged(id);
    for (const Prop& p : props) observer_->PropertiesChanged(p.id, p.changes, p.before, p.after);
    for (int id : live) observer_->LiveRegionChanged(id);
    if (last_.focus != oldFocus) observer_->FocusChanged(last_.focus);
    updating_ = false;
}

}  // namespace nui
