// Lexia - reads text aloud from anywhere on the screen (OCR + text to speech).
//
// Default keys (all of them can be changed in settings.json):
//   F8          freeze the screen and open the selection overlay.
//               Hover = preview what will be read, click = read that block,
//               drag = read an area. Every selection is read straight away
//               (queued one after another) and the overlay STAYS OPEN, so you
//               can select as much as you need on the screen.
//               Leave the overlay with F8 again, right-click, or Esc.
//   F7          pause / resume reading (only while Lexia is speaking or paused)
//   F6          skip a Block; twice quickly = stop (only while speaking or paused)
//   (no key)    switch Language; bind "switchLanguage" in settings.json
//
// Modules:  app       (the state and actions; hotkeys call them)
//           settings  (settings.json)
//           capture   (screen capture, which monitor)
//           ocr       (PaddleOCR via ONNX Runtime)
//           layout    (lines -> blocks, reading order)
//           overlay   (the selection overlay window)
//           speech    (Piper text to speech)
//           language  (per-language data)
//           log       (console + %LOCALAPPDATA%\Lexia\lexia.log)

#include <windows.h>

#include <cstdio>
#include <exception>
#include <string>

#include "app.h"
#include "command_line.h"
#include "common.h"
#include "elevation.h"
#include "instance.h"
#include "settings.h"
#include "stale_data.h"
#include "version.h"
#include "log.h"
#include "strings/string_ids.h"
#include "strings/strings.h"

// How long an --elevated-relaunch start waits for the instance that started it to end.
static constexpr unsigned kElevatedRelaunchWaitMs = 5000;

// The App lives inside this function, so it is destroyed (speech, keys, windows) before wWinMain
// shuts COM down.
static int Run() {
    App app;
    if (!app.Init()) return 1;
    return app.Run();
}

// --console: a console window for development, so the log lines and ConsolePrint show and Ctrl+C works.
static void OpenConsole() {
    if (!AllocConsole()) return;
    FILE* f = nullptr;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONOUT$", "w", stderr);
    SetConsoleOutputCP(CP_UTF8);
}

// lexia.exe starts here. The order matters: the DLL search path, the command line, one instance only,
// Run as admin (which may start an elevated Lexia and end this one), DPI and COM, then the App. A
// failure to start is a message box, as there is no console to show it in.
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    // First: DLLs loaded from now on come only from the exe's folder and System32, never from
    // the current folder or PATH, where a planted DLL could be picked up.
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);

    const CommandLine cmd = ParseCommandLine(ProgramArguments());

    // One Lexia at a time: a second launch only asks the first to show its Settings window. An elevated
    // relaunch (ADR-0008) waits a few seconds instead: the instance that started it is ending and gives the
    // mutex up.
    SingleInstance instance(cmd.elevatedRelaunch ? kElevatedRelaunchWaitMs : 0);
    if (!instance.IsFirst()) {
        SingleInstance::AskFirstToOpenSettings();
        return 0;
    }

    // Run as admin (ADR-0008): when the setting is on and this Lexia is not elevated, start it again with
    // the "runas" verb (the UAC prompt) and end. The mutex goes first, so that the new one can take it. A
    // relaunch that was started that way never relaunches again. A cancelled prompt or a failure is logged
    // and Lexia goes on unelevated.
    if (!cmd.elevatedRelaunch && RunAsAdminSetting() && !IsElevated()) {
        instance.Release();
        if (RelaunchElevated(Utf8ToWide(RelaunchArgs(cmd.minimised))) == RelaunchResult::Started) return 0;
        instance.Acquire();
        if (!instance.IsFirst()) {  // another Lexia started meanwhile
            SingleInstance::AskFirstToOpenSettings();
            return 0;
        }
    }

    if (cmd.console) OpenConsole();
    LogInfo("---- Lexia ", LEXIA_VERSION, " started ----");
    for (const std::string& arg : cmd.unknown) LogWarn("Unknown command-line argument ignored: ", arg);

    // Lexia 0.5's Settings window kept a WebView2 profile here. The native window does not use it, so it is
    // deleted once (junctions and links inside are not followed: stale_data.h).
    switch (RemoveStaleWebView2Data(LexiaDataDir())) {
    case StaleRemoval::Absent: break;
    case StaleRemoval::Removed:
        LogInfo("Removed the old WebView2 data folder of Lexia 0.5 (%LOCALAPPDATA%\\Lexia\\WebView2).");
        break;
    case StaleRemoval::Failed:
        LogWarn("The old WebView2 data folder of Lexia 0.5 could not be removed completely; it is tried again at the next start.");
        break;
    }

    // Real pixels on every monitor, even when the monitors use different scaling (the manifest
    // asks for it too).
    if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
        SetProcessDPIAware();
    }

    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) {
        LogError("COM initialisation failed.");
        return 1;
    }

    // Run() owns the speech engine, so it is released before COM is shut down. Whatever escapes
    // it is logged: an uncaught exception would end the program with no message at all.
    int rc = 1;
    try {
        rc = Run();
    } catch (const std::exception& e) {
        LogError("Lexia stopped because of an unexpected error: ", e.what());
    } catch (...) {
        LogError("Lexia stopped because of an unexpected error that is not a std::exception.");
    }
    if (rc != 0 && !cmd.console) {
        // There is no console to read the reason from.
        MessageBoxW(nullptr, Utf8ToWide(strings::Text("en", strings::id::kAppStartFailed)).c_str(),
                    Utf8ToWide(strings::Text("en", strings::id::kWindowTitle)).c_str(), MB_OK | MB_ICONERROR);
    }
    CoUninitialize();
    return rc;
}
