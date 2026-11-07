#include "overlay.h"

#include <windowsx.h>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <deque>
#include <mutex>
#include <utility>

#include "control_bar_layout.h"
#include "controlbar.h"
#include "hotkeys.h"
#include "layout.h"
#include "log.h"
#include "mouse_role.h"
#include "win32_geom.h"

// Everything one Session of the Overlay knows. It lives on RunOverlay's stack and the window
// reaches it through GWLP_USERDATA, so nothing is shared between Sessions.
struct Session {
    const Shot* shot = nullptr;
    // The Analysis. A job fills `analysis` and then sets `ready`, both under `mu` and only if
    // it still belongs to the current `generation` (a Language switch starts a new one); the UI
    // thread reads `analysis` only while `ready` is set, when no job can write it.
    std::mutex mu;
    unsigned generation = 0;
    std::atomic<bool> ready{false};
    Recognition analysis;
    HDC backDc = nullptr;
    HBITMAP backBmp = nullptr;
    HGDIOBJ backOld = nullptr;
    bool dragging = false;
    POINT start = {}, cur = {};
    int hover = -1;
    bool done = false;      // leave the overlay
    bool helpAsked = false; // the Control bar's Help ended the Session: open the Help page afterwards
    bool pending = false;   // a selection was just made and still needs handling
    bool busy = false;      // a Pick is being read (or a Click is waiting for the Analysis)
    std::vector<RECT> picked;  // areas already read on this screen (kept highlighted)
    // The read-along bar (G1): the Blocks of each Reading Picked in this Session, by the Reading's id, and
    // the bars under the word that is heard now (Shot coordinates). Only this Session's Readings are
    // known, so a word of an earlier Session's Reading draws nothing.
    ReadAlongTrack track;
    const std::function<std::optional<HeardWord>()>* heardWord = nullptr;
    std::vector<Rect> bars;
    int barThickness = 3, barGap = 1;  // px, scaled for the DPI
    OverlayStyle style;
    ControlBar bar;         // the Control bar above the Overlay; its windows belong to this Session
    int hoverTol = 8;       // the Block hover tolerance (FindBlockAt), scaled for the DPI
    Selection result;       // the Pick the window procedure just recorded

    const Recognizer* recognize = nullptr;
    JobQueue* jobs = nullptr;
    const SelectHandler* onSelect = nullptr;
    unsigned seenEpoch = 0;  // OverlayCallbacks::languageEpoch as of the last Analysis
    std::deque<Selection> picks;  // Picks not handled yet, oldest first
    // The Drag being recognised on a job thread. The job writes dragResult, then sets dragDone
    // and posts kMsgPickDone; only then does the UI thread read dragResult.
    bool dragRunning = false;
    std::atomic<bool> dragDone{false};
    Selection dragSel;
    Recognition dragResult;
};

static const UINT kMsgAnalysisDone = WM_APP + 1;
static const UINT kMsgPickDone = WM_APP + 2;  // a Drag job finished; the loop's Pump picks it up
// A window timer that asks which word is heard. The speech thread cannot post a message at each word
// (it would need a channel into the Session), and 30 ms is well under the length of a spoken word.
static const UINT_PTR kTimerReadAlong = 1;
static const UINT kReadAlongMs = 30;

// The Drag's rectangle from where the button went down to where the mouse is now, in either direction,
// kept on the Shot.
static RECT NormalizedDragRect(const Session& ss) {
    RECT r;
    r.left = std::max<LONG>(0, std::min(ss.start.x, ss.cur.x));
    r.top = std::max<LONG>(0, std::min(ss.start.y, ss.cur.y));
    r.right = std::min<LONG>(ss.shot->w, std::max(ss.start.x, ss.cur.x));
    r.bottom = std::min<LONG>(ss.shot->h, std::max(ss.start.y, ss.cur.y));
    return r;
}

// Shows area `r` at full brightness (copied from the original Shot over the dimmed one) with a frame
// of `thickness` px around it, drawn outwards.
static void DrawHighlight(HDC dst, const Shot& s, RECT r, COLORREF colour, int thickness) {
    const int rw = r.right - r.left, rh = r.bottom - r.top;
    if (rw <= 0 || rh <= 0) return;
    BitBlt(dst, r.left, r.top, rw, rh, s.origDc, r.left, r.top, SRCCOPY);  // un-dim
    HBRUSH br = CreateSolidBrush(colour);
    for (int i = 0; i < thickness; ++i) {
        FrameRect(dst, &r, br);
        InflateRect(&r, 1, 1);
    }
    DeleteObject(br);
}

// A Block's box as it is highlighted: a little bigger, but never off the Shot.
static RECT HighlightRect(const Rect& box, const Shot& s) {
    RECT r = ToRECT(box);
    InflateRect(&r, 3, 3);
    r.left = std::max<LONG>(0, r.left);
    r.top = std::max<LONG>(0, r.top);
    r.right = std::min<LONG>(s.w, r.right);
    r.bottom = std::min<LONG>(s.h, r.bottom);
    return r;
}

// Paints the Overlay: the dimmed Shot, the areas already Picked, then the Drag rectangle or the Block
// under the mouse. Everything is drawn into the back buffer first and copied in one go, so the
// picture does not flicker.
static void PaintOverlay(HWND hwnd, Session& ss) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    const Shot& s = *ss.shot;

    BitBlt(ss.backDc, 0, 0, s.w, s.h, s.dimDc, 0, 0, SRCCOPY);

    for (const RECT& pr : ss.picked) DrawHighlight(ss.backDc, s, pr, ss.style.pickedColor, 2);

    // The word being read: a bar under it, over the Picked frames and under the hover / drag frame.
    if (!ss.bars.empty()) {
        HBRUSH bar = CreateSolidBrush(ss.style.hoverColor);
        for (const Rect& b : ss.bars) {
            // A word at the edge of the Shot: keep the bar on it.
            const RECT r = ToRECT(ClipToArea(b, s.w, s.h));
            if (r.right > r.left && r.bottom > r.top) FillRect(ss.backDc, &r, bar);
        }
        DeleteObject(bar);
    }

    if (ss.dragging) {
        DrawHighlight(ss.backDc, s, NormalizedDragRect(ss), ss.style.hoverColor, 1);
    } else if (ss.hover >= 0 && ss.ready) {
        DrawHighlight(ss.backDc, s, HighlightRect(ss.analysis.blocks[ss.hover].box, s), ss.style.hoverColor, 2);
    }

    BitBlt(hdc, 0, 0, s.w, s.h, ss.backDc, 0, 0, SRCCOPY);
    EndPaint(hwnd, &ps);
}

// The Block under the mouse (once the Analysis is done); a repaint only when it changed.
static void UpdateHover(HWND hwnd, Session& ss, POINT p) {
    int idx = -1;
    if (ss.ready) idx = FindBlockAt(ss.analysis.blocks, ToPoint(p), ss.hoverTol);
    if (idx != ss.hover) {
        ss.hover = idx;
        InvalidateRect(hwnd, nullptr, FALSE);
    }
}

// The hover highlight again for wherever the mouse is now, and a repaint.
static void RefreshHover(HWND hwnd, Session& ss) {
    POINT p;
    GetCursorPos(&p);
    ScreenToClient(hwnd, &p);
    ss.hover = -2;  // force a refresh
    UpdateHover(hwnd, ss, p);
    InvalidateRect(hwnd, nullptr, FALSE);
}

// Asks which word is heard and moves the bars to it; repaints only when the bars changed (a word lasts
// a fraction of a second, a timer tick much less). Nothing to do before the first Pick of the Session.
static void PollReadAlong(HWND hwnd, Session& ss) {
    if (!ss.heardWord || !*ss.heardWord || ss.track.Empty()) return;
    const std::vector<Rect> bars = ss.track.Bars((*ss.heardWord)(), ss.barThickness, ss.barGap);
    if (SameRects(bars, ss.bars)) return;
    ss.bars = bars;
    InvalidateRect(hwnd, nullptr, FALSE);
}

// The Overlay window's procedure: painting, the mouse (Pick, Drag, Leave), and the job results that
// come back as messages. The window never takes focus (WS_EX_NOACTIVATE, ADR-0001), so key presses
// normally go to the game; Lexia's own keys are global hotkeys. Esc is handled for the rare case
// that the Overlay has the keyboard.
static LRESULT CALLBACK OverlayProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    // Set right after CreateWindowEx: messages sent while the window is created have no Session.
    Session* sp = reinterpret_cast<Session*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!sp) return DefWindowProcW(hwnd, msg, wp, lp);
    Session& ss = *sp;

    switch (msg) {
    case WM_ERASEBKGND:
        return 1;

    case WM_MOUSEACTIVATE:  // clicking the overlay must not steal focus from the game
        return MA_NOACTIVATE;

    case WM_SETCURSOR:
        SetCursor(LoadCursorW(nullptr, IDC_CROSS));
        return TRUE;

    case WM_PAINT:
        PaintOverlay(hwnd, ss);
        return 0;

    case WM_TIMER:
        if (wp == kTimerReadAlong) {
            PollReadAlong(hwnd, ss);
            return 0;
        }
        break;

    // The button that Picks (click or drag) and the one that Leaves depend on the left-handed setting
    // (mouse_role.h); the handlers below are the same for both, only the role of the message differs.
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN: {
        const MouseButton button = msg == WM_LBUTTONDOWN ? MouseButton::Left : MouseButton::Right;
        if (RoleOf(button, ss.style.leftHanded) == MouseRole::Leave) {
            if (ss.dragging) {
                ReleaseCapture();
                ss.dragging = false;
            }
            ss.done = true;
            return 0;
        }
        SetCapture(hwnd);
        ss.dragging = true;
        ss.start.x = ss.cur.x = GET_X_LPARAM(lp);
        ss.start.y = ss.cur.y = GET_Y_LPARAM(lp);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_MOUSEMOVE: {
        POINT p = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        if (ss.dragging) {
            ss.cur = p;
            InvalidateRect(hwnd, nullptr, FALSE);
        } else {
            UpdateHover(hwnd, ss, p);
        }
        return 0;
    }

    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
        if (ss.dragging && RoleOf(msg == WM_LBUTTONUP ? MouseButton::Left : MouseButton::Right, ss.style.leftHanded) ==
                               MouseRole::Pick) {
            ReleaseCapture();
            ss.dragging = false;
            ss.cur.x = GET_X_LPARAM(lp);
            ss.cur.y = GET_Y_LPARAM(lp);
            const LONG dx = std::labs(ss.cur.x - ss.start.x);
            const LONG dy = std::labs(ss.cur.y - ss.start.y);
            if (dx < 6 && dy < 6) {
                ss.result.kind = SelKind::Click;
                ss.result.point = ss.cur;
            } else {
                ss.result.kind = SelKind::Drag;
                ss.result.rect = NormalizedDragRect(ss);
            }
            ss.pending = true;  // the message loop reads it; the overlay stays open
        }
        return 0;

    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) {
            ss.result.kind = SelKind::Cancel;
            ss.done = true;
        }
        return 0;

    default:
        if (msg == kMsgAnalysisDone) {  // background analysis finished
            ss.bar.SetAnalysing(!ss.ready);
            RefreshHover(hwnd, ss);
            return 0;
        }
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// Recognition can throw (cv::Exception, bad_alloc...): an Analysis or Drag that fails reads as
// "no text found" instead of ending the program, and never leaves a Click held for ever.
static Recognition RecognizeSafely(const Recognizer& recognize, Rect area, bool isDrag) {
    try {
        return recognize(area, isDrag);
    } catch (const std::exception& e) {
        LogError("Recognition failed: ", e.what());  // never the text itself (ADR-0005)
        return Recognition{};
    }
}

// Starts an Analysis of the whole Shot as a job; the previous one, if still running, is ignored
// when it ends.
static void StartAnalysis(HWND hwnd, Session& ss) {
    unsigned gen;
    {
        std::lock_guard<std::mutex> lock(ss.mu);
        gen = ++ss.generation;
        ss.ready = false;
        ss.analysis = Recognition{};
    }
    ss.hover = -1;
    ss.bar.SetAnalysing(true);  // Pause, Skip and Stop are greyed until kMsgAnalysisDone
    InvalidateRect(hwnd, nullptr, FALSE);
    Session* sp = &ss;
    ss.jobs->Post([sp, hwnd, gen]() {
        const Rect all = {0, 0, sp->shot->w, sp->shot->h};
        Recognition r = RecognizeSafely(*sp->recognize, all, /*isDrag=*/false);
        {
            std::lock_guard<std::mutex> lock(sp->mu);
            if (gen != sp->generation) return;  // a newer Analysis has been started
            sp->analysis = std::move(r);
            sp->ready = true;
        }
        PostMessageW(hwnd, kMsgAnalysisDone, 0, 0);
    });
}

// Highlights the area of a finished Pick if the handler found text there.
static void FinishPick(Session& ss, const Selection& sel, const Recognition& picked) {
    const PickOutcome outcome = (*ss.onSelect)(sel, picked);
    if (!outcome.found) return;
    ss.picked.push_back(sel.kind == SelKind::Drag ? sel.rect : HighlightRect(picked.blocks[0].box, *ss.shot));
    // Keep the Blocks (Shot coordinates) under the Reading's id, to find the heard word's box in them.
    ss.track.Add(outcome.readingId, picked.blocks);
}

// Moves the Picks along, in the order they were made: a finished Drag is handed to the handler,
// the next Drag is sent to the JobQueue, a Click is handled once the Analysis is done (until then
// it, and every Pick after it, waits here).
static void Pump(HWND hwnd, Session& ss) {
    bool finished = false;
    if (ss.dragRunning && ss.dragDone) {
        ss.dragRunning = false;
        ss.dragDone = false;
        FinishPick(ss, ss.dragSel, ss.dragResult);
        finished = true;
    }

    while (!ss.dragRunning && !ss.picks.empty()) {
        Selection sel = ss.picks.front();
        if (sel.kind == SelKind::Click) {
            if (!ss.ready) break;
            ss.picks.pop_front();
            sel.block = FindBlockAt(ss.analysis.blocks, ToPoint(sel.point), ss.hoverTol);
            // A Click on no Block is a Pick that found no text, once the Analysis is done: the
            // handler gets it with no Blocks and announces it.
            Recognition one;
            one.ms = ss.analysis.ms;
            if (sel.block >= 0) one.blocks.push_back(ss.analysis.blocks[static_cast<size_t>(sel.block)]);
            FinishPick(ss, sel, one);
            finished = true;
        } else {
            ss.picks.pop_front();
            ss.dragRunning = true;
            ss.dragDone = false;
            ss.dragSel = sel;
            Session* sp = &ss;
            ss.jobs->Post([sp, hwnd, sel]() {
                sp->dragResult = RecognizeSafely(*sp->recognize, ToRect(sel.rect), /*isDrag=*/true);
                sp->dragDone = true;
                PostMessageW(hwnd, kMsgPickDone, 0, 0);
            });
        }
    }

    const bool busy = ss.dragRunning || !ss.picks.empty();
    if (busy != ss.busy || finished) {
        ss.busy = busy;
        RefreshHover(hwnd, ss);
    }
}

// Shows the frozen screenshot full-screen, with the Control bar above it, and keeps it up until the
// user leaves (F8 again, right-click, Esc or the bar's Close). Each click/drag is passed to
// `onSelect`, which reads the text aloud and says if it found any (the area then stays
// highlighted). While the user looks at the screen, a job finds all text blocks so hovering and
// clicking are instant.
OverlayResult RunOverlay(const Shot& shot, const Recognizer& recognize, JobQueue& jobs, const OverlayStyle& style,
                       const OverlayCallbacks& cb) {
    static bool registered = false;
    HINSTANCE inst = GetModuleHandleW(nullptr);
    if (!registered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = OverlayProc;
        wc.hInstance = inst;
        wc.hCursor = LoadCursorW(nullptr, IDC_CROSS);
        wc.lpszClassName = L"LexiaOverlay";
        RegisterClassExW(&wc);
        registered = true;
    }

    Session ss;
    ss.shot = &shot;
    ss.recognize = &recognize;
    ss.jobs = &jobs;
    ss.onSelect = &cb.onSelect;
    if (cb.languageEpoch) ss.seenEpoch = *cb.languageEpoch;
    ss.style = style;

    HWND hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"LexiaOverlay", L"Lexia",
                                WS_POPUP, shot.ox, shot.oy, shot.w, shot.h, nullptr, nullptr, inst,
                                nullptr);
    if (!hwnd) return {};
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&ss));
    const int dpi = std::max(96, static_cast<int>(GetDpiForWindow(hwnd)));
    ss.hoverTol = ScaleForDpi(8, dpi);
    ss.barThickness = ScaleForDpi(3, dpi);
    ss.barGap = ScaleForDpi(1, dpi);
    // Read-along off: no heard-word source, so no bar is ever drawn and no timer is started below.
    ss.heardWord = style.readAlong ? &cb.heardWord : nullptr;

    HDC winDc = GetDC(hwnd);
    ss.backDc = CreateCompatibleDC(winDc);
    ss.backBmp = CreateCompatibleBitmap(winDc, shot.w, shot.h);
    ss.backOld = SelectObject(ss.backDc, ss.backBmp);
    ReleaseDC(hwnd, winDc);

    // Show the overlay WITHOUT taking focus: fullscreen games minimise themselves
    // when they lose focus, so the game must stay the foreground window.
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    UpdateWindow(hwnd);
    const bool escHotkey = RegisterHotKey(nullptr, HK_ESC, MOD_NOREPEAT, VK_ESCAPE);

    // The Control bar: its own windows above the Overlay, never active. If it cannot be made the
    // Session still works with the keys and the mouse.
    ControlBarConfig bc;
    bc.owner = hwnd;
    bc.monitor = RECT{shot.ox, shot.oy, shot.ox + shot.w, shot.oy + shot.h};
    bc.size = style.barSize;
    bc.savedX = style.barX;
    bc.savedY = style.barY;
    bc.theme = style.theme;
    bc.interfaceLanguage = style.interfaceLanguage;
    bc.pauseKey = style.pauseKey;
    bc.speechKey = style.speechKey;
    bc.leftHanded = style.leftHanded;
    bc.speechState = cb.speechState;
    bc.onPause = cb.onPause;
    bc.onSkip = cb.onSkip;
    bc.onStop = cb.onStop;
    bc.onHelp = [&ss]() {
        ss.helpAsked = true;
        ss.done = true;
    };
    bc.onChanged = cb.onBarChanged;
    // The mouse is on the bar now, so the Overlay no longer hovers whatever Block it last passed.
    bc.onPointerEntered = [&ss, hwnd]() {
        if (ss.hover != -1) {
            ss.hover = -1;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
    };
    if (!ss.bar.Create(bc)) LogWarn("The Control bar could not be shown; use the keys.");

    StartAnalysis(hwnd, ss);
    // A failed SetTimer only means no read-along bar; the Session works without it.
    const bool readAlongTimer = style.readAlong && cb.heardWord && SetTimer(hwnd, kTimerReadAlong, kReadAlongMs, nullptr) != 0;

    MSG msg;
    while (!ss.done) {
        const BOOL r = GetMessageW(&msg, nullptr, 0, 0);
        if (r <= 0) {
            if (r == 0) PostQuitMessage(0);  // pass a quit request on to the main loop
            break;
        }

        // Global hotkeys arrive as thread messages (no window), so handle them here.
        if (msg.message == WM_HOTKEY) {
            if (msg.wParam == HK_READ) {
                ss.done = true;  // F8 again = leave the overlay
            } else if (msg.wParam == HK_ESC) {
                ss.done = true;  // Esc = leave the overlay
            } else if (msg.wParam == HK_PAUSE) {
                cb.onPause();
            } else if (msg.wParam == HK_SPEECH) {
                cb.onSpeechKey();
            } else if (msg.wParam == HK_LANG) {
                cb.onSwitchLanguage();
            }
        } else {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        // A Language switch (this key or any other way) makes the Analysis out of date. The
        // Shot stays, so read it again with the new Language; the Picked areas are kept.
        if (!ss.done && cb.languageEpoch && *cb.languageEpoch != ss.seenEpoch) {
            ss.seenEpoch = *cb.languageEpoch;
            StartAnalysis(hwnd, ss);
        }

        if (ss.pending) {
            ss.pending = false;
            if (!ss.done) ss.picks.push_back(ss.result);
        }
        if (!ss.done) Pump(hwnd, ss);
    }

    if (escHotkey) UnregisterHotKey(nullptr, HK_ESC);
    if (readAlongTimer) KillTimer(hwnd, kTimerReadAlong);
    ss.bar.Destroy();
    ShowWindow(hwnd, SW_HIDE);
    // A job still running reads the Shot and writes into `ss`: wait for it before they go away
    // and before anyone else uses the OCR engine. Picks that were still waiting are dropped.
    jobs.CancelPending();
    jobs.WaitIdle();

    SelectObject(ss.backDc, ss.backOld);
    DeleteObject(ss.backBmp);
    DeleteDC(ss.backDc);
    DestroyWindow(hwnd);
    // Only now, with nothing of the Session left on screen, may another window of Lexia take focus.
    OverlayResult result;
    result.track = std::move(ss.track);  // for the read-along layer (G2); the Session's own copy is done with
    if (ss.helpAsked && cb.onHelp) cb.onHelp();
    return result;
}
