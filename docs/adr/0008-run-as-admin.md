# Run as administrator, as a setting

Decided by the owner on 2026-11-03 (Lexia 0.5, D8).

## Why

Some games run elevated (as administrator). Windows keeps a program that is not elevated from
sending input to, or watching, a window of an elevated one (UIPI, User Interface Privilege
Isolation). Over such a game a non-elevated Lexia gets no hotkeys, and its Overlay cannot be
clicked, so reading does not work. The owner wants Lexia to work over those games, and does not
want every person to run as administrator all the time, as that is a risk for little gain.

## Decision

- A setting, `runAsAdmin` (settings.json, a switch on the Settings page), off by default.
- At start, if `runAsAdmin` is on and Lexia is not elevated (`IsElevated`, `TokenElevation`),
  Lexia starts itself again with `ShellExecuteExW` and the verb `runas` (`RelaunchElevated`),
  passing `--elevated-relaunch` (and `--minimised` when it was started minimised:
  `RelaunchArgs`), and then ends. Turning the setting on in the Settings window relaunches in
  the same way.
- The relaunched instance has to take the single-instance mutex (`Local\Lexia.SingleInstance`)
  from the instance that is ending. With `--elevated-relaunch` it waits for the mutex for up to
  5 s instead of handing the "open Settings" message to the first instance and exiting.
  At start the first instance lets go of the mutex before it starts the elevated one (and takes it again if
  the prompt is cancelled). A second launch, or Explorer, that is not elevated cannot post its registered
  messages (`Lexia.OpenSettings`, `TaskbarCreated`) to an elevated Lexia unless the window lets them through
  (UIPI), so the App window does that with `ChangeWindowMessageFilterEx`.
- If the person cancels the UAC prompt (`RelaunchResult::Cancelled`), Lexia goes on without
  elevation. The state message carries `runAsAdmin` (the stored setting) and `elevated` (the
  fact), so the Settings page can show that the setting is on but not in effect.
- If `RelaunchElevated` fails for another reason (`Failed`), the cause is logged and Lexia goes
  on unelevated as well. It never fails to start because it could not elevate.
- Start at logon (Run key, D6) with `runAsAdmin` on shows a UAC prompt at every logon, as the
  Run key cannot start an elevated program silently. Accepted for 0.5.

## Consequences

- An elevated Lexia can read the windows of elevated games. It also runs the Settings
  window, the downloads and the Voice files with administrator rights. ADR-0005 (network only on
  a request) and ADR-0009 (a native window, its assets are resources in the exe) therefore
  matter more, and nothing in them changes. Lexia does not write outside `%LOCALAPPDATA%\Lexia`.
- Drag and drop and the file dialogs of a non-elevated Explorer may not reach an elevated Lexia
  (UIPI again). The Voice tab's file dialogs are Lexia's own (`IFileOpenDialog`), so they work.
- An elevated Lexia also hears `Lexia.Quit` from the installer (K.7): the App window lets this registered
  message through UIPI (`ChangeWindowMessageFilterEx`), as it does `Lexia.OpenSettings`, so Setup and the
  uninstaller (not elevated) can close it cleanly. Security: any process of the same user can already end a
  non-elevated Lexia, and the message carries no data and does one thing, `App::Quit()`; it opens no other door
  into the elevated process. The switch `--set-run-as-admin on|off` only saves that one boolean in the settings
  file of the user who runs it, and never elevates.
- The Run key value is unchanged (no switch). Start minimised decides after the relaunch too.
- Later option: a Task Scheduler task with "highest privileges", started at logon, which gives an
  elevated start without a UAC prompt. It needs the installer or an admin step once, so it waits
  for an installer.
- Not needed: a manifest with `requireAdministrator`. It would ask for elevation of every start
  and of every person, and the setting would mean nothing.
