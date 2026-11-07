; Lexia's installer (task H4), an Inno Setup 6 script. It installs the package folder that the build
; makes (build\package\Lexia: lexia.exe, the DLLs, models, Voices, notices) and replaces Lexia.zip
; for people who are not developers.
;
; Built by CMake when the build is configured with -DLEXIA_INSTALLER=ON (cmake/installer.cmake runs
; ISCC after the package step), or by hand:
;   ISCC /DAppVersion=0.5.0 /DSourceDir=<build>\package\Lexia /DOutputDir=<build>\package installer\lexia.iss
;
; What it does, and why:
; - Installs for the current user by default (no UAC prompt), in %LOCALAPPDATA%\Programs\Lexia. The
;   first page lets a person with admin rights install for all users (Program Files) instead. Lexia's
;   own data (settings.json, the log, the user's Voices and Presets) is always in %LOCALAPPDATA%\Lexia.
; - Run at startup is an optional task. It writes the same Run value that Lexia itself writes
;   ("<exe>" --minimised: Start minimised is on by default), and Lexia keeps it up to date at each start.
; - The last page starts Lexia: it opens its Settings window (no --minimised) and is Off until the
;   user turns it on (D9).
; - Lexia's Settings window is Lexia's own (Direct2D, ADR-0009): it needs nothing beyond Windows itself,
;   so the installer carries no runtime and does not use the network.
; - An update installs over the old Lexia. A running Lexia (its mutex) must be closed first, and Setup and the
;   uninstaller do it for the person (K.7, close_lexia.isi): they ask, post the registered message Lexia.Quit
;   (a clean quit, as the tray's Quit), wait up to 10 s, and only then offer to end Lexia by force (taskkill,
;   with one UAC prompt for an elevated Lexia). SILENT RUNS (/SILENT, /VERYSILENT, silent uninstall) never ask:
;   they send Lexia.Quit, wait, then run taskkill as the user (Lexia keeps no unsaved work); a Lexia that is
;   elevated and still runs after that stops the run (a silent run cannot show a UAC prompt).
; - "Run Lexia as administrator" is a task. Its start state is what the settings file of the person who runs
;   Setup says ("runAsAdmin"), and the choice is written after the install by lexia.exe --set-run-as-admin
;   (as the original user, also in an all-users install, so it is that person's own settings.json). With it on,
;   "Start Lexia now" on the last page makes Lexia relaunch itself elevated: Windows shows its UAC prompt.
;   A silent run changes the setting only when /TASKS=runasadmin asks for it (it never turns it off).
; - The uninstaller removes the program and the Run value, and asks whether to remove the user's data too.
;
; The installer's words are here, in [Messages] / [CustomMessages], not in src/strings: Inno Setup shows
; its own pages and cannot read Lexia's string table. English only, like Lexia 0.5 (ADR-0007).

#ifndef AppVersion
  #error AppVersion is not set: pass /DAppVersion=<major.minor.patch> (the version in vcpkg.json)
#endif
#ifndef SourceDir
  #error SourceDir is not set: pass /DSourceDir=<the package folder, build\package\Lexia>
#endif
#ifndef OutputDir
  #define OutputDir "."
#endif

#define AppName "Lexia"
#define AppExe "lexia.exe"
; The mutex of a running Lexia (src/instance.cpp: Local\Lexia.SingleInstance, the session namespace) and the
; class of its hidden App window (src/instance.h, kAppWindowClass), which Setup posts Lexia.Quit to.
#define MutexName "Lexia.SingleInstance"
#define AppWindowClass "LexiaApp"

[Setup]
; AppId names Lexia for Windows (Apps & features) and for updates. Never change it: a new id would
; install a second Lexia beside the first.
AppId={{6F28CA60-4B09-4F9A-AD8A-9C21EAC5DFCB}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=Xencaye
AppPublisherURL=https://xencaye.itch.io/lexia
AppSupportURL=https://github.com/Xencaye/Lexia/issues
VersionInfoVersion={#AppVersion}
; {autopf} is %LOCALAPPDATA%\Programs for a per-user install and Program Files for all users.
DefaultDirName={autopf}\{#AppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
; 64-bit Windows 10 1809 or newer: Lexia is x64, and the Control bar's SVG icons need Direct2D's SVG
; support (Windows 10 1703+). 1809 (build 17763) is a conservative floor above that; lower it only after
; a test on an older Windows 10.
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
OutputDir={#OutputDir}
OutputBaseFilename={#AppName}-{#AppVersion}-setup
SetupIconFile=..\src\res\lexia.ico
UninstallDisplayIcon={app}\{#AppExe}
UninstallDisplayName={#AppName}
; modern, and dynamic: its pages are light or dark as Windows' app mode is (Inno Setup 6.6 or newer).
WizardStyle=modern dynamic
Compression=lzma2/max
SolidCompression=yes
; The licence is GPL-3.0-or-later, which is not an agreement to accept: it is installed beside the
; program (LICENSE, THIRD-PARTY-NOTICES.txt), not shown as a page to agree to.

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
english.TaskStartup=Start Lexia when I sign in to Windows (in the tray)
english.TaskDesktop=Create a desktop shortcut
english.LaunchLexia=Start Lexia now
english.WhoSetup=Setup
english.WhoUninstall=The uninstaller
english.LexiaRunning=Lexia is running.%n%n%1 can close it for you. Choose Yes to close Lexia now, or No to stop.
english.LexiaStillRunning=Lexia is still running.%n%nChoose Yes to try to close it again, or No to stop.
english.LexiaForce=Lexia did not close by itself.%n%nIt can be ended by force. Lexia keeps no unsaved work, but any reading in progress stops. If Lexia runs as administrator, Windows asks for permission first.%n%nEnd Lexia now?
english.LexiaStillRunningSetup=Lexia is still running, so Setup cannot replace its files. Close Lexia (right-click its icon in the tray, then Quit), and run Setup again.
english.TaskRunAsAdmin=Run Lexia as administrator (Windows asks for permission at each start; needed to read games that run as administrator)
english.RemoveUserData=Also remove your Lexia settings, log, Voices and Presets?%n%n%1%n%nChoose No to keep them for a later install.

[Tasks]
Name: "startup"; Description: "{cm:TaskStartup}"; Flags: unchecked
Name: "desktopicon"; Description: "{cm:TaskDesktop}"; Flags: unchecked
; Unchecked here, but CurPageChanged sets the real start state from the settings file (UsePreviousTasks would
; otherwise bring back the choice of the last install, which the person may have changed in Lexia since).
Name: "runasadmin"; Description: "{cm:TaskRunAsAdmin}"; Flags: unchecked

[Files]
; The whole package folder, as the build made it. ignoreversion: Lexia's files always replace the old ones.
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[InstallDelete]
; An update starts from a clean program folder, so a file that a newer Lexia no longer ships (an old DLL,
; such as WebView2Loader.dll) is not left behind. The user's data is not in {app}.
; {app}\ui held the HTML Settings window of Lexia 0.5, which 0.6 replaced with a native one (ADR-0009):
; an update of 0.5 removes it.
Type: filesandordirs; Name: "{app}\ui"
Type: filesandordirs; Name: "{app}\espeak-ng-data"
Type: filesandordirs; Name: "{app}\models"
Type: filesandordirs; Name: "{app}\voices"
Type: filesandordirs; Name: "{app}\licenses"
Type: files; Name: "{app}\*.dll"

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Registry]
; Run at startup, as Lexia writes it (src/startup.cpp, RunValueText). Lexia writes it again at each
; start, with the Start minimised setting the user chose.
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "{#AppName}"; \
    ValueData: """{app}\{#AppExe}"" --minimised"; Tasks: startup

[Run]
; Run as admin (K.7): lexia.exe saves only that setting and ends (no window, no elevation). runasoriginaluser:
; the setting is the installing person's own, also in an all-users install where Setup itself is elevated.
; "off" is written only when the file says "on" (the person cleared the task), so a plain install makes no file.
Filename: "{app}\{#AppExe}"; Parameters: "--set-run-as-admin on"; Flags: runhidden waituntilterminated runasoriginaluser; Tasks: runasadmin
Filename: "{app}\{#AppExe}"; Parameters: "--set-run-as-admin off"; Flags: runhidden waituntilterminated runasoriginaluser; Tasks: not runasadmin; Check: ShouldWriteRunAsAdminOff
; Not elevated, also after an install for all users: Lexia's data is the user's own (runasoriginaluser).
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchLexia}"; Flags: nowait postinstall skipifsilent runasoriginaluser

[Code]
// Closing a running Lexia (K.7): the same code for Setup and, with the "un" prefix, for the uninstaller.
#define P ""
#define Q "WizardSilent"
#define W "WhoSetup"
#include "close_lexia.isi"
#undef P
#undef Q
#undef W
#define P "un"
#define Q "UninstallSilent"
#define W "WhoUninstall"
#include "close_lexia.isi"

// Setup: Lexia must be closed before anything is copied. Asked at the start, so an update does not run
// through the pages first; PrepareToInstall asks again in case Lexia was started while the pages were open.
function InitializeSetup(): Boolean;
begin
  Result := CloseRunningLexia();
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  if CloseRunningLexia() then Result := '' else Result := CustomMessage('LexiaStillRunningSetup');
end;

// The uninstaller: the same, before any file is removed.
function InitializeUninstall(): Boolean;
begin
  Result := unCloseRunningLexia();
end;

// Does the settings file of the person who runs Setup say "runAsAdmin": true? A plain text check, not a JSON
// parse: Setup has no JSON reader, and Lexia's own file is written by Lexia ("runAsAdmin": true, one key per
// line, or a hand-edited one). An occurrence after // on its line is a comment and does not count. The file
// is %LOCALAPPDATA%\Lexia\settings.json (an old copy that sits only beside lexia.exe is not looked for).
function RunAsAdminInSettingsFile(): Boolean;
var
  raw: AnsiString;
  text, before, after, line: String;
  keyPos, i: Integer;
begin
  Result := False;
  if not LoadStringFromFile(ExpandConstant('{localappdata}\{#AppName}\settings.json'), raw) then Exit;
  text := String(raw);
  keyPos := Pos('"runAsAdmin"', text);
  while keyPos > 0 do
  begin
    before := Copy(text, 1, keyPos - 1);
    // The text of the key's own line before it.
    line := before;
    for i := Length(before) downto 1 do
      if before[i] = #10 then
      begin
        line := Copy(before, i + 1, Length(before));
        Break;
      end;
    after := Copy(text, keyPos + Length('"runAsAdmin"'), Length(text));
    if Pos('//', line) = 0 then
    begin
      after := TrimLeft(after);
      if (Length(after) > 0) and (after[1] = ':') then
      begin
        after := TrimLeft(Copy(after, 2, Length(after)));
        Result := Copy(after, 1, 4) = 'true';
        Exit;
      end;
    end;
    // A commented-out key, or a "runAsAdmin" that is a value: look further on.
    text := after;
    keyPos := Pos('"runAsAdmin"', text);
  end;
end;

var
  RunAsAdminTaskSet: Boolean;

// The first time the Tasks page is shown, "Run Lexia as administrator" gets the state of the setting.
// Later visits (Back, Next) keep what the person chose.
procedure CurPageChanged(CurPageID: Integer);
begin
  if (CurPageID = wpSelectTasks) and not RunAsAdminTaskSet then
  begin
    RunAsAdminTaskSet := True;
    if RunAsAdminInSettingsFile() then WizardSelectTasks('runasadmin') else WizardSelectTasks('!runasadmin');
  end;
end;

// The [Run] entry that writes "off": only after a page-based install in which the person cleared the task of
// a setting that was on. A silent run does not turn the setting off.
function ShouldWriteRunAsAdminOff(): Boolean;
begin
  Result := (not WizardSilent) and RunAsAdminInSettingsFile();
end;
// The uninstaller: the Run value goes with the program (Lexia may have written it without the task),
// but only when it starts this installed lexia.exe: a value of another Lexia (a portable copy, a
// developer build) is left alone. The user decides about their own data; silent uninstalls keep it.
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  dataDir, runValue: String;
begin
  if CurUninstallStep = usUninstall then
    if RegQueryStringValue(HKCU, 'Software\Microsoft\Windows\CurrentVersion\Run', '{#AppName}', runValue) then
      if Pos(Lowercase(ExpandConstant('{app}\{#AppExe}')), Lowercase(runValue)) > 0 then
        RegDeleteValue(HKCU, 'Software\Microsoft\Windows\CurrentVersion\Run', '{#AppName}');
  if CurUninstallStep = usPostUninstall then
  begin
    dataDir := ExpandConstant('{localappdata}\{#AppName}');
    if DirExists(dataDir) and not UninstallSilent then
      if MsgBox(FmtMessage(CustomMessage('RemoveUserData'), [dataDir]), mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES then
        DelTree(dataDir, True, True, True);
  end;
end;
