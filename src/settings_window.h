#pragma once
// The Settings window: one top-level window that draws the pages of the native widget tree
// (native_ui/settings_ui.*) with Direct2D (ADR-0009, which replaced WebView2 and ui/). Thin Win32 glue: it
// turns window messages into toolkit events (mouse, keys, wheel), paints the tree on a D2dCanvas, shows the
// tooltip and the cursor the tree asks for, and carries typed values between the tree and its owner:
// `onCommand(PageMessage)` out, SetState / ShowPage / SetDark in. ui_host.* has the messages, App the actions.
//
// It is a normal window of a fixed size (800 x 600 DIPs client area, not resizable) that takes focus (it is
// not the Overlay). Created when the user opens it and destroyed when they close it. It never touches the
// network and loads no file: the icons, engravings and the font are resources of the exe.
//
// Everything runs on the UI thread. The owner must not delete this object from inside one of its
// callbacks: `onDestroyed` is called when the window is gone, and the owner is to delete it later
// (App posts itself a message). `onCommand` runs after the event that caused it has been dealt with
// (Screen::Defer), so the owner may call SetState / ShowPage from it.
#include <windows.h>

#include <functional>
#include <memory>
#include <string>

#include "native_ui/pages/page.h"
#include "ui_host.h"

class SettingsWindow {
public:
    struct Options {
        std::wstring title;               // the window's title (strings window.title)
        std::string interfaceLanguage = "en";  // the language of the texts of the pages
        bool dark = false;                // the title bar and the paper before the first state
        nui::Page startPage = nui::Page::Home;
        std::function<void(const PageMessage&)> onCommand;  // a user action of the pages
        std::function<void()> onSystemThemeChanged;  // Windows changed its light / dark setting
        std::function<void()> onDestroyed;           // the window is gone (closed)
    };

    explicit SettingsWindow(Options options);
    ~SettingsWindow();
    SettingsWindow(const SettingsWindow&) = delete;
    SettingsWindow& operator=(const SettingsWindow&) = delete;

    // Makes the window, centred on `monitor`, and shows it. False if the window or its Direct2D canvas could
    // not be made (logged).
    bool Create(const RECT& monitor);

    void BringToFront();                     // restores it if minimised, and takes focus
    void ShowPage(nui::Page page);           // switches the page
    void SetState(const UiState& state);     // redraws from the state (the theme in it included)
    void SetDark(bool dark);                 // the title bar and the paper; SetState does it from state.dark
    HWND Handle() const { return hwnd_; }

private:
    struct Impl;
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT OnMessage(UINT msg, WPARAM wp, LPARAM lp);

    Options options_;
    HWND hwnd_ = nullptr;
    std::unique_ptr<Impl> impl_;
};
