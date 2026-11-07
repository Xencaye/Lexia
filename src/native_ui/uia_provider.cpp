#include "native_ui/uia_provider.h"

#include <windows.h>

#include <UIAutomationClient.h>  // the UIA_*Id constants (the IUIAutomation client headers define them)
#include <UIAutomationCore.h>
#include <UIAutomationCoreApi.h>
#include <oleauto.h>

#include <cmath>
#include <deque>
#include <memory>
#include <vector>

#include "common.h"
#include "log.h"
#include "native_ui/access_tree.h"
#include "native_ui/screen.h"
#include "strings/string_ids.h"
#include "strings/strings.h"

// ---------------------------------------------------------------------------------- context ---

// The state the providers share with the window. The window's UiaHost owns it through a shared_ptr and every
// element holds one too, so an element that a screen reader keeps outlives the window safely: it only finds
// `alive` false.
struct UiaHost::Context {
    HWND hwnd = nullptr;
    nui::Screen* screen = nullptr;  // null once detached; only the UI thread reads it
    UINT dpi = 96;
    DWORD thread = 0;  // the thread that made the window: the only one that may touch the tree
    bool alive = true;
    std::string language;

    struct Pending {
        int id = 0;
        nui::AccessRequest request;
    };
    std::deque<Pending> pending;  // UI thread only
};

namespace {

using ContextPtr = std::shared_ptr<UiaHost::Context>;

// Why a call failed, as UI Automation says it.
HRESULT FromResult(nui::AccessResult r) {
    switch (r) {
        case nui::AccessResult::Done: return S_OK;
        case nui::AccessResult::Gone: return UIA_E_ELEMENTNOTAVAILABLE;
        case nui::AccessResult::NotEnabled: return UIA_E_ELEMENTNOTENABLED;
        case nui::AccessResult::NotSupported: return UIA_E_NOTSUPPORTED;
        case nui::AccessResult::BadValue: return E_INVALIDARG;
    }
    return E_FAIL;
}

// The control type for a role. A switch is a Button with the Toggle pattern: UI Automation has no switch type
// (WinUI's ToggleSwitch does the same), and CheckBox would make a screen reader say "check box", which is
// not what the person sees. The localized control type says "switch" (strings uia.switch).
CONTROLTYPEID ControlTypeOf(nui::Role role) {
    switch (role) {
        case nui::Role::Button: return UIA_ButtonControlTypeId;
        case nui::Role::Link: return UIA_HyperlinkControlTypeId;
        case nui::Role::Switch: return UIA_ButtonControlTypeId;
        case nui::Role::Slider: return UIA_SliderControlTypeId;
        case nui::Role::ComboBox: return UIA_ComboBoxControlTypeId;
        case nui::Role::ListItem: return UIA_ListItemControlTypeId;
        case nui::Role::List: return UIA_ListControlTypeId;
        case nui::Role::Text: return UIA_TextControlTypeId;
        case nui::Role::Image: return UIA_ImageControlTypeId;
        case nui::Role::Group: return UIA_GroupControlTypeId;
        case nui::Role::Dialog: return UIA_WindowControlTypeId;  // with IsDialog, like WinUI's ContentDialog
        case nui::Role::ProgressBar: return UIA_ProgressBarControlTypeId;
        case nui::Role::KeyChip: return UIA_ButtonControlTypeId;
        case nui::Role::Edit: return UIA_EditControlTypeId;
        case nui::Role::Pane: return UIA_PaneControlTypeId;
        case nui::Role::None: break;
    }
    return UIA_GroupControlTypeId;
}

BSTR ToBstr(const std::string& utf8) {
    const std::wstring w = Utf8ToWide(utf8);
    return SysAllocStringLen(w.c_str(), static_cast<UINT>(w.size()));
}

void SetBstr(VARIANT* v, const std::string& utf8) {
    v->vt = VT_BSTR;
    v->bstrVal = ToBstr(utf8);
}
void SetI4(VARIANT* v, long value) {
    v->vt = VT_I4;
    v->lVal = value;
}
void SetBool(VARIANT* v, bool value) {
    v->vt = VT_BOOL;
    v->boolVal = value ? VARIANT_TRUE : VARIANT_FALSE;
}
void SetR8(VARIANT* v, double value) {
    v->vt = VT_R8;
    v->dblVal = value;
}

// ---------------------------------------------------------------------------------- Element ---

// One UI Automation element: the window root (id 0) or a widget (its Widget::Id()). All the interfaces live in
// one object; GetPatternProvider offers only the ones the widget's role and state allow. It holds the id, never
// a pointer into the tree.
class Element : public IRawElementProviderSimple,
                public IRawElementProviderFragment,
                public IRawElementProviderFragmentRoot,
                public IInvokeProvider,
                public IToggleProvider,
                public IRangeValueProvider,
                public IValueProvider,
                public IExpandCollapseProvider,
                public ISelectionProvider,
                public ISelectionItemProvider,
                public IScrollItemProvider,
                public IWindowProvider {
public:
    Element(ContextPtr ctx, int id) : ctx_(std::move(ctx)), id_(id) {}

    // ---- IUnknown
    STDMETHODIMP_(ULONG) AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&refs_)); }
    STDMETHODIMP_(ULONG) Release() override {
        const ULONG n = static_cast<ULONG>(InterlockedDecrement(&refs_));
        if (n == 0) delete this;
        return n;
    }
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IRawElementProviderSimple)) {
            *ppv = static_cast<IRawElementProviderSimple*>(this);
        } else if (riid == __uuidof(IRawElementProviderFragment)) {
            *ppv = static_cast<IRawElementProviderFragment*>(this);
        } else if (riid == __uuidof(IRawElementProviderFragmentRoot) && id_ == 0) {
            *ppv = static_cast<IRawElementProviderFragmentRoot*>(this);
        } else if (riid == __uuidof(IInvokeProvider)) {
            *ppv = static_cast<IInvokeProvider*>(this);
        } else if (riid == __uuidof(IToggleProvider)) {
            *ppv = static_cast<IToggleProvider*>(this);
        } else if (riid == __uuidof(IRangeValueProvider)) {
            *ppv = static_cast<IRangeValueProvider*>(this);
        } else if (riid == __uuidof(IValueProvider)) {
            *ppv = static_cast<IValueProvider*>(this);
        } else if (riid == __uuidof(IExpandCollapseProvider)) {
            *ppv = static_cast<IExpandCollapseProvider*>(this);
        } else if (riid == __uuidof(ISelectionProvider)) {
            *ppv = static_cast<ISelectionProvider*>(this);
        } else if (riid == __uuidof(ISelectionItemProvider)) {
            *ppv = static_cast<ISelectionItemProvider*>(this);
        } else if (riid == __uuidof(IScrollItemProvider)) {
            *ppv = static_cast<IScrollItemProvider*>(this);
        } else if (riid == __uuidof(IWindowProvider)) {
            *ppv = static_cast<IWindowProvider*>(this);
        } else {
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }

    // ---- IRawElementProviderSimple
    STDMETHODIMP get_ProviderOptions(ProviderOptions* pRetVal) override {
        if (!pRetVal) return E_POINTER;
        // UseComThreading: UI Automation calls us on the thread that made the element (the window's).
        *pRetVal = static_cast<ProviderOptions>(ProviderOptions_ServerSideProvider | ProviderOptions_UseComThreading);
        return S_OK;
    }

    STDMETHODIMP GetPatternProvider(PATTERNID patternId, IUnknown** pRetVal) override {
        if (!pRetVal) return E_POINTER;
        *pRetVal = nullptr;
        Target t;
        if (const HRESULT hr = Lookup(t); FAILED(hr)) return hr;
        if (id_ == 0) return S_OK;  // the host window offers the window patterns
        const nui::AccessInfo& a = t.info;
        IUnknown* offer = nullptr;
        switch (patternId) {
            case UIA_InvokePatternId:
                // A power button also toggles; it keeps Invoke so "press" works for a tool that only knows it.
                if (a.role == nui::Role::Button || a.role == nui::Role::Link || a.role == nui::Role::KeyChip) {
                    offer = static_cast<IInvokeProvider*>(this);
                }
                break;
            case UIA_TogglePatternId:
                if (a.toggle != nui::Toggle::None) offer = static_cast<IToggleProvider*>(this);
                break;
            case UIA_RangeValuePatternId:
                if (a.hasRange) offer = static_cast<IRangeValueProvider*>(this);
                break;
            case UIA_ValuePatternId:
                if (a.role == nui::Role::ComboBox || a.role == nui::Role::KeyChip) {
                    offer = static_cast<IValueProvider*>(this);
                }
                break;
            case UIA_ExpandCollapsePatternId:
                if (a.expanded != nui::Tri::Unset) offer = static_cast<IExpandCollapseProvider*>(this);
                break;
            case UIA_SelectionPatternId:
                if (a.role == nui::Role::ComboBox || a.role == nui::Role::List) {
                    offer = static_cast<ISelectionProvider*>(this);
                }
                break;
            case UIA_SelectionItemPatternId:
                if (a.selectable) offer = static_cast<ISelectionItemProvider*>(this);
                break;
            case UIA_ScrollItemPatternId:
                if (nui::InScrollArea(*t.widget)) offer = static_cast<IScrollItemProvider*>(this);
                break;
            case UIA_WindowPatternId:
                if (a.role == nui::Role::Dialog) offer = static_cast<IWindowProvider*>(this);
                break;
            default:
                break;
        }
        if (offer) {
            offer->AddRef();
            *pRetVal = offer;
        }
        return S_OK;
    }

    STDMETHODIMP GetPropertyValue(PROPERTYID propertyId, VARIANT* pRetVal) override {
        if (!pRetVal) return E_POINTER;
        VariantInit(pRetVal);  // VT_EMPTY: "not provided here", UI Automation asks the next provider
        Target t;
        if (const HRESULT hr = Lookup(t); FAILED(hr)) return hr;
        if (id_ == 0) return RootProperty(propertyId, pRetVal);

        const nui::AccessInfo& a = t.info;
        switch (propertyId) {
            case UIA_ControlTypePropertyId: SetI4(pRetVal, ControlTypeOf(a.role)); break;
            case UIA_LocalizedControlTypePropertyId:
                if (a.role == nui::Role::Switch) {
                    SetBstr(pRetVal, strings::Text(ctx_->language, strings::id::kUiaSwitch));
                }
                break;
            case UIA_NamePropertyId: SetBstr(pRetVal, nui::AccessName(a)); break;
            case UIA_HelpTextPropertyId:
                if (!a.help.empty()) SetBstr(pRetVal, a.help);
                break;
            case UIA_IsEnabledPropertyId: SetBool(pRetVal, a.enabled); break;
            case UIA_IsKeyboardFocusablePropertyId: SetBool(pRetVal, a.focusable); break;
            case UIA_HasKeyboardFocusPropertyId: SetBool(pRetVal, HasFocus(t)); break;
            case UIA_AutomationIdPropertyId: SetBstr(pRetVal, nui::AutomationIdOf(*t.screen, *t.widget)); break;
            case UIA_FrameworkIdPropertyId: SetBstr(pRetVal, "Lexia"); break;
            case UIA_ClassNamePropertyId: SetBstr(pRetVal, "LexiaWidget"); break;
            case UIA_IsControlElementPropertyId:
            case UIA_IsContentElementPropertyId: SetBool(pRetVal, true); break;
            case UIA_IsPasswordPropertyId: SetBool(pRetVal, false); break;
            case UIA_IsOffscreenPropertyId: SetBool(pRetVal, nui::BoundsOf(*t.screen, *t.widget).offscreen); break;
            case UIA_BoundingRectanglePropertyId: return BoundsVariant(t, pRetVal);
            case UIA_LiveSettingPropertyId:
                if (a.liveRegion) SetI4(pRetVal, Polite);
                break;
            case UIA_IsDialogPropertyId:
                if (a.role == nui::Role::Dialog) SetBool(pRetVal, true);
                break;
            case UIA_ProviderDescriptionPropertyId: SetBstr(pRetVal, "Lexia Settings window"); break;
            default: break;
        }
        return S_OK;
    }

    STDMETHODIMP get_HostRawElementProvider(IRawElementProviderSimple** pRetVal) override {
        if (!pRetVal) return E_POINTER;
        *pRetVal = nullptr;
        if (id_ != 0) return S_OK;
        if (!ctx_->alive) return UIA_E_ELEMENTNOTAVAILABLE;
        return UiaHostProviderFromHwnd(ctx_->hwnd, pRetVal);  // the window's own provider: title, window patterns
    }

    // ---- IRawElementProviderFragment
    STDMETHODIMP Navigate(NavigateDirection direction, IRawElementProviderFragment** pRetVal) override {
        if (!pRetVal) return E_POINTER;
        *pRetVal = nullptr;
        Target t;
        if (const HRESULT hr = Lookup(t); FAILED(hr)) return hr;

        nui::Widget* result = nullptr;
        bool toRoot = false;
        switch (direction) {
            case NavigateDirection_Parent:
                if (id_ == 0) return S_OK;  // the host window is the root's parent
                result = nui::AccessParent(*t.screen, *t.widget);
                toRoot = result == nullptr;
                break;
            case NavigateDirection_FirstChild:
            case NavigateDirection_LastChild: {
                const std::vector<nui::Widget*> kids = nui::AccessChildren(*t.screen, t.widget);
                if (!kids.empty()) result = direction == NavigateDirection_FirstChild ? kids.front() : kids.back();
                break;
            }
            case NavigateDirection_NextSibling:
            case NavigateDirection_PreviousSibling: {
                if (id_ == 0) return S_OK;
                const std::vector<nui::Widget*> sibs =
                    nui::AccessChildren(*t.screen, nui::AccessParent(*t.screen, *t.widget));
                for (size_t i = 0; i < sibs.size(); ++i) {
                    if (sibs[i] != t.widget) continue;
                    if (direction == NavigateDirection_NextSibling && i + 1 < sibs.size()) result = sibs[i + 1];
                    if (direction == NavigateDirection_PreviousSibling && i > 0) result = sibs[i - 1];
                    break;
                }
                break;
            }
        }
        if (toRoot) {
            *pRetVal = static_cast<IRawElementProviderFragment*>(new Element(ctx_, 0));
        } else if (result) {
            *pRetVal = static_cast<IRawElementProviderFragment*>(new Element(ctx_, result->Id()));
        }
        return S_OK;
    }

    STDMETHODIMP GetRuntimeId(SAFEARRAY** pRetVal) override {
        if (!pRetVal) return E_POINTER;
        *pRetVal = nullptr;
        if (id_ == 0) return S_OK;  // the host window supplies the root's id; UiaAppendRuntimeId chains ours to it
        SAFEARRAY* sa = SafeArrayCreateVector(VT_I4, 0, 2);
        if (!sa) return E_OUTOFMEMORY;
        LONG i = 0;
        int values[2] = {UiaAppendRuntimeId, id_};
        for (i = 0; i < 2; ++i) SafeArrayPutElement(sa, &i, &values[i]);
        *pRetVal = sa;
        return S_OK;
    }

    STDMETHODIMP get_BoundingRectangle(UiaRect* pRetVal) override {
        if (!pRetVal) return E_POINTER;
        *pRetVal = UiaRect{};
        Target t;
        if (const HRESULT hr = Lookup(t); FAILED(hr)) return hr;
        *pRetVal = ScreenRect(t);
        return S_OK;
    }

    STDMETHODIMP GetEmbeddedFragmentRoots(SAFEARRAY** pRetVal) override {
        if (!pRetVal) return E_POINTER;
        *pRetVal = nullptr;
        return S_OK;
    }

    STDMETHODIMP SetFocus() override {
        if (id_ == 0) return S_OK;
        return Queue(nui::AccessAction::SetFocus);
    }

    STDMETHODIMP get_FragmentRoot(IRawElementProviderFragmentRoot** pRetVal) override {
        if (!pRetVal) return E_POINTER;
        *pRetVal = nullptr;
        if (!ctx_->alive) return UIA_E_ELEMENTNOTAVAILABLE;
        *pRetVal = static_cast<IRawElementProviderFragmentRoot*>(new Element(ctx_, 0));
        return S_OK;
    }

    // ---- IRawElementProviderFragmentRoot
    STDMETHODIMP ElementProviderFromPoint(double x, double y, IRawElementProviderFragment** pRetVal) override {
        if (!pRetVal) return E_POINTER;
        *pRetVal = nullptr;
        Target t;
        if (const HRESULT hr = Lookup(t); FAILED(hr)) return hr;
        POINT p = {static_cast<LONG>(std::lround(x)), static_cast<LONG>(std::lround(y))};
        ScreenToClient(ctx_->hwnd, &p);
        const float k = 96.0f / static_cast<float>(ctx_->dpi);
        nui::Widget* hit = nui::AccessibleAt(*t.screen, {static_cast<float>(p.x) * k, static_cast<float>(p.y) * k});
        if (hit) *pRetVal = static_cast<IRawElementProviderFragment*>(new Element(ctx_, hit->Id()));
        return S_OK;  // null = nothing of ours here: the root answers
    }

    STDMETHODIMP GetFocus(IRawElementProviderFragment** pRetVal) override {
        if (!pRetVal) return E_POINTER;
        *pRetVal = nullptr;
        Target t;
        if (const HRESULT hr = Lookup(t); FAILED(hr)) return hr;
        if (nui::Widget* f = nui::AccessFocus(*t.screen)) {
            *pRetVal = static_cast<IRawElementProviderFragment*>(new Element(ctx_, f->Id()));
        }
        return S_OK;
    }

    // ---- IInvokeProvider
    STDMETHODIMP Invoke() override { return Queue(nui::AccessAction::Invoke); }

    // ---- IToggleProvider
    STDMETHODIMP Toggle() override { return Queue(nui::AccessAction::Toggle); }
    STDMETHODIMP get_ToggleState(ToggleState* pRetVal) override {
        if (!pRetVal) return E_POINTER;
        Target t;
        if (const HRESULT hr = Lookup(t); FAILED(hr)) return hr;
        *pRetVal = t.info.toggle == nui::Toggle::On ? ToggleState_On : ToggleState_Off;
        return S_OK;
    }

    // ---- IRangeValueProvider (sliders and the progress bar)
    STDMETHODIMP SetValue(double value) override { return Queue(nui::AccessAction::SetValue, value); }
    STDMETHODIMP get_Value(double* pRetVal) override { return Range(pRetVal, 0); }
    STDMETHODIMP get_Maximum(double* pRetVal) override { return Range(pRetVal, 1); }
    STDMETHODIMP get_Minimum(double* pRetVal) override { return Range(pRetVal, 2); }
    STDMETHODIMP get_LargeChange(double* pRetVal) override { return Range(pRetVal, 3); }
    STDMETHODIMP get_SmallChange(double* pRetVal) override { return Range(pRetVal, 4); }

    // ---- IValueProvider (the Select's shown choice, a key chip's key: both read only) and
    // IRangeValueProvider share get_IsReadOnly; an element has one of the two patterns, never both.
    STDMETHODIMP SetValue(LPCWSTR) override {
        Target t;
        if (const HRESULT hr = Lookup(t); FAILED(hr)) return hr;
        return UIA_E_ELEMENTNOTENABLED;  // read only: a choice is made by Select, a key by pressing it
    }
    STDMETHODIMP get_Value(BSTR* pRetVal) override {
        if (!pRetVal) return E_POINTER;
        *pRetVal = nullptr;
        Target t;
        if (const HRESULT hr = Lookup(t); FAILED(hr)) return hr;
        *pRetVal = ToBstr(t.info.value);
        return *pRetVal ? S_OK : E_OUTOFMEMORY;
    }
    STDMETHODIMP get_IsReadOnly(BOOL* pRetVal) override {
        if (!pRetVal) return E_POINTER;
        Target t;
        if (const HRESULT hr = Lookup(t); FAILED(hr)) return hr;
        // A slider can be set while it is enabled; a progress bar, a Select and a chip never.
        *pRetVal = (t.info.role == nui::Role::Slider && t.info.enabled) ? FALSE : TRUE;
        return S_OK;
    }

    // ---- IExpandCollapseProvider
    STDMETHODIMP Expand() override { return Queue(nui::AccessAction::Expand); }
    STDMETHODIMP Collapse() override { return Queue(nui::AccessAction::Collapse); }
    STDMETHODIMP get_ExpandCollapseState(ExpandCollapseState* pRetVal) override {
        if (!pRetVal) return E_POINTER;
        Target t;
        if (const HRESULT hr = Lookup(t); FAILED(hr)) return hr;
        *pRetVal = t.info.expanded == nui::Tri::Yes ? ExpandCollapseState_Expanded : ExpandCollapseState_Collapsed;
        return S_OK;
    }

    // ---- ISelectionProvider (a Select, and the list it opens)
    STDMETHODIMP GetSelection(SAFEARRAY** pRetVal) override {
        if (!pRetVal) return E_POINTER;
        *pRetVal = nullptr;
        Target t;
        if (const HRESULT hr = Lookup(t); FAILED(hr)) return hr;
        // A Select has items only while its list is open; the list is a layer of the Screen, not its child.
        nui::Widget* list = nullptr;
        if (t.info.role == nui::Role::List) list = t.widget;
        else if (t.info.role == nui::Role::ComboBox && t.info.expanded == nui::Tri::Yes) list = t.screen->Popup();
        std::vector<nui::Widget*> chosen;
        if (list) {
            for (nui::Widget* row : nui::AccessChildren(*t.screen, list)) {
                if (row->Access().selected) chosen.push_back(row);
            }
        }
        SAFEARRAY* sa = SafeArrayCreateVector(VT_UNKNOWN, 0, static_cast<ULONG>(chosen.size()));
        if (!sa) return E_OUTOFMEMORY;
        for (LONG i = 0; i < static_cast<LONG>(chosen.size()); ++i) {
            Element* e = new Element(ctx_, chosen[static_cast<size_t>(i)]->Id());
            IUnknown* u = static_cast<IRawElementProviderSimple*>(e);
            SafeArrayPutElement(sa, &i, u);  // adds its own reference
            e->Release();
        }
        *pRetVal = sa;
        return S_OK;
    }
    STDMETHODIMP get_CanSelectMultiple(BOOL* pRetVal) override {
        if (!pRetVal) return E_POINTER;
        *pRetVal = FALSE;
        return S_OK;
    }
    STDMETHODIMP get_IsSelectionRequired(BOOL* pRetVal) override {
        if (!pRetVal) return E_POINTER;
        *pRetVal = TRUE;
        return S_OK;
    }

    // ---- ISelectionItemProvider (a row of the list, a key row, a link of the frame)
    STDMETHODIMP Select() override { return Queue(nui::AccessAction::Select); }
    STDMETHODIMP AddToSelection() override { return ChangeSelection(true); }
    STDMETHODIMP RemoveFromSelection() override { return ChangeSelection(false); }
    STDMETHODIMP get_IsSelected(BOOL* pRetVal) override {
        if (!pRetVal) return E_POINTER;
        Target t;
        if (const HRESULT hr = Lookup(t); FAILED(hr)) return hr;
        *pRetVal = t.info.selected ? TRUE : FALSE;
        return S_OK;
    }
    STDMETHODIMP get_SelectionContainer(IRawElementProviderSimple** pRetVal) override {
        if (!pRetVal) return E_POINTER;
        *pRetVal = nullptr;
        Target t;
        if (const HRESULT hr = Lookup(t); FAILED(hr)) return hr;
        nui::Widget* parent = nui::AccessParent(*t.screen, *t.widget);
        if (parent && parent->Access().role == nui::Role::List) {
            *pRetVal = static_cast<IRawElementProviderSimple*>(new Element(ctx_, parent->Id()));
        }
        return S_OK;
    }

    // ---- IScrollItemProvider
    STDMETHODIMP ScrollIntoView() override { return Queue(nui::AccessAction::ScrollIntoView); }

    // ---- IWindowProvider (a sheet)
    STDMETHODIMP SetVisualState(WindowVisualState state) override {
        Target t;
        if (const HRESULT hr = Lookup(t); FAILED(hr)) return hr;
        return state == WindowVisualState_Normal ? S_OK : UIA_E_INVALIDOPERATION;  // a sheet cannot be maximised
    }
    STDMETHODIMP Close() override { return Queue(nui::AccessAction::Close); }
    STDMETHODIMP WaitForInputIdle(int, BOOL* pRetVal) override {
        if (!pRetVal) return E_POINTER;
        *pRetVal = TRUE;  // there is no work queue to wait for: the sheet is ready when it exists
        return S_OK;
    }
    STDMETHODIMP get_CanMaximize(BOOL* pRetVal) override { return FixedBool(pRetVal, FALSE); }
    STDMETHODIMP get_CanMinimize(BOOL* pRetVal) override { return FixedBool(pRetVal, FALSE); }
    STDMETHODIMP get_IsModal(BOOL* pRetVal) override { return FixedBool(pRetVal, TRUE); }
    STDMETHODIMP get_IsTopmost(BOOL* pRetVal) override { return FixedBool(pRetVal, TRUE); }
    STDMETHODIMP get_WindowVisualState(WindowVisualState* pRetVal) override {
        if (!pRetVal) return E_POINTER;
        *pRetVal = WindowVisualState_Normal;
        return S_OK;
    }
    STDMETHODIMP get_WindowInteractionState(WindowInteractionState* pRetVal) override {
        if (!pRetVal) return E_POINTER;
        *pRetVal = WindowInteractionState_ReadyForUserInteraction;
        return S_OK;
    }

private:
    // The widget this element stands for, looked up now. `widget` is null for the root.
    struct Target {
        nui::Screen* screen = nullptr;
        nui::Widget* widget = nullptr;
        nui::AccessInfo info;
    };

    // Every entry point starts here. Fails with "not available" when the window is gone, on any thread but the
    // window's (the tree is not thread safe), or when the widget is gone, hidden, or under a modal sheet.
    HRESULT Lookup(Target& t) const {
        if (!ctx_->alive || !ctx_->screen) return UIA_E_ELEMENTNOTAVAILABLE;
        if (GetCurrentThreadId() != ctx_->thread) return UIA_E_ELEMENTNOTAVAILABLE;
        t.screen = ctx_->screen;
        if (id_ == 0) return S_OK;
        t.widget = nui::FindAccessible(*t.screen, id_);
        if (!t.widget) return UIA_E_ELEMENTNOTAVAILABLE;
        t.info = t.widget->Access();
        return S_OK;
    }

    // Checks the action now, and queues it for the window's message loop (see the header).
    HRESULT Queue(nui::AccessAction action, double value = 0) {
        Target t;
        if (const HRESULT hr = Lookup(t); FAILED(hr)) return hr;
        if (id_ == 0) return UIA_E_NOTSUPPORTED;
        nui::AccessRequest request;
        request.action = action;
        request.value = value;
        if (const HRESULT hr = FromResult(nui::CheckAccess(*t.screen, id_, request)); FAILED(hr)) return hr;
        const bool wasEmpty = ctx_->pending.empty();
        ctx_->pending.push_back({id_, request});
        if (wasEmpty) PostMessageW(ctx_->hwnd, UiaHost::kActionsMessage, 0, 0);
        return S_OK;
    }

    // AddToSelection / RemoveFromSelection for single selection: allowed only when it changes nothing.
    HRESULT ChangeSelection(bool select) {
        Target t;
        if (const HRESULT hr = Lookup(t); FAILED(hr)) return hr;
        if (!t.info.selectable) return UIA_E_NOTSUPPORTED;
        if (t.info.selected == select) return S_OK;
        return select ? Queue(nui::AccessAction::Select) : UIA_E_INVALIDOPERATION;  // one is always selected
    }

    HRESULT Range(double* out, int which) {
        if (!out) return E_POINTER;
        *out = 0;
        Target t;
        if (const HRESULT hr = Lookup(t); FAILED(hr)) return hr;
        if (!t.info.hasRange) return UIA_E_NOTSUPPORTED;
        switch (which) {
            case 0: *out = t.info.current; break;
            case 1: *out = t.info.max; break;
            case 2: *out = t.info.min; break;
            case 3: *out = t.info.step * 10; break;  // the PageUp / PageDown step of the slider
            default: *out = t.info.step; break;
        }
        return S_OK;
    }

    static HRESULT FixedBool(BOOL* out, BOOL value) {
        if (!out) return E_POINTER;
        *out = value;
        return S_OK;
    }

    // The widget's box in screen pixels: window DIPs x dpi / 96 + the client area's corner.
    UiaRect ScreenRect(const Target& t) const {
        POINT origin = {0, 0};
        ClientToScreen(ctx_->hwnd, &origin);
        const double k = static_cast<double>(ctx_->dpi) / 96.0;
        if (id_ == 0) {
            const nui::SizeF s = t.screen->Size();
            return UiaRect{static_cast<double>(origin.x), static_cast<double>(origin.y), s.w * k, s.h * k};
        }
        const nui::RectF r = nui::BoundsOf(*t.screen, *t.widget).rect;
        return UiaRect{origin.x + r.x * k, origin.y + r.y * k, r.w * k, r.h * k};
    }

    HRESULT BoundsVariant(const Target& t, VARIANT* out) const {
        const UiaRect r = ScreenRect(t);
        SAFEARRAY* sa = SafeArrayCreateVector(VT_R8, 0, 4);
        if (!sa) return E_OUTOFMEMORY;
        const double values[4] = {r.left, r.top, r.width, r.height};
        for (LONG i = 0; i < 4; ++i) {
            double v = values[i];
            SafeArrayPutElement(sa, &i, &v);
        }
        out->vt = VT_R8 | VT_ARRAY;
        out->parray = sa;
        return S_OK;
    }

    // Has the keyboard focus: this is the widget the focus is on AND the window has the real keyboard focus.
    bool HasFocus(const Target& t) const {
        if (::GetFocus() != ctx_->hwnd) return false;
        const nui::Widget* f = nui::AccessFocus(*t.screen);
        return f && f == t.widget;
    }

    // The root offers only what the host window cannot: the framework.
    HRESULT RootProperty(PROPERTYID propertyId, VARIANT* out) const {
        if (propertyId == UIA_FrameworkIdPropertyId) SetBstr(out, "Lexia");
        else if (propertyId == UIA_ProviderDescriptionPropertyId) SetBstr(out, "Lexia Settings window");
        return S_OK;
    }

    ~Element() = default;  // only Release deletes

    ContextPtr ctx_;
    int id_;
    LONG refs_ = 1;
};

// ---------------------------------------------------------------------------------- events ---

// Turns what AccessTracker found into UI Automation events. It runs inside UiaHost::Update, on the UI thread,
// between events: the elements it makes call back into the tree, which is safe then.
class EventRaiser : public nui::AccessObserver {
public:
    explicit EventRaiser(ContextPtr ctx) : ctx_(std::move(ctx)) {}

    void FocusChanged(int widgetId) override {
        if (widgetId == 0) return;  // focus left the window's widgets: the host window reports its own focus
        With(widgetId, [](IRawElementProviderSimple* p) { UiaRaiseAutomationEvent(p, UIA_AutomationFocusChangedEventId); });
    }

    void StructureChanged(int parentId) override {
        Element* e = new Element(ctx_, parentId);
        IRawElementProviderSimple* frag = e;
        UiaRaiseStructureChangedEvent(frag, StructureChangeType_ChildrenInvalidated, nullptr, 0);
        e->Release();
    }

    void LiveRegionChanged(int widgetId) override {
        With(widgetId, [](IRawElementProviderSimple* p) { UiaRaiseAutomationEvent(p, UIA_LiveRegionChangedEventId); });
    }

    void PropertiesChanged(int widgetId, unsigned changes, const nui::AccessInfo& before,
                           const nui::AccessInfo& after) override {
        With(widgetId, [&](IRawElementProviderSimple* p) {
            VARIANT o, n;
            auto raise = [&](PROPERTYID prop) {
                UiaRaiseAutomationPropertyChangedEvent(p, prop, o, n);
                VariantClear(&o);
                VariantClear(&n);
            };
            VariantInit(&o);
            VariantInit(&n);
            // A text's words are its name.
            if ((changes & (nui::kChangeName | nui::kChangeValue)) && after.role == nui::Role::Text) {
                SetBstr(&o, nui::AccessName(before));
                SetBstr(&n, nui::AccessName(after));
                raise(UIA_NamePropertyId);
            } else {
                if (changes & nui::kChangeName) {
                    SetBstr(&o, before.name);
                    SetBstr(&n, after.name);
                    raise(UIA_NamePropertyId);
                }
                if ((changes & nui::kChangeValue) &&
                    (after.role == nui::Role::ComboBox || after.role == nui::Role::KeyChip)) {
                    SetBstr(&o, before.value);
                    SetBstr(&n, after.value);
                    raise(UIA_ValueValuePropertyId);
                }
            }
            if (changes & nui::kChangeToggle) {
                SetI4(&o, before.toggle == nui::Toggle::On ? ToggleState_On : ToggleState_Off);
                SetI4(&n, after.toggle == nui::Toggle::On ? ToggleState_On : ToggleState_Off);
                raise(UIA_ToggleToggleStatePropertyId);
            }
            if (changes & nui::kChangeEnabled) {
                SetBool(&o, before.enabled);
                SetBool(&n, after.enabled);
                raise(UIA_IsEnabledPropertyId);
            }
            if (changes & nui::kChangeExpanded) {
                SetI4(&o, before.expanded == nui::Tri::Yes ? ExpandCollapseState_Expanded : ExpandCollapseState_Collapsed);
                SetI4(&n, after.expanded == nui::Tri::Yes ? ExpandCollapseState_Expanded : ExpandCollapseState_Collapsed);
                raise(UIA_ExpandCollapseExpandCollapseStatePropertyId);
            }
            if ((changes & nui::kChangeRange) && after.hasRange) {
                SetR8(&o, before.current);
                SetR8(&n, after.current);
                raise(UIA_RangeValueValuePropertyId);
            }
            if (changes & nui::kChangeSelected) {
                SetBool(&o, before.selected);
                SetBool(&n, after.selected);
                raise(UIA_SelectionItemIsSelectedPropertyId);
                if (after.selected) UiaRaiseAutomationEvent(p, UIA_SelectionItem_ElementSelectedEventId);
            }
            if (changes & nui::kChangeHelp) {
                SetBstr(&o, before.help);
                SetBstr(&n, after.help);
                raise(UIA_HelpTextPropertyId);
            }
        });
    }

private:
    template <class Fn>
    void With(int widgetId, Fn&& fn) {
        Element* e = new Element(ctx_, widgetId);
        IRawElementProviderSimple* simple = e;
        fn(simple);
        e->Release();
    }

    ContextPtr ctx_;
};

}  // namespace

// ------------------------------------------------------------------------------------ UiaHost ---

struct UiaHost::Impl {
    ContextPtr ctx;
    nui::AccessTracker tracker;
    EventRaiser raiser;
    Element* root = nullptr;  // made at the first WM_GETOBJECT; one reference is ours

    Impl(ContextPtr c, nui::Screen& screen) : ctx(c), tracker(screen), raiser(c) { tracker.SetObserver(&raiser); }
};

UiaHost::UiaHost(HWND hwnd, nui::Screen& screen, std::string interfaceLanguage) {
    auto ctx = std::make_shared<Context>();
    ctx->hwnd = hwnd;
    ctx->screen = &screen;
    ctx->thread = GetCurrentThreadId();
    ctx->language = std::move(interfaceLanguage);
    impl_ = std::make_unique<Impl>(ctx, screen);
}

UiaHost::~UiaHost() { Detach(); }

void UiaHost::SetDpi(UINT dpi) { impl_->ctx->dpi = dpi ? dpi : 96; }

LRESULT UiaHost::HandleGetObject(WPARAM wp, LPARAM lp) {
    if (!impl_->ctx->alive || static_cast<long>(lp) != static_cast<long>(UiaRootObjectId)) return 0;
    if (!impl_->root) impl_->root = new Element(impl_->ctx, 0);
    IRawElementProviderSimple* simple = impl_->root;
    return UiaReturnRawElementProvider(impl_->ctx->hwnd, wp, lp, simple);
}

void UiaHost::RunPendingActions() {
    Context& c = *impl_->ctx;
    if (!c.alive || !c.screen) return;
    // Take the list first: an action's callbacks may queue more (they run later, on the next message).
    std::deque<Context::Pending> todo;
    todo.swap(c.pending);
    for (const Context::Pending& p : todo) {
        // Looked up again: the widget may have gone since the call was made (a state or an earlier action).
        nui::PerformAccess(*c.screen, p.id, p.request);
    }
}

void UiaHost::Update() {
    Context& c = *impl_->ctx;
    if (!c.alive || !c.screen) return;
    // Costs nothing while no screen reader listens. The look is taken again when one starts: events that
    // happened before it listened are not news to it.
    if (!UiaClientsAreListening()) {
        impl_->tracker.Forget();
        return;
    }
    impl_->tracker.Update();
}

void UiaHost::RaiseFocus() {
    Context& c = *impl_->ctx;
    if (!c.alive || !c.screen || !UiaClientsAreListening()) return;
    if (const nui::Widget* f = nui::AccessFocus(*c.screen)) impl_->raiser.FocusChanged(f->Id());
}

void UiaHost::Detach() {
    Context& c = *impl_->ctx;
    if (!c.alive) return;
    // From here every element answers "not available", wherever it is held. The tree is not touched again.
    c.alive = false;
    c.screen = nullptr;
    c.pending.clear();
    if (c.hwnd && IsWindow(c.hwnd)) UiaReturnRawElementProvider(c.hwnd, 0, 0, nullptr);  // lets go of the host provider
    if (impl_->root) {
        IRawElementProviderSimple* simple = impl_->root;
        UiaDisconnectProvider(simple);  // the client side of every element made from it fails from now on
        impl_->root->Release();
        impl_->root = nullptr;
    }
}
