#pragma once
// The accessible view of the widget tree (K.5, ADR-0009). Pure and Win32-free, in lexia_core: the UI Automation
// providers (uia_provider.*, lexia.exe) are thin glue over it, and the tests drive it with a FakeCanvas.
//
// What a screen reader sees is a PROJECTION of the widget tree:
//   - A widget is offered when it is visible and has something to say (IsAccessible): a control, a named group,
//     a text with words, a named picture. Plain panels, the scroll areas and the pictures without a name are
//     not offered; their offered children move up to the nearest offered ancestor.
//   - The window root holds the page and the layers (a sheet, the open list of a Select). While a modal sheet
//     is open the page beneath, and any sheet under the top one, is not offered (aria-modal).
//   - The accessibility focus is the focused widget, or, while a Select's list is open, the row the arrow
//     keys are on (the Screen keeps the real focus on the Select).
//
// What can change is reported by AccessTracker: it keeps a snapshot of the projection and, when Update() is
// called after an event, tells its observer what differs (focus, properties, structure, live text). It never
// reports in the middle of a tree change, because Update() is called only between events.
//
// The actions a screen reader can ask for (Invoke, Toggle, SetValue, Expand, Select, ScrollIntoView, Close,
// SetFocus) are CheckAccess (can it be done now?) and PerformAccess (do it, as an event).
#include "native_ui/screen.h"
#include "native_ui/widget.h"

#include <map>
#include <string>
#include <vector>

namespace nui {

// ---------------------------------------------------------------------------- the projection ---

// Does the widget appear to a screen reader? (Its visibility and its ancestors' are the caller's: the walks
// below only enter visible widgets.)
bool IsAccessible(const Widget& w);

// The offered children of `parent`; null = the window root (the page and the layers).
std::vector<Widget*> AccessChildren(const Screen& screen, const Widget* parent);
// The nearest offered ancestor; null = the window root.
Widget* AccessParent(const Screen& screen, const Widget& w);
// The offered widget with this Widget::Id(), or null when there is none now (removed, hidden, under a sheet).
Widget* FindAccessible(const Screen& screen, int id);
// The widget that has the keyboard focus for a screen reader (see the top of the file); null = none.
Widget* AccessFocus(const Screen& screen);
// The offered widget under the point (window DIPs), or null.
Widget* AccessibleAt(const Screen& screen, PointF p);

// The name a screen reader reads: the widget's name; for a text also its words (a notice: lead and text).
std::string AccessName(const AccessInfo& info);
// A stable string for the widget: its own (Widget::AutomationId) or one made from its place in the tree
// ("home.pane[0]/button[1]"). Unique among the offered widgets.
std::string AutomationIdOf(const Screen& screen, const Widget& w);

// Where the widget is, in window DIPs, clipped to the scroll areas it sits in and to the window; `offscreen` when
// nothing of it shows (scrolled out).
struct AccessBounds {
    RectF rect;
    bool offscreen = false;
};
AccessBounds BoundsOf(const Screen& screen, const Widget& w);
// Is the widget inside a ScrollArea (so ScrollIntoView means something)?
bool InScrollArea(const Widget& w);

// ---------------------------------------------------------------------------------- actions ---

enum class AccessAction { Invoke, Toggle, SetValue, Expand, Collapse, Select, ScrollIntoView, Close, SetFocus };

struct AccessRequest {
    AccessAction action = AccessAction::Invoke;
    double value = 0;  // SetValue
};

enum class AccessResult {
    Done,
    Gone,          // no such offered widget now (UIA_E_ELEMENTNOTAVAILABLE)
    NotEnabled,    // the widget is disabled (UIA_E_ELEMENTNOTENABLED)
    NotSupported,  // the widget does not do that (UIA_E_NOTSUPPORTED)
    BadValue,      // SetValue outside the range (E_INVALIDARG)
};

// Can it be done now? Changes nothing: a provider asks this before it queues the action.
AccessResult CheckAccess(const Screen& screen, int id, const AccessRequest& request);
// Does it, as an event (Screen::Perform), on the widget with this id; the widget is looked up again, so an
// action that was queued for a widget that has gone since returns Gone.
AccessResult PerformAccess(Screen& screen, int id, const AccessRequest& request);

// ---------------------------------------------------------------------------------- changes ---

// What differs between two AccessInfo of the same widget.
enum AccessChange : unsigned {
    kChangeName = 1,
    kChangeValue = 2,     // the value text
    kChangeToggle = 4,
    kChangeEnabled = 8,
    kChangeExpanded = 16,
    kChangeRange = 32,    // current, minimum or maximum
    kChangeSelected = 64,
    kChangeHelp = 128,
};
unsigned DiffAccess(const AccessInfo& before, const AccessInfo& after);

class AccessObserver {
public:
    virtual ~AccessObserver() = default;
    // The keyboard focus (for a screen reader) moved to the widget; 0 = to nothing.
    virtual void FocusChanged(int /*widgetId*/) {}
    virtual void PropertiesChanged(int /*widgetId*/, unsigned /*changes*/, const AccessInfo& /*before*/,
                                   const AccessInfo& /*after*/) {}
    // The offered children of this widget changed (a page, a sheet or a row came or went); 0 = the window.
    virtual void StructureChanged(int /*parentId*/) {}
    // The text of a live region changed, or the region appeared with text.
    virtual void LiveRegionChanged(int /*widgetId*/) {}
};

class AccessTracker {
public:
    explicit AccessTracker(const Screen& screen) : screen_(screen) {}
    void SetObserver(AccessObserver* observer) { observer_ = observer; }

    // Compares the projection with the last one and tells the observer. The first call (and the first after
    // Forget) only takes the snapshot. Call it between events, never inside a widget callback.
    void Update();
    // Drops the snapshot: the next Update starts again without reporting (nobody was listening meanwhile).
    void Forget();

private:
    struct Entry {
        AccessInfo info;
        int parent = 0;
        std::vector<int> children;
    };
    struct Snapshot {
        std::map<int, Entry> entries;
        std::vector<int> rootChildren;
        int focus = 0;
    };
    Snapshot Take() const;
    void TakeChildren(Widget* parent, int parentId, Snapshot& out, std::vector<int>& childIds) const;

    const Screen& screen_;
    AccessObserver* observer_ = nullptr;
    Snapshot last_;
    bool primed_ = false;
    bool updating_ = false;  // an observer must not make Update run again
};

}  // namespace nui
