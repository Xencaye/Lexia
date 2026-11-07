#pragma once
// UI Automation for the native Settings window (K.5, ADR-0009): what lets Narrator and other screen readers read
// and operate the self-drawn pages. lexia.exe only (COM, UIAutomationCore). The meaning of the tree (which
// widgets are offered, their names, their actions) is in access_tree.* (pure, tested); this file is the COM
// glue over it: IRawElementProviderSimple / Fragment / FragmentRoot and the control patterns, and the events.
//
// One UiaHost per window. The window forwards WM_GETOBJECT to it, calls Update() after every event that may have
// changed the tree, and Detach() on WM_DESTROY.
//
// Threads and lifetime (the security-sensitive part)
//   - Every provider is made with ProviderOptions_UseComThreading and is returned from WM_GETOBJECT on the
//     window's thread, so UI Automation calls it on that thread (the apartment that made it). The widget tree
//     is touched on that thread only; each entry point checks the thread and fails with
//     UIA_E_ELEMENTNOTAVAILABLE when it is another one.
//   - The providers do not own the window or the tree. They share a small context (UiaHost::Context) with an
//     `alive` flag. Detach() clears it on WM_DESTROY, so a screen reader that calls late (it may hold
//     elements for seconds) gets UIA_E_ELEMENTNOTAVAILABLE, never a dangling pointer. A provider holds only a
//     widget id: every call looks the widget up again in the tree, so a widget that was removed is "gone".
//   - Actions (Invoke, Toggle, SetValue ...) are checked at once and run LATER on the window's message loop
//     (UiaHost::kActionsMessage -> RunPendingActions): a click can start a file dialog, and UI Automation must
//     not wait for that inside its call.
#include <windows.h>

#include <memory>
#include <string>

namespace nui {
class Screen;
}

class UiaHost {
public:
    // The message the window gets when actions are waiting; the window calls RunPendingActions().
    static constexpr UINT kActionsMessage = WM_APP + 1;

    UiaHost(HWND hwnd, nui::Screen& screen, std::string interfaceLanguage);
    ~UiaHost();
    UiaHost(const UiaHost&) = delete;
    UiaHost& operator=(const UiaHost&) = delete;

    void SetDpi(UINT dpi);  // BoundingRectangle is in pixels: the window's DPI

    // WM_GETOBJECT. Returns the answer for UiaRootObjectId; 0 when the request is not a UI Automation one.
    LRESULT HandleGetObject(WPARAM wp, LPARAM lp);
    // The actions that wait (kActionsMessage). The window calls Update() afterwards.
    void RunPendingActions();
    // Compares the tree with the last look and raises the events (focus, properties, structure, live text).
    // Does nothing, and costs nothing, while no screen reader listens. Call between events.
    void Update();
    // The window got the keyboard focus (WM_SETFOCUS): says which element has it.
    void RaiseFocus();
    // The window is going (WM_DESTROY): every element fails from now on, and UI Automation lets go of them.
    void Detach();

    struct Context;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
