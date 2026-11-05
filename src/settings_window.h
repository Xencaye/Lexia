#pragma once
// The Settings window: one top-level window that hosts WebView2 and shows the pages of ui/
// (ADR-0006). Thin Win32 glue: it carries JSON text between the page and its owner and decides
// nothing; ui_host.* has the messages, App has the actions.
//
// It is a normal window of a fixed size (800 x 600 DIPs client area, not resizable) that takes focus (it is not the Overlay). Created when the user opens it
// and destroyed when they close it. It never touches the network: the pages are served from the
// ui\ folder through a virtual host name, and every other request is refused (ADR-0005).
//
// Everything runs on the UI thread. The owner must not delete this object from inside one of its
// callbacks: `onDestroyed` is called when the window is gone, and the owner is to delete it later
// (App posts itself a message).
#include <windows.h>

#include <filesystem>
#include <functional>
#include <memory>
#include <string>

class SettingsWindow {
public:
    struct Options {
        std::wstring title;          // the window's title (strings window.title)
        std::wstring noRuntimeText;  // the message box when the WebView2 Runtime is missing
        std::filesystem::path uiDir; // the folder with index.html
        bool dark = false;           // title bar and the colour shown before the page has drawn
        std::wstring startPage;      // "" = the Home page; else a page name for ?page= (lower-case letters)
        std::function<void(const std::string& json)> onMessage;  // one message from the page
        std::function<void()> onSystemThemeChanged;  // Windows changed its light / dark setting
        std::function<void()> onDestroyed;           // the window is gone (closed, or WebView2 failed)
    };

    explicit SettingsWindow(Options options);
    ~SettingsWindow();
    SettingsWindow(const SettingsWindow&) = delete;
    SettingsWindow& operator=(const SettingsWindow&) = delete;

    // Makes the window, centred on `monitor`, and starts WebView2. False if the window could not be
    // made (logged). A missing WebView2 Runtime is logged and shown in a message box; the window
    // then closes by itself and onDestroyed is called.
    bool Create(const RECT& monitor);

    void BringToFront();        // restores it if minimised, and takes focus
    void SetDark(bool dark);    // the title bar, and the colour behind the page
    void ShowPage(const std::wstring& page);  // loads the pages again on `page` ("" = the start page); the page sends `ready` again
    void Post(const std::string& json);  // to the page; dropped until WebView2 is ready
    HWND Handle() const { return hwnd_; }

private:
    struct Impl;
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT OnMessage(UINT msg, WPARAM wp, LPARAM lp);

    Options options_;
    HWND hwnd_ = nullptr;
    bool dark_ = false;
    std::unique_ptr<Impl> impl_;  // the WebView2 objects
    std::shared_ptr<bool> alive_;  // WebView2's callbacks may come after this object is gone
};

