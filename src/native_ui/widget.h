#pragma once
// The base of every control of the native Settings window (K1.b, ADR-0009). Pure, Win32-free, in
// lexia_core: the widget tree is tested with tests/fake_canvas.h, no window.
//
// How it fits together
//   - A Widget has bounds RELATIVE to its parent (plus the parent's ChildOffset(), which a ScrollArea
//     uses to shift its content). Abs() is the rectangle in window DIPs; painting and hit testing use it.
//   - Screen (screen.h) owns the tree, turns mouse and key events into calls on widgets, and keeps the
//     hover / focus / capture / sheet state. A widget finds its Screen through its parent chain.
//   - Widgets never change what they show on a user action. A click on a switch calls its callback and
//     leaves the switch as it was; the page code sends a PageMessage, and the next `state` message
//     comes back as SetChecked(). That is the rule of the old ui/ ("the page keeps no copy of a
//     setting") and it keeps the host the one owner of every setting.
//   - Accessibility data (Access()) is plain data for the UI Automation task: role, name, value, range,
//     toggle, expanded, selected. The widget fills it; nothing here knows UI Automation.
#include "native_ui/animation.h"
#include "native_ui/canvas.h"
#include "native_ui/input.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace nui {

class Screen;

// ----------------------------------------------------------------------- accessibility data ---

enum class Role {
    None,
    Button,
    Link,
    Switch,
    Slider,
    ComboBox,
    ListItem,
    List,
    Text,
    Image,
    Group,
    Dialog,
    ProgressBar,
    KeyChip,  // a key on a tag: pressed to change, then takes the next key (the host maps it to a button)
    Edit,
    Pane,
};

enum class Toggle { None, Off, On };  // None = not a toggle at all
enum class Tri { Unset, No, Yes };    // for flags a control may not have (expanded)

struct AccessInfo {
    Role role = Role::Group;
    std::string name;       // what it is called (the label)
    std::string value;      // the value as the person reads it ("1.25x", "Anna", "F8")
    std::string help;       // longer help text (the tooltip, a note)
    Toggle toggle = Toggle::None;
    bool hasRange = false;
    double min = 0, max = 0, current = 0, step = 0;
    Tri expanded = Tri::Unset;  // a select with its list open
    bool selected = false;      // a list item, the current page's link
    bool selectable = false;    // it can be selected, so `selected` means something (a list item, a nav link)
    bool liveRegion = false;    // changes of the text are announced (the result notice)
    bool enabled = true;
    bool focusable = false;
    bool focused = false;
};

// Colour roles of the palette, so a widget can be told "draw in accent" without holding a palette.
enum class ColourRole { Ink, Accent, Danger, Line, OnAccent, Paper, Card };
Colour Resolve(const Palette& p, ColourRole role);

// ---------------------------------------------------------------------------------- Widget ---

class Widget {
public:
    Widget();
    virtual ~Widget();
    Widget(const Widget&) = delete;
    Widget& operator=(const Widget&) = delete;

    // A number that stays the same for the widget's life (the UI Automation task uses it as the
    // runtime id). Assigned from a counter; a page may set its own with SetId.
    int Id() const { return id_; }
    void SetId(int id) { id_ = id; }

    // ---- tree
    Widget* Parent() const { return parent_; }
    const std::vector<std::unique_ptr<Widget>>& Children() const { return children_; }
    Widget* AddChild(std::unique_ptr<Widget> child);
    template <class T, class... Args>
    T* Add(Args&&... args) {
        auto owned = std::make_unique<T>(std::forward<Args>(args)...);
        T* raw = owned.get();
        AddChild(std::move(owned));
        return raw;
    }
    // Removing destroys the widget. Do not remove a widget from inside one of its own callbacks (a
    // click handler): use Screen::Defer for that. The Screen forgets hover / focus / capture that
    // pointed into the removed part.
    void RemoveChild(Widget* child);
    void ClearChildren();
    Screen* GetScreen() const;

    // ---- geometry
    RectF Bounds() const { return bounds_; }
    void SetBounds(const RectF& b);
    void SetPosition(float x, float y) { SetBounds({x, y, bounds_.w, bounds_.h}); }
    void SetSize(float w, float h) { SetBounds({bounds_.x, bounds_.y, w, h}); }
    RectF Abs() const;  // in window DIPs
    // The wanted size for a given width (text widgets wrap). The default is the size it has now. The
    // page (or a Sheet) places the widget with SetBounds afterwards.
    virtual SizeF Measure(Canvas& canvas, float maxWidth);

    // ---- flags
    bool Visible() const { return visible_; }
    void SetVisible(bool v);
    bool Enabled() const { return enabled_; }
    void SetEnabled(bool e);
    bool Hovered() const { return hovered_; }
    // A hook for the preview picture tool (lexia_settingsshot): shows the hovered look without a pointer,
    // so one picture can hold several hovered widgets. The Screen's own hover tracking does not use it.
    void PreviewHover(bool h) { SetHovered(h); }
    bool Focused() const { return focused_; }
    // Can the Tab key land here now? A native disabled button cannot be focused; an icon button or a
    // switch that explains itself with a tooltip (aria-disabled in the old page) keeps the focus.
    bool FocusableNow() const { return visible_ && focusable_ && (enabled_ || focusWhenDisabled_); }
    void SetFocusable(bool f) { focusable_ = f; }
    void SetFocusWhenDisabled(bool f) { focusWhenDisabled_ = f; }
    void Focus();  // asks the Screen to move the focus here
    void SetTooltip(std::string text) { tooltip_ = std::move(text); }
    const std::string& Tooltip() const { return tooltip_; }

    // ---- accessibility
    void SetRole(Role r) { role_ = r; }
    void SetName(std::string n) { name_ = std::move(n); }
    void SetHelp(std::string h) { help_ = std::move(h); }
    void SetValueText(std::string v) { valueText_ = std::move(v); }
    void SetLiveRegion(bool live) { liveRegion_ = live; }
    virtual AccessInfo Access() const;
    // A stable name for the automation id of the widget (such as "home.power"), set by the page. Empty = the
    // id is made from the widget's place in the tree (access_tree.h). Unique within the window.
    void SetAutomationId(std::string id) { automationId_ = std::move(id); }
    const std::string& AutomationId() const { return automationId_; }
    // A widget that only repeats what another one says (the word beside a slider, the title above a card of the
    // same name) is not offered to a screen reader: it would be read twice. It is still drawn and still in the tree.
    void SetAccessHidden(bool hidden) { accessHidden_ = hidden; }
    bool AccessHidden() const { return accessHidden_; }

    // ---- accessibility actions (K.5). What a screen reader asks for through UI Automation (Invoke,
    // Toggle, SetValue, Expand / Collapse, Select). Each does what a click or a key would do and returns
    // false when the widget does not do that, or cannot now (disabled). They run through PerformAccess
    // (access_tree.h), which treats the call as an event, so a callback may remove widgets as in a click.
    virtual bool AccInvoke() { return false; }
    virtual bool AccSetValue(double) { return false; }
    virtual bool AccExpand(bool) { return false; }
    virtual bool AccSelect() { return false; }

    // ---- painting. Paint draws the widget itself; PaintChildren its children (a ScrollArea clips
    // them); PaintTree is both. Nothing is drawn for a hidden widget.
    virtual void Paint(Canvas& canvas, const Palette& palette);
    virtual void PaintChildren(Canvas& canvas, const Palette& palette);
    void PaintTree(Canvas& canvas, const Palette& palette);

    // ---- input. The Screen calls these. OnMouse returns true when the event was used (a Wheel that is
    // not used goes on to the parent). OnKey returns true when used; unused keys go to the parent.
    virtual Widget* HitTest(PointF p);  // the deepest visible widget at p, or null
    virtual bool OnMouse(const MouseEvent& ev);
    virtual bool OnKey(const KeyEvent& ev);
    virtual bool CapturesAllKeys() const { return false; }  // a key chip waiting for its key
    virtual CursorHint Cursor() const { return CursorHint::Arrow; }

    // ---- hooks of the Screen
    virtual void PrepareLayout(Canvas& canvas);        // Sheets lay out here, before painting
    virtual void OnDescendantFocused(Widget* w);       // a ScrollArea scrolls it into view
    virtual void OnPopupClosed();                      // the owner of a popup (a Select)

    // The shift the children get inside this widget (a ScrollArea returns minus its offset).
    virtual PointF ChildOffset() const { return {0, 0}; }

    // The 0..2 look amount of the state transition (0 idle, 1 hover, 2 pressed) at the Screen's time.
    float VisualAmount() const;

protected:
    // The target of the state transition: widgets say which look they should have now.
    virtual float VisualTarget() const;
    // A flag changed (hovered, focused, enabled, focus-visible): re-aim the transition and redraw.
    virtual void StateChanged();
    void SyncVisual();
    void Invalidate();
    double Now() const;
    bool Animate() const;  // transitions on? (after the first draw, reduced motion off)
    // Moves a Tween to a target at the Screen's time and keeps the Screen ticking until it ends.
    void MoveTween(Tween& t, float target);
    float TweenValue(const Tween& t) const;
    bool RingVisible() const;  // focused and the focus came by keyboard

    bool hovered_ = false;
    bool focused_ = false;
    bool enabled_ = true;
    Role role_ = Role::Group;
    std::string name_, help_, valueText_;
    bool liveRegion_ = false;
    AccessInfo BaseAccess() const;

private:
    friend class Screen;
    void SetHovered(bool h);
    void SetFocused(bool f);

    int id_;
    Widget* parent_ = nullptr;
    Screen* ownerScreen_ = nullptr;  // set only on the two roots a Screen makes; the rest find it upwards
    std::vector<std::unique_ptr<Widget>> children_;
    RectF bounds_;
    bool visible_ = true;
    bool focusable_ = false;
    bool focusWhenDisabled_ = false;
    std::string tooltip_;
    std::string automationId_;
    bool accessHidden_ = false;
    Tween visual_;
};

// The focus ring of the design: 2 DIP accent line, 2 DIP outside the widget (`outline-offset: 2px`).
void DrawFocusRing(Canvas& canvas, const Palette& palette, const RectF& around, float radius);

// Draws with an opacity for the life of the object (the disabled look of several controls).
class OpacityScope {
public:
    OpacityScope(Canvas& canvas, float opacity) : canvas_(canvas), on_(opacity < 1.0f) {
        if (on_) canvas_.PushOpacity(opacity);
    }
    ~OpacityScope() {
        if (on_) canvas_.PopOpacity();
    }
    OpacityScope(const OpacityScope&) = delete;
    OpacityScope& operator=(const OpacityScope&) = delete;

private:
    Canvas& canvas_;
    bool on_;
};

}  // namespace nui
