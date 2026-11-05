; Lexia's installer (task H4), an Inno Setup 6 script. It installs the package folder that the build
; makes (build\package\Lexia: lexia.exe, the DLLs, models, Voices, ui, notices) and replaces Lexia.zip
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
; - Lexia's Settings window needs the Microsoft Edge WebView2 Runtime. Windows 11 has it; a Windows 10
;   PC may not. Then a task (on by default) runs Microsoft's Evergreen Bootstrapper, which this installer
;   carries (installer\redist, fetched by scripts\fetch_deps.ps1 -Installer): it downloads the runtime
;   from Microsoft and installs it, without admin rights too. The task says that it downloads, and the user
;   can turn it off (ADR-0005: the network only when the user asks); then the last page offers the
;   download page instead.
; - An update installs over the old Lexia; a running Lexia must be closed first (AppMutex).
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
; The WebView2 Runtime's id in EdgeUpdate's registry keys (Microsoft's documented way to detect it).
#define WebView2Id "{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}"
#define WebView2Setup "MicrosoftEdgeWebview2Setup.exe"
#if !FileExists(AddBackslash(SourcePath) + "redist\" + WebView2Setup)
  #error redist\MicrosoftEdgeWebview2Setup.exe is missing: run scripts\fetch_deps.ps1 -Installer
#endif

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
; support (Windows 10 1703+). 1809 is the oldest Windows 10 that Microsoft's WebView2 Runtime supports.
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
; A running Lexia holds this mutex (src/instance.cpp, Local\Lexia.SingleInstance): Setup and the
; uninstaller ask the user to quit it from the tray first, so no file is in use.
AppMutex=Lexia.SingleInstance
; The licence is GPL-3.0-or-later, which is not an agreement to accept: it is installed beside the
; program (LICENSE, THIRD-PARTY-NOTICES.txt), not shown as a page to agree to.

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
english.TaskStartup=Start Lexia when I sign in to Windows (in the tray)
english.TaskDesktop=Create a desktop shortcut
english.LaunchLexia=Start Lexia now
english.TaskWebView2=Install the Microsoft Edge WebView2 Runtime, which Lexia's window needs (downloaded from Microsoft, about 150 MB)
english.InstallingWebView2=Installing the Microsoft Edge WebView2 Runtime (downloading from Microsoft)...
english.OpenWebView2Page=Open the download page of the Microsoft Edge WebView2 Runtime
english.RemoveUserData=Also remove your Lexia settings, log, Voices and Presets?%n%n%1%n%nChoose No to keep them for a later install.

[Tasks]
Name: "startup"; Description: "{cm:TaskStartup}"; Flags: unchecked
Name: "desktopicon"; Description: "{cm:TaskDesktop}"; Flags: unchecked
; Only shown when the runtime is missing; on by default then.
Name: "webview2"; Description: "{cm:TaskWebView2}"; Check: not WebView2Installed

[Files]
; The whole package folder, as the build made it. ignoreversion: Lexia's files always replace the old ones.
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion
; The bootstrapper goes to Setup's temporary folder only, and only when the task is chosen.
Source: "redist\{#WebView2Setup}"; DestDir: "{tmp}"; Flags: deleteafterinstall; Tasks: webview2

[InstallDelete]
; An update starts from a clean program folder, so a file that a newer Lexia no longer ships (an old DLL,
; a removed page) is not left behind. The user's data is not in {app}.
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
; First the runtime (if chosen), so that Lexia's window can open when Lexia starts below. /silent /install
; is Microsoft's documented silent install; it waits until the runtime is in place.
Filename: "{tmp}\{#WebView2Setup}"; Parameters: "/silent /install"; StatusMsg: "{cm:InstallingWebView2}"; \
    Flags: waituntilterminated; Tasks: webview2
; Not elevated, also after an install for all users: Lexia's data is the user's own (runasoriginaluser).
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchLexia}"; Flags: nowait postinstall skipifsilent runasoriginaluser
; Still no runtime (the task was turned off, or the download failed): offer Microsoft's page.
Filename: "https://developer.microsoft.com/microsoft-edge/webview2/"; Description: "{cm:OpenWebView2Page}"; \
    Flags: shellexec postinstall skipifsilent runasoriginaluser unchecked; Check: not WebView2Installed

[Code]
// The WebView2 Runtime is installed when EdgeUpdate lists it with a version ("pv") other than 0.0.0.0,
// for the machine (32-bit registry view, where Microsoft keeps it) or for this user.
function WebView2Version(RootKey: Integer; SubKey: String): String;
begin
  if not RegQueryStringValue(RootKey, SubKey, 'pv', Result) then Result := '';
end;

function WebView2Installed: Boolean;
var
  v: String;
begin
  v := WebView2Version(HKLM32, 'SOFTWARE\Microsoft\EdgeUpdate\Clients\{#WebView2Id}');
  if (v = '') or (v = '0.0.0.0') then
    v := WebView2Version(HKCU, 'Software\Microsoft\EdgeUpdate\Clients\{#WebView2Id}');
  Result := (v <> '') and (v <> '0.0.0.0');
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
