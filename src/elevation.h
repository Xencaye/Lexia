#pragma once
// Running elevated ("Run as administrator", ADR-0008), the Win32 side. Some games run elevated,
// and Windows (UIPI) then blocks a non-elevated Lexia's Overlay and hotkeys over them. The
// switches are command_line.h (--elevated-relaunch, RelaunchArgs); the decision when to relaunch
// is App's.
#include <string>

// Does this process run with an elevated token (TokenElevation)? False if it cannot be told, and
// for a standard user on a system with UAC off (there nothing is elevated).
bool IsElevated();

enum class RelaunchResult {
    Started,    // the elevated process was started: the caller should end this instance
    Cancelled,  // the person said no in the UAC prompt: carry on unelevated
    Failed,     // anything else (logged)
};

// Starts this exe again with `args` (wide, e.g. Utf8ToWide(RelaunchArgs(minimised))) and the
// "runas" verb, which shows the UAC prompt. Returns when the process is started or the prompt is
// answered; does not wait for the new process.
RelaunchResult RelaunchElevated(const std::wstring& args);
