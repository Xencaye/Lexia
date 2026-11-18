// lexia_uia_test (ctest `uia_tree`, K.5): a UI Automation CLIENT test of the real Settings window. It makes the
// window with a sample state (off-screen), then, from another thread, uses IUIAutomation to
//   - walk the tree of every page (Home, Voices, Settings, Report, Help, About) and check the names, control
//     types, patterns and automation ids of the main controls (the names come from strings/);
//   - invoke the power button, set the Master volume and a Voice slider with RangeValue, toggle a switch, and
//     check the PageMessage that the window's onCommand received;
//   - open the Delete voice sheet and check that the page beneath is no longer offered;
//   - destroy the window and check that an element the client still holds now fails with
//     UIA_E_ELEMENTNOTAVAILABLE (a late call of a screen reader must not crash or read freed memory).
// The tree is printed once, so the log of a run shows what Narrator sees.
//
// The window lives on the main thread, which pumps messages; the client runs on a second thread (MTA), as a
// screen reader would be another process. Anything that must happen on the window's thread (showing a page,
// destroying the window) is posted to it (OnUi).
//
// It needs an interactive desktop: UI Automation reaches a window through its window station. In a service
// session (the window station is not "WinSta0") there is none: the test then says so and exits 0 (SKIP).
// Exit code: 0 = passed or skipped, 1 = a check failed, 2 = the window could not be made.
//
// Waiting (R49): every check that depends on something that happens later (a queued action, a UIA event, a window
// state change, a page that is built) waits for its CONDITION with WaitUntil (polls every 20 ms, up to 5 s), never for
// a fixed time: a slow or busy machine just waits longer, a fast one is quicker. No check needs the window to be the
// foreground window or to have the real keyboard focus: the Master volume focus event follows the widget focus of
// the tree (not ::GetFocus), and the window is only minimised and restored (restored without activation, so the test
// never takes the foreground from the person who runs it).
#include <windows.h>

#include <UIAutomationClient.h>
#include <UIAutomationCoreApi.h>  // UIA_E_ELEMENTNOTAVAILABLE
#include <wrl/client.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "common.h"
#include "native_ui/pages/page.h"
#include "settings_window.h"
#include "settingsshot_sample.h"
#include "strings/string_ids.h"
#include "strings/strings.h"

using Microsoft::WRL::ComPtr;

namespace {

const UINT kRunOnUi = WM_APP + 100;  // message to the helper window: lParam = std::shared_ptr<UiJob>* (heap)

std::mutex g_mutex;
std::vector<PageMessage> g_commands;  // what the window's onCommand received (UI thread writes, client reads)
std::atomic<bool> g_destroyed{false};
std::vector<std::string> g_failures;
int g_checks = 0;

void Check(bool ok, const std::string& what) {
    ++g_checks;
    if (!ok) {
        g_failures.push_back(what);
        std::printf("  FAIL: %s\n", what.c_str());
    }
}

std::string T(const char* id) { return strings::Text("en", id); }

std::string Narrow(BSTR b) { return b ? WideToUtf8(std::wstring(b, SysStringLen(b))) : std::string(); }

const char* TypeName(int t) {
    switch (t) {
        case UIA_ButtonControlTypeId: return "Button";
        case UIA_HyperlinkControlTypeId: return "Hyperlink";
        case UIA_SliderControlTypeId: return "Slider";
        case UIA_ComboBoxControlTypeId: return "ComboBox";
        case UIA_ListItemControlTypeId: return "ListItem";
        case UIA_ListControlTypeId: return "List";
        case UIA_TextControlTypeId: return "Text";
        case UIA_ImageControlTypeId: return "Image";
        case UIA_GroupControlTypeId: return "Group";
        case UIA_WindowControlTypeId: return "Window";
        case UIA_ProgressBarControlTypeId: return "ProgressBar";
        case UIA_PaneControlTypeId: return "Pane";
        case UIA_TitleBarControlTypeId: return "TitleBar";
        case UIA_MenuBarControlTypeId: return "MenuBar";
        default: return "?";
    }
}

// ---- the window's thread

// A message-only window on the window's thread. OnUi posts to it with PostMessageW instead of posting a thread
// message (PostThreadMessageW with no window): a thread message is dropped when the thread is inside a modal loop
// (COM's STA wait during a UIA call or event, a system loop ...), because DispatchMessage gets a message with no
// window and discards it, so the waiter below would wait for ever. A message to a window is dispatched by every
// modal loop that pumps.
HWND g_uiHelper = nullptr;
const UINT kQuitUi = WM_APP + 101;  // asks the helper window's thread to leave its message loop

// What one OnUi call shares with the job it posts. Held by a shared_ptr that the job also owns, so a job that runs
// late (after OnUi gave up) still touches live memory only.
struct UiJob {
    std::function<void()> fn;
    std::promise<void> done;
    std::atomic<bool> abandoned{false};  // set when the waiter timed out: a late run skips `fn` (its captures may be gone)
};

LRESULT CALLBACK UiHelperProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == kRunOnUi) {
        // lParam = a heap std::shared_ptr<UiJob>* made by OnUi; this side deletes it.
        std::unique_ptr<std::shared_ptr<UiJob>> holder(reinterpret_cast<std::shared_ptr<UiJob>*>(lp));
        UiJob& job = **holder;
        if (!job.abandoned.load()) job.fn();
        job.done.set_value();
        return 0;
    }
    if (msg == kQuitUi) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// Makes the helper window; call on the window's thread before the client starts.
bool CreateUiHelper() {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = UiHelperProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"LexiaUiaTestHelper";
    RegisterClassW(&wc);
    g_uiHelper = CreateWindowExW(0, wc.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
    return g_uiHelper != nullptr;
}

// Runs `fn` on the window's thread and waits for it (at most 30 s). A job that cannot be posted or does not run in
// time is a failed check and OnUi returns without waiting further; the job state lives on the heap and is shared
// with the posted message, so a late run never touches freed memory (and skips `fn`, whose captures may be gone).
void OnUi(std::function<void()> fn) {
    auto job = std::make_shared<UiJob>();
    job->fn = std::move(fn);
    std::future<void> wait = job->done.get_future();
    auto* holder = new std::shared_ptr<UiJob>(job);  // owned by the message until the helper window's proc takes it
    if (!PostMessageW(g_uiHelper, kRunOnUi, 0, reinterpret_cast<LPARAM>(holder))) {
        const DWORD err = GetLastError();
        delete holder;
        std::printf("  FAIL: OnUi could not post (error %lu)\n", static_cast<unsigned long>(err));
        ++g_checks;
        g_failures.push_back("OnUi could not post");
        return;
    }
    if (wait.wait_for(std::chrono::seconds(30)) != std::future_status::ready) {
        job->abandoned = true;
        std::printf("  FAIL: OnUi timed out after 30 s: the window thread did not run the job\n");
        ++g_checks;
        g_failures.push_back("OnUi timed out");
    }
}

// Polls `pred` every 20 ms until it is true or `timeoutMs` has passed; returns the last answer. Called from the
// client thread only (the window's thread pumps messages, so the thing waited for can happen meanwhile; this
// thread owns no window and needs no pump). The deadline is generous (5 s) on purpose: it is a "never", not a pace.
bool WaitUntil(const std::function<bool()>& pred, int timeoutMs = 5000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    for (;;) {
        if (pred()) return true;
        if (std::chrono::steady_clock::now() >= deadline) return false;
        Sleep(20);
    }
}

// Waits until a command that `pred` accepts has arrived; returns it in `out`.
bool WaitCommand(const std::function<bool(const PageMessage&)>& pred, PageMessage& out) {
    return WaitUntil([&] {
        std::lock_guard<std::mutex> lock(g_mutex);
        for (const PageMessage& m : g_commands) {
            if (pred(m)) {
                out = m;
                return true;
            }
        }
        return false;
    });
}

void ClearCommands() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_commands.clear();
}

// ---- UI Automation helpers

struct Client {
    ComPtr<IUIAutomation> uia;
    ComPtr<IUIAutomationTreeWalker> walker;
    HWND hwnd = nullptr;

    ComPtr<IUIAutomationElement> Root() {
        ComPtr<IUIAutomationElement> el;
        uia->ElementFromHandle(hwnd, &el);
        return el;
    }
};

struct Node {
    ComPtr<IUIAutomationElement> el;
    int depth = 0;
    int type = 0;
    std::string name, id;
};

std::string NameOf(IUIAutomationElement* el) {
    BSTR b = nullptr;
    if (FAILED(el->get_CurrentName(&b))) return std::string();
    const std::string s = Narrow(b);
    SysFreeString(b);
    return s;
}

std::string IdOf(IUIAutomationElement* el) {
    BSTR b = nullptr;
    if (FAILED(el->get_CurrentAutomationId(&b))) return std::string();
    const std::string s = Narrow(b);
    SysFreeString(b);
    return s;
}

// The whole control view under the window, depth first.
void Walk(Client& c, IUIAutomationElement* el, int depth, std::vector<Node>& out) {
    ComPtr<IUIAutomationElement> child;
    c.walker->GetFirstChildElement(el, &child);
    while (child) {
        Node n;
        n.el = child;
        n.depth = depth;
        CONTROLTYPEID t = 0;
        child->get_CurrentControlType(&t);
        n.type = t;
        n.name = NameOf(child.Get());
        n.id = IdOf(child.Get());
        out.push_back(n);
        Walk(c, child.Get(), depth + 1, out);
        ComPtr<IUIAutomationElement> next;
        c.walker->GetNextSiblingElement(child.Get(), &next);
        child = next;
    }
}

std::vector<Node> TreeOf(Client& c) {
    std::vector<Node> out;
    ComPtr<IUIAutomationElement> root = c.Root();
    if (root) Walk(c, root.Get(), 0, out);
    return out;
}

const Node* Find(const std::vector<Node>& tree, const std::string& name, int type = 0) {
    for (const Node& n : tree) {
        if (n.name == name && (type == 0 || n.type == type)) return &n;
    }
    return nullptr;
}

template <class P>
ComPtr<P> Pattern(IUIAutomationElement* el, PATTERNID id) {
    ComPtr<P> p;
    el->GetCurrentPatternAs(id, __uuidof(P), reinterpret_cast<void**>(p.GetAddressOf()));
    return p;
}

void Dump(const char* title, const std::vector<Node>& tree) {
    std::printf("--- %s (%zu elements)\n", title, tree.size());
    for (const Node& n : tree) {
        std::printf("%*s[%s] \"%s\"  id=%s\n", n.depth * 2, "", TypeName(n.type), n.name.c_str(), n.id.c_str());
    }
}

// Every automation id the page's elements carry must be there and different from the others.
void CheckIds(const char* page, const std::vector<Node>& tree) {
    std::set<std::string> seen;
    for (const Node& n : tree) {
        // The title bar and system buttons belong to the host window, not to Lexia's tree.
        if (n.type == UIA_TitleBarControlTypeId || n.type == UIA_MenuBarControlTypeId) continue;
        if (n.id.empty()) continue;  // host elements (system menu button ...) have none
        Check(seen.insert(n.id).second, std::string(page) + ": duplicate automation id " + n.id);
    }
}

// The frame's link that belongs to a page (every page has one).
const char* NavLinkId(nui::Page page) {
    switch (page) {
        case nui::Page::Home: return strings::id::kWindowNavHome;
        case nui::Page::Voices: return strings::id::kWindowNavVoices;
        case nui::Page::Settings: return strings::id::kWindowNavSettings;
        case nui::Page::Report: return strings::id::kWindowNavReport;
        case nui::Page::Help: return strings::id::kWindowNavHelp;
        case nui::Page::About: return strings::id::kWindowNavAbout;
    }
    return strings::id::kWindowNavHome;
}

// Shows a page on the window's thread, then waits until a client SEES it: the frame's link of that page is
// selected in the UIA tree (the page's own widgets are built in the same call, so the tree is whole by then). Not a
// fixed sleep: a busy machine only takes longer. A page that never shows up is a failed check of its own.
void ShowPage(Client& c, nui::Page page, SettingsWindow& w) {
    OnUi([&] { w.ShowPage(page); });
    const std::string link = T(NavLinkId(page));
    const bool seen = WaitUntil([&] {
        std::vector<Node> tree = TreeOf(c);
        const Node* n = Find(tree, link, UIA_HyperlinkControlTypeId);
        if (!n) return false;
        auto item = Pattern<IUIAutomationSelectionItemPattern>(n->el.Get(), UIA_SelectionItemPatternId);
        BOOL selected = FALSE;
        if (item) item->get_CurrentIsSelected(&selected);
        return selected != FALSE;
    });
    Check(seen, std::string("the ") + nui::PageName(page) + " page is offered (its frame link is selected)");
}

// ---- the checks

void ControlsOfAllPages(Client& c, SettingsWindow& w) {
    // Home
    ShowPage(c, nui::Page::Home, w);
    {
        std::vector<Node> tree = TreeOf(c);
        Dump("Home", tree);
        CheckIds("Home", tree);
        const Node* power = Find(tree, T(strings::id::kWindowHomePower), UIA_ButtonControlTypeId);
        Check(power != nullptr, "Home: the power button is a Button called " + T(strings::id::kWindowHomePower));
        if (power) {
            auto toggle = Pattern<IUIAutomationTogglePattern>(power->el.Get(), UIA_TogglePatternId);
            Check(toggle != nullptr, "Home: the power button has the Toggle pattern");
            ToggleState st = ToggleState_Indeterminate;
            if (toggle) toggle->get_CurrentToggleState(&st);
            Check(st == ToggleState_Off, "Home: the power button is Off in the sample state");
            auto invoke = Pattern<IUIAutomationInvokePattern>(power->el.Get(), UIA_InvokePatternId);
            Check(invoke != nullptr, "Home: the power button can be invoked");
            BOOL enabled = FALSE;
            power->el->get_CurrentIsEnabled(&enabled);
            Check(enabled, "Home: the power button is enabled");
            BOOL focusable = FALSE;
            power->el->get_CurrentIsKeyboardFocusable(&focusable);
            Check(focusable, "Home: the power button can take the keyboard focus");
            VARIANT v;
            VariantInit(&v);
            power->el->GetCurrentPropertyValue(UIA_BoundingRectanglePropertyId, &v);
            Check(v.vt == (VT_R8 | VT_ARRAY), "Home: the power button has a bounding rectangle");
            VariantClear(&v);
            // The box is in screen pixels: 124 DIPs wide at the window's DPI, and inside the window's rectangle.
            ::RECT pr = {}, wr = {};
            power->el->get_CurrentBoundingRectangle(&pr);
            c.Root()->get_CurrentBoundingRectangle(&wr);
            const double expected = 124.0 * GetDpiForWindow(c.hwnd) / 96.0;
            Check(std::abs((pr.right - pr.left) - expected) <= 2.0 && pr.left >= wr.left && pr.right <= wr.right &&
                      pr.top >= wr.top && pr.bottom <= wr.bottom,
                  "Home: the power button's rectangle is 124 DIPs wide and inside the window");
        }
        Check(Find(tree, T(strings::id::kWindowHomeEnable), UIA_ButtonControlTypeId) != nullptr,
              "Home: the enable button");
        for (const char* id : {strings::id::kWindowNavHome, strings::id::kWindowNavVoices, strings::id::kWindowNavSettings,
                               strings::id::kWindowNavReport, strings::id::kWindowNavHelp, strings::id::kWindowNavAbout}) {
            const Node* link = Find(tree, T(id), UIA_HyperlinkControlTypeId);
            Check(link != nullptr, "Home: link " + T(id));
            if (link) {
                auto item = Pattern<IUIAutomationSelectionItemPattern>(link->el.Get(), UIA_SelectionItemPatternId);
                Check(item != nullptr, "Home: link " + T(id) + " has SelectionItem");
                BOOL selected = FALSE;
                if (item) item->get_CurrentIsSelected(&selected);
                Check(static_cast<bool>(selected) == (std::string(id) == strings::id::kWindowNavHome),
                      "Home: only the Home link is selected (" + T(id) + ")");
            }
        }
        const Node* vol = Find(tree, T(strings::id::kWindowVolume), UIA_SliderControlTypeId);
        Check(vol != nullptr, "Home: the Master volume is a Slider");
        if (vol) {
            auto range = Pattern<IUIAutomationRangeValuePattern>(vol->el.Get(), UIA_RangeValuePatternId);
            Check(range != nullptr, "Home: the Master volume has RangeValue");
            if (range) {
                double v = -1, lo = -1, hi = -1;
                range->get_CurrentValue(&v);
                range->get_CurrentMinimum(&lo);
                range->get_CurrentMaximum(&hi);
                Check(v == 50 && lo == 0 && hi == 100, "Home: the Master volume reads 50 in 0..100");
            }
        }
    }

    // Voices
    ShowPage(c, nui::Page::Voices, w);
    {
        std::vector<Node> tree = TreeOf(c);
        Dump("Voices", tree);
        CheckIds("Voices", tree);
        const Node* voice = Find(tree, T(strings::id::kWindowVoiceVoice), UIA_ComboBoxControlTypeId);
        Check(voice != nullptr, "Voices: the Voice choice is a ComboBox");
        if (voice) {
            Check(Pattern<IUIAutomationExpandCollapsePattern>(voice->el.Get(), UIA_ExpandCollapsePatternId) != nullptr,
                  "Voices: the Voice choice has ExpandCollapse");
            auto value = Pattern<IUIAutomationValuePattern>(voice->el.Get(), UIA_ValuePatternId);
            Check(value != nullptr, "Voices: the Voice choice has Value");
            if (value) {
                BSTR b = nullptr;
                value->get_CurrentValue(&b);
                Check(Narrow(b) == "Alba (Medium)", "Voices: the shown choice is the user's Voice");  // short name + quality
                SysFreeString(b);
            }
        }
        for (const char* id : {strings::id::kWindowVoiceDownload, strings::id::kWindowVoiceAddFromFile,
                               strings::id::kWindowVoiceDelete}) {
            Check(Find(tree, T(id), UIA_ButtonControlTypeId) != nullptr, "Voices: button " + T(id));
        }
        for (const char* id : {strings::id::kWindowVoiceSpeed, strings::id::kWindowVoicePitch, strings::id::kWindowVoiceVolume}) {
            const Node* s = Find(tree, T(id), UIA_SliderControlTypeId);
            Check(s != nullptr, "Voices: slider " + T(id));
            if (s) Check(Pattern<IUIAutomationRangeValuePattern>(s->el.Get(), UIA_RangeValuePatternId) != nullptr,
                         "Voices: slider " + T(id) + " has RangeValue");
        }
        Check(Find(tree, T(strings::id::kWindowVoiceSavePreset), UIA_HyperlinkControlTypeId) != nullptr,
              "Voices: the Save preset link");
    }

    // Settings
    ShowPage(c, nui::Page::Settings, w);
    {
        std::vector<Node> tree = TreeOf(c);
        Dump("Settings", tree);
        CheckIds("Settings", tree);
        int keyRows = 0, chips = 0, switches = 0;
        for (const Node& n : tree) {
            if (n.type == UIA_ListItemControlTypeId) ++keyRows;
            if (n.type == UIA_ButtonControlTypeId && n.name.find("F") != std::string::npos &&
                (n.name.find("F8") != std::string::npos || n.name.find("F7") != std::string::npos ||
                 n.name.find("F6") != std::string::npos)) {
                ++chips;
            }
        }
        for (const char* id : {strings::id::kWindowSettingsRunAtStartup, strings::id::kWindowSettingsStartMinimised,
                               strings::id::kWindowSettingsRunAsAdmin, strings::id::kWindowSettingsDarkMode,
                               strings::id::kWindowSettingsLeftHanded, strings::id::kWindowSettingsReadAlong}) {
            const Node* sw = Find(tree, T(id), UIA_ButtonControlTypeId);
            Check(sw != nullptr, "Settings: switch " + T(id));
            if (!sw) continue;
            ++switches;
            Check(Pattern<IUIAutomationTogglePattern>(sw->el.Get(), UIA_TogglePatternId) != nullptr,
                  "Settings: switch " + T(id) + " has Toggle");
            BSTR b = nullptr;
            sw->el->get_CurrentLocalizedControlType(&b);
            Check(Narrow(b) == T(strings::id::kUiaSwitch), "Settings: switch " + T(id) + " is localised as a switch");
            SysFreeString(b);
        }
        Check(keyRows == 3, "Settings: three key rows (" + std::to_string(keyRows) + ")");
        Check(chips == 3, "Settings: three key chips (" + std::to_string(chips) + ")");
        Check(switches == 6, "Settings: six switches");
    }

    // Report, Help, About
    for (nui::Page p : {nui::Page::Report, nui::Page::Help, nui::Page::About}) {
        ShowPage(c, p, w);
        std::vector<Node> tree = TreeOf(c);
        Dump(nui::PageName(p), tree);
        CheckIds(nui::PageName(p), tree);
        int links = 0, texts = 0;
        for (const Node& n : tree) {
            if (n.type == UIA_HyperlinkControlTypeId) ++links;
            if (n.type == UIA_TextControlTypeId && !n.name.empty()) ++texts;
        }
        Check(links >= 6, std::string(nui::PageName(p)) + ": the links (" + std::to_string(links) + ")");
        Check(texts >= 1, std::string(nui::PageName(p)) + ": the texts (" + std::to_string(texts) + ")");
    }
}

void Actions(Client& c, SettingsWindow& w) {
    PageMessage m;

    // Invoke / Toggle the power button -> SetOn.
    ShowPage(c, nui::Page::Home, w);
    {
        std::vector<Node> tree = TreeOf(c);
        const Node* power = Find(tree, T(strings::id::kWindowHomePower), UIA_ButtonControlTypeId);
        if (power) {
            ClearCommands();
            auto toggle = Pattern<IUIAutomationTogglePattern>(power->el.Get(), UIA_TogglePatternId);
            Check(toggle && SUCCEEDED(toggle->Toggle()), "Actions: Toggle on the power button succeeds");
            Check(WaitCommand([](const PageMessage& x) { return x.type == PageMessageType::SetOn && x.on; }, m),
                  "Actions: the power button's Toggle sent SetOn(true)");
            // The shown state is still the last state's until the host answers.
            ToggleState st = ToggleState_Indeterminate;
            if (toggle) toggle->get_CurrentToggleState(&st);
            Check(st == ToggleState_Off, "Actions: the power button still shows the old state");

            ClearCommands();
            auto invoke = Pattern<IUIAutomationInvokePattern>(power->el.Get(), UIA_InvokePatternId);
            Check(invoke && SUCCEEDED(invoke->Invoke()), "Actions: Invoke on the power button succeeds");
            Check(WaitCommand([](const PageMessage& x) { return x.type == PageMessageType::SetOn; }, m),
                  "Actions: the power button's Invoke sent SetOn");
        }
        // Master volume.
        const Node* vol = Find(tree, T(strings::id::kWindowVolume), UIA_SliderControlTypeId);
        if (vol) {
            ClearCommands();
            auto range = Pattern<IUIAutomationRangeValuePattern>(vol->el.Get(), UIA_RangeValuePatternId);
            Check(range && SUCCEEDED(range->SetValue(80)), "Actions: SetValue(80) on the Master volume succeeds");
            Check(WaitCommand([](const PageMessage& x) { return x.type == PageMessageType::SetMasterVolume && x.value == 80; }, m),
                  "Actions: the Master volume's SetValue sent SetMasterVolume(80)");
            Check(range && FAILED(range->SetValue(500)), "Actions: SetValue(500) is refused");
        }
    }

    // A Voice slider.
    ShowPage(c, nui::Page::Voices, w);
    {
        std::vector<Node> tree = TreeOf(c);
        const Node* speed = Find(tree, T(strings::id::kWindowVoiceSpeed), UIA_SliderControlTypeId);
        if (speed) {
            ClearCommands();
            auto range = Pattern<IUIAutomationRangeValuePattern>(speed->el.Get(), UIA_RangeValuePatternId);
            Check(range && SUCCEEDED(range->SetValue(1.5)), "Actions: SetValue(1.5) on Speed succeeds");
            Check(WaitCommand([](const PageMessage& x) {
                      return x.type == PageMessageType::SetControl && x.name == "speed" && x.value > 1.4 && x.value < 1.6;
                  }, m),
                  "Actions: Speed's SetValue sent SetControl(speed, 1.5)");
        }
        // The Select opens and closes.
        const Node* voice = Find(tree, T(strings::id::kWindowVoiceVoice), UIA_ComboBoxControlTypeId);
        if (voice) {
            auto ec = Pattern<IUIAutomationExpandCollapsePattern>(voice->el.Get(), UIA_ExpandCollapsePatternId);
            Check(ec && SUCCEEDED(ec->Expand()), "Actions: Expand on the Voice choice succeeds");
            // Expand / Collapse are queued for the window's thread: wait until the state shows it.
            ExpandCollapseState st = ExpandCollapseState_Collapsed;
            auto stateIs = [&](ExpandCollapseState want) {
                return WaitUntil([&] {
                    if (ec) ec->get_CurrentExpandCollapseState(&st);
                    return ec && st == want;
                });
            };
            Check(stateIs(ExpandCollapseState_Expanded), "Actions: the Voice choice is expanded");
            std::vector<Node> open;
            Check(WaitUntil([&] {
                      open = TreeOf(c);
                      return Find(open, "Cori (Medium)", UIA_ListItemControlTypeId) != nullptr;
                  }),
                  "Actions: the open list offers its rows");
            Check(ec && SUCCEEDED(ec->Collapse()), "Actions: Collapse succeeds");
            Check(stateIs(ExpandCollapseState_Collapsed), "Actions: the Voice choice is collapsed again");
        }
    }

    // A switch.
    ShowPage(c, nui::Page::Settings, w);
    {
        std::vector<Node> tree = TreeOf(c);
        const Node* sw = Find(tree, T(strings::id::kWindowSettingsRunAtStartup), UIA_ButtonControlTypeId);
        if (sw) {
            ClearCommands();
            auto toggle = Pattern<IUIAutomationTogglePattern>(sw->el.Get(), UIA_TogglePatternId);
            Check(toggle && SUCCEEDED(toggle->Toggle()), "Actions: Toggle on Run at startup succeeds");
            Check(WaitCommand([](const PageMessage& x) { return x.type == PageMessageType::SetRunAtStartup && x.on; }, m),
                  "Actions: the switch's Toggle sent SetRunAtStartup(true)");
        }
        // The disabled Start minimised switch refuses.
        const Node* min = Find(tree, T(strings::id::kWindowSettingsStartMinimised), UIA_ButtonControlTypeId);
        if (min) {
            auto toggle = Pattern<IUIAutomationTogglePattern>(min->el.Get(), UIA_TogglePatternId);
            BOOL enabled = TRUE;
            min->el->get_CurrentIsEnabled(&enabled);
            Check(!enabled, "Actions: Start minimised is disabled while Run at startup is off");
            Check(toggle && FAILED(toggle->Toggle()), "Actions: Toggle on a disabled switch is refused");
        }
    }
}

// ---- K10: the window has no title bar; its own minimise and close buttons are Buttons with names

void WindowButtons(Client& c, SettingsWindow& w) {
    const HWND hwnd = c.hwnd;
    // No caption and no frame: the client area is the whole window (WM_NCCALCSIZE), 800 x 600 DIPs.
    RECT window = {}, client = {};
    GetWindowRect(hwnd, &window);
    GetClientRect(hwnd, &client);
    Check(client.right - client.left == window.right - window.left && client.bottom - client.top == window.bottom - window.top,
          "WindowButtons: the client area is the whole window (no title bar)");
    const UINT dpi = GetDpiForWindow(hwnd);
    Check(client.right - client.left == MulDiv(800, static_cast<int>(dpi), 96) &&
              client.bottom - client.top == MulDiv(600, static_cast<int>(dpi), 96),
          "WindowButtons: the window is 800 x 600 DIPs");

    for (nui::Page page : {nui::Page::Home, nui::Page::Voices, nui::Page::Settings}) {
        ShowPage(c, page, w);
        std::vector<Node> tree = TreeOf(c);
        const Node* minimise = Find(tree, T(strings::id::kWindowMinimise), UIA_ButtonControlTypeId);
        const Node* close = Find(tree, T(strings::id::kWindowClose), UIA_ButtonControlTypeId);
        const std::string where = std::string(nui::PageName(page)) + ": ";
        Check(minimise != nullptr, where + "the minimise button is a Button called " + T(strings::id::kWindowMinimise));
        Check(close != nullptr, where + "the close button is a Button called " + T(strings::id::kWindowClose));
        if (minimise) {
            Check(minimise->id == "frame.minimise", where + "minimise has the automation id frame.minimise");
            Check(Pattern<IUIAutomationInvokePattern>(minimise->el.Get(), UIA_InvokePatternId) != nullptr,
                  where + "minimise can be invoked");
        }
        if (close) {
            Check(close->id == "frame.close", where + "close has the automation id frame.close");
            Check(Pattern<IUIAutomationInvokePattern>(close->el.Get(), UIA_InvokePatternId) != nullptr,
                  where + "close can be invoked");
        }
    }

    // Invoke on minimise minimises the window (a posted WM_SYSCOMMAND). Close is NOT invoked here: it would end the
    // window before the checks that come after this one (the end of the test destroys it itself).
    ShowPage(c, nui::Page::Home, w);
    std::vector<Node> tree = TreeOf(c);
    const Node* minimise = Find(tree, T(strings::id::kWindowMinimise), UIA_ButtonControlTypeId);
    ClearCommands();
    if (minimise) {
        Check(!IsIconic(hwnd), "WindowButtons: the window is not minimised before the test");
        auto invoke = Pattern<IUIAutomationInvokePattern>(minimise->el.Get(), UIA_InvokePatternId);
        Check(invoke && SUCCEEDED(invoke->Invoke()), "WindowButtons: Invoke on minimise succeeds");
        // The Invoke posts WM_SYSCOMMAND SC_MINIMIZE to the window's thread: wait for the window state itself
        // (IsIconic), not for a time. Nothing here needs the foreground: a minimise works for any window.
        Check(WaitUntil([&] { return IsIconic(hwnd) != FALSE; }),
              "WindowButtons: Invoke on minimise minimises the window");
        // Back to normal for the checks that follow. SW_SHOWNOACTIVATE restores without activating, so the test
        // never takes the foreground from the person who runs it (SW_RESTORE would).
        OnUi([&] { ShowWindow(hwnd, SW_SHOWNOACTIVATE); });
        Check(WaitUntil([&] { return IsIconic(hwnd) == FALSE; }), "WindowButtons: the window can be restored");
    }
    // Neither button sent the App anything (they are the window's own).
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        Check(g_commands.empty(), "WindowButtons: the window buttons send no PageMessage");
    }
    ShowPage(c, nui::Page::Home, w);
}

// ---- events: a focus change and a property change reach a client's handlers

class FocusHandler final : public IUIAutomationFocusChangedEventHandler {
public:
    STDMETHODIMP_(ULONG) AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&refs_)); }
    STDMETHODIMP_(ULONG) Release() override {
        const ULONG n = static_cast<ULONG>(InterlockedDecrement(&refs_));
        if (n == 0) delete this;
        return n;
    }
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IUIAutomationFocusChangedEventHandler)) {
            *ppv = static_cast<IUIAutomationFocusChangedEventHandler*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP HandleFocusChangedEvent(IUIAutomationElement* sender) override {
        const std::string name = sender ? NameOf(sender) : std::string();
        std::lock_guard<std::mutex> lock(m);
        names.push_back(name);
        return S_OK;
    }
    // Waits (up to 5 s) until the handler has been told that `name` has the focus. UIA delivers the event on its
    // own thread, after the provider raised it, so how long that takes depends on the load.
    bool Saw(const std::string& name) {
        return WaitUntil([&] {
            std::lock_guard<std::mutex> lock(m);
            for (const std::string& n : names) {
                if (n == name) return true;
            }
            return false;
        });
    }
    std::mutex m;
    std::vector<std::string> names;

private:
    LONG refs_ = 1;
};

class PropertyHandler final : public IUIAutomationPropertyChangedEventHandler {
public:
    STDMETHODIMP_(ULONG) AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&refs_)); }
    STDMETHODIMP_(ULONG) Release() override {
        const ULONG n = static_cast<ULONG>(InterlockedDecrement(&refs_));
        if (n == 0) delete this;
        return n;
    }
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IUIAutomationPropertyChangedEventHandler)) {
            *ppv = static_cast<IUIAutomationPropertyChangedEventHandler*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP HandlePropertyChangedEvent(IUIAutomationElement* sender, PROPERTYID, VARIANT newValue) override {
        const std::string name = sender ? NameOf(sender) : std::string();
        std::lock_guard<std::mutex> lock(m);
        changes.push_back({name, newValue.vt == VT_I4 ? newValue.lVal : -1});
        return S_OK;
    }
    // As FocusHandler::Saw: waits (up to 5 s) for the property change of `name` to `value`.
    bool Saw(const std::string& name, long value) {
        return WaitUntil([&] {
            std::lock_guard<std::mutex> lock(m);
            for (const auto& c : changes) {
                if (c.first == name && c.second == value) return true;
            }
            return false;
        });
    }
    std::mutex m;
    std::vector<std::pair<std::string, long>> changes;

private:
    LONG refs_ = 1;
};

void Events(Client& c, SettingsWindow& w) {
    ComPtr<IUIAutomationElement> root = c.Root();
    FocusHandler* focus = new FocusHandler();
    PropertyHandler* props = new PropertyHandler();
    c.uia->AddFocusChangedEventHandler(nullptr, focus);
    PROPERTYID ids[] = {UIA_ToggleToggleStatePropertyId};
    c.uia->AddPropertyChangedEventHandlerNativeArray(root.Get(), TreeScope_Subtree, nullptr, props, ids, 1);

    // The window raises events only while UIA says a client listens (UiaClientsAreListening, which is process
    // wide, so the test can ask it). UIA registers the handlers on its own thread, so on a busy machine the
    // provider side may not know yet when AddFocusChangedEventHandler returns: wait until it does. Without this
    // the page switches below could run before the window listens, and the "first look" would be lost.
    Check(WaitUntil([] { return UiaClientsAreListening() != FALSE; }),
          "Events: the window sees that a client listens to events");
    // The first Update after a client starts to listen only takes a look; this page switch is that look.
    ShowPage(c, nui::Page::Voices, w);
    ShowPage(c, nui::Page::Home, w);
    std::vector<Node> tree = TreeOf(c);
    const Node* vol = Find(tree, T(strings::id::kWindowVolume), UIA_SliderControlTypeId);
    Check(vol != nullptr, "Events: the Master volume");
    if (vol) {
        vol->el->SetFocus();
        Check(focus->Saw(T(strings::id::kWindowVolume)), "Events: SetFocus on the Master volume raises a focus event");
    }
    // The host answers with a new state: the toggle state changes, and the client hears it.
    OnUi([&] { w.SetState(shot::SampleState(/*on=*/true, /*dark=*/false, "user")); });
    Check(props->Saw(T(strings::id::kWindowHomePower), ToggleState_On),
          "Events: a new state raises a ToggleState property event on the power button");
    OnUi([&] { w.SetState(shot::SampleState(/*on=*/false, /*dark=*/false, "user")); });
    c.uia->RemoveAllEventHandlers();
    focus->Release();
    props->Release();
}

// Returns the element of the open sheet (kept by the caller for the late-call check).
ComPtr<IUIAutomationElement> SheetAndModality(Client& c, SettingsWindow& w) {
    ShowPage(c, nui::Page::Voices, w);
    std::vector<Node> before = TreeOf(c);
    const Node* del = Find(before, T(strings::id::kWindowVoiceDelete), UIA_ButtonControlTypeId);
    Check(del != nullptr, "Sheet: the Delete button");
    ComPtr<IUIAutomationElement> sheetEl;
    if (!del) return sheetEl;
    BOOL enabled = FALSE;
    del->el->get_CurrentIsEnabled(&enabled);
    Check(enabled, "Sheet: Delete is enabled for the user's Voice");
    auto invoke = Pattern<IUIAutomationInvokePattern>(del->el.Get(), UIA_InvokePatternId);
    Check(invoke && SUCCEEDED(invoke->Invoke()), "Sheet: Invoke on Delete succeeds");
    // The Invoke is queued for the window's thread: wait until the sheet is in the tree (the top Window that names
    // the Voice).
    auto findSheet = [](const std::vector<Node>& nodes) -> const Node* {
        const Node* found = nullptr;
        for (const Node& n : nodes) {
            if (n.type == UIA_WindowControlTypeId && n.depth == 0 && n.name.find("Alba (Medium)") != std::string::npos) {
                found = &n;
            }
        }
        return found;
    };
    std::vector<Node> tree;
    WaitUntil([&] {
        tree = TreeOf(c);
        return findSheet(tree) != nullptr;
    });
    Dump("Voices with the Delete voice sheet", tree);
    CheckIds("Sheet", tree);
    const Node* sheet = findSheet(tree);
    Check(sheet != nullptr, "Sheet: a Window (dialog) that names the Voice is offered");
    Check(Find(tree, T(strings::id::kWindowVoiceSpeed), UIA_SliderControlTypeId) == nullptr,
          "Sheet: the Speed slider of the page beneath is no longer offered");
    Check(Find(tree, T(strings::id::kWindowNavHome), UIA_HyperlinkControlTypeId) == nullptr,
          "Sheet: the links of the frame are not offered either");
    if (sheet) {
        sheetEl = sheet->el;
        auto window = Pattern<IUIAutomationWindowPattern>(sheet->el.Get(), UIA_WindowPatternId);
        Check(window != nullptr, "Sheet: the dialog has the Window pattern");
        BOOL modal = FALSE;
        if (window) window->get_CurrentIsModal(&modal);
        Check(modal, "Sheet: the dialog is modal");
        VARIANT v;
        VariantInit(&v);
        sheet->el->GetCurrentPropertyValue(UIA_IsDialogPropertyId, &v);
        Check(v.vt == VT_BOOL && v.boolVal == VARIANT_TRUE, "Sheet: IsDialog is true");
        VariantClear(&v);
        // The sheet's own buttons are inside it.
        std::vector<Node> inside;
        Walk(c, sheet->el.Get(), 0, inside);
        int buttons = 0;
        for (const Node& n : inside) buttons += n.type == UIA_ButtonControlTypeId;
        Check(buttons >= 2, "Sheet: the dialog holds its buttons");

        // Close through the Window pattern: the page is offered again.
        if (window) {
            Check(SUCCEEDED(window->Close()), "Sheet: Close succeeds");
            // Close is queued too: wait until the page beneath is offered again.
            std::vector<Node> after;
            Check(WaitUntil([&] {
                      after = TreeOf(c);
                      return Find(after, T(strings::id::kWindowVoiceSpeed), UIA_SliderControlTypeId) != nullptr;
                  }),
                  "Sheet: the page is offered again after Close");
            // Open it again for the late-call check.
            const Node* del2 = Find(after, T(strings::id::kWindowVoiceDelete), UIA_ButtonControlTypeId);
            if (del2) {
                auto inv2 = Pattern<IUIAutomationInvokePattern>(del2->el.Get(), UIA_InvokePatternId);
                if (inv2) inv2->Invoke();
                std::vector<Node> again;
                WaitUntil([&] {
                    again = TreeOf(c);
                    return findSheet(again) != nullptr;
                });
                const Node* reopened = findSheet(again);
                Check(reopened != nullptr, "Sheet: the Delete voice sheet opens again");
                sheetEl = reopened ? reopened->el : nullptr;
            }
        }
    }
    return sheetEl;
}

int RunClient(HWND hwnd, SettingsWindow* window) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    Client c;
    c.hwnd = hwnd;
    HRESULT hr = CoCreateInstance(__uuidof(CUIAutomation8), nullptr, CLSCTX_INPROC_SERVER, __uuidof(IUIAutomation),
                                  reinterpret_cast<void**>(c.uia.GetAddressOf()));
    if (FAILED(hr) || !c.uia) {
        Check(false, "the UI Automation client could not be made");
        return 1;
    }
    c.uia->get_ControlViewWalker(&c.walker);

    // The window answers UI Automation at all.
    ComPtr<IUIAutomationElement> root = c.Root();
    Check(root != nullptr, "the window has a UI Automation element");
    if (!root) return 1;
    BSTR fw = nullptr;
    root->get_CurrentFrameworkId(&fw);
    std::printf("framework id of the window: \"%s\"\n", Narrow(fw).c_str());
    SysFreeString(fw);

    ControlsOfAllPages(c, *window);
    Actions(c, *window);
    WindowButtons(c, *window);
    Events(c, *window);
    ComPtr<IUIAutomationElement> held = SheetAndModality(c, *window);

    // A slider element held across the destruction too.
    // The Delete voice sheet is still open here and modal, so the frame's links are NOT offered: the page switch
    // is only posted, without the wait for the page's link that ShowPage(c, ...) does. Nothing below needs the
    // page; the held elements are read straight from the sheet.
    OnUi([&] { window->ShowPage(nui::Page::Voices); });
    ComPtr<IUIAutomationElement> heldSlider;
    // (the sheet is open on the Voices page: take the element from the sheet's tree instead)
    if (held) {
        std::vector<Node> inside;
        Walk(c, held.Get(), 0, inside);
        if (!inside.empty()) heldSlider = inside.front().el;
    }

    // Destroy the window; every element we still hold must now fail cleanly.
    OnUi([&] { DestroyWindow(hwnd); });
    Check(WaitUntil([] { return g_destroyed.load(); }), "the window was destroyed");
    for (ComPtr<IUIAutomationElement> el : {held, heldSlider, root}) {
        if (!el) continue;
        BSTR name = nullptr;
        const HRESULT r = el->get_CurrentName(&name);
        SysFreeString(name);
        char buf[96];
        std::snprintf(buf, sizeof(buf), "a held element fails after the window is gone (0x%08lX)", static_cast<unsigned long>(r));
        // The sheet and its widgets answer UIA_E_ELEMENTNOTAVAILABLE. The window root may also answer with the
        // host's RPC/invalid-window error, because the HWND itself is gone; any failure is a clean one.
        Check(FAILED(r), buf);
    }
    if (held) {
        BSTR name = nullptr;
        const HRESULT r = held->get_CurrentName(&name);
        SysFreeString(name);
        Check(r == static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE), "the held sheet element fails with UIA_E_ELEMENTNOTAVAILABLE");
    }
    return 0;
}

// Is this an interactive desktop? UI Automation of a window needs the window station "WinSta0".
bool InteractiveDesktop(std::string& why) {
    HWINSTA ws = GetProcessWindowStation();
    wchar_t name[128] = {};
    DWORD len = 0;
    if (!ws || !GetUserObjectInformationW(ws, UOI_NAME, name, sizeof(name), &len)) {
        why = "the window station could not be read";
        return false;
    }
    if (std::wstring(name) != L"WinSta0") {
        why = "the window station is \"" + WideToUtf8(name) + "\", not WinSta0 (a service or session 0)";
        return false;
    }
    return true;
}

}  // namespace

int main() {
    // Unbuffered output: when stdout is a pipe or a file (ctest) the C runtime would hold the lines back, and a run
    // that hangs or is killed at ctest's timeout would show nothing about where it stopped.
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::string why;
    if (!InteractiveDesktop(why)) {
        std::printf("SKIP uia_tree: no interactive desktop: %s\n", why.c_str());
        return 0;
    }
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    // Pixels, not virtualised coordinates: the same as the shipped exe's manifest.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    SettingsWindow::Options options;
    options.title = L"Lexia (UI Automation test)";
    options.interfaceLanguage = "en";
    options.onCommand = [](const PageMessage& m) {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_commands.push_back(m);
    };
    options.onDestroyed = [] { g_destroyed = true; };
    options.startPage = nui::Page::Home;
    auto window = std::make_unique<SettingsWindow>(std::move(options));
    window->SetState(shot::SampleState(/*on=*/false, /*dark=*/false, "user"));

    // Off-screen: a big "monitor" far to the top left, so the window never covers the developer's work.
    const RECT away = {-20000, -20000, -16000, -17000};
    if (!window->Create(away)) {
        std::printf("FAIL: the Settings window could not be made\n");
        return 2;
    }
    const HWND hwnd = window->Handle();
    // The helper window that OnUi posts to, made on this thread before the client starts.
    if (!CreateUiHelper()) {
        std::printf("FAIL: the helper window could not be made\n");
        return 2;
    }
    // The sample state and an empty message queue first, then the client.
    MSG msg;
    PeekMessageW(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);  // makes the thread's queue

    int clientResult = 0;
    std::thread client([&] {
        clientResult = RunClient(hwnd, window.get());
        // Ends the main thread's loop on EVERY way out of RunClient (also its early returns, which would otherwise
        // leave GetMessageW waiting for ever). Through the helper window: a thread-level WM_QUIT could be lost in a
        // modal loop, a window message is not.
        PostMessageW(g_uiHelper, kQuitUi, 0, 0);
    });

    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    client.join();
    DestroyWindow(g_uiHelper);  // before the Settings window goes (no job can arrive any more: the client is done)
    g_uiHelper = nullptr;
    window.reset();

    std::printf("\n%d checks, %zu failed\n", g_checks, g_failures.size());
    return (g_failures.empty() && clientResult == 0) ? 0 : 1;
}
