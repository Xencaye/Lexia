#pragma once
// Run at startup (D6), the Win32 side: the Run key of the current user and the command line of this
// process. The pure part (the value's text) is startup.h, the switches are command_line.h. The registry is the
// truth for "Run at startup"; settings.json only mirrors it.
#include <string>

// Is there a Lexia value in HKCU\Software\Microsoft\Windows\CurrentVersion\Run?
bool ReadRunAtStartup();

// On: writes the value (RunValueText: the exe, and --minimised when `minimised`), which also brings an
// old value up to date (another path, another switch). Off: removes it. False, logged, if the registry refused.
bool WriteRunAtStartup(bool on, bool minimised);

// Was this process started with --minimised?
bool StartedMinimised();
