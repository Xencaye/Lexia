#pragma once
// Every id of Lexia's own user-facing text, in one list. Code asks for a text with the constant
// (strings::id::kTrayQuit), so a typo is a compile error; the tests check that every id here has
// an English text and that no table has an id that is not here.
//
// To add a text: one line here, one in table_en.cpp (required), one in each other table (without
// one, that Interface language shows the English text). Ids are stable: the name of a text never
// changes once it ships, only its wording.
//
// {name} in a text is a placeholder, filled by strings::Format. The ones used:
//   {key}        a key as shown to the user ("F6")
//   {language}   a Language's name in the Interface language (strings::LanguageName)
#include <vector>

// X(ConstantName, "id")
#define LEXIA_STRING_IDS(X)                                                                      \
    /* An action that has no key (it clashed in settings.json) */                                \
    X(KeyNone, "key.none")                                                                       \
    /* Announcements (B3). A Language switch says Language::ownName in its own Voice and needs  \
       no text. */                                                                               \
    X(AnnounceNoOtherLanguage, "announce.noOtherLanguage")                                       \
    X(AnnouncePickNoText, "announce.pickNoText")                                                 \
    X(AnnounceCaptureFailed, "announce.captureFailed")                                           \
    X(AnnounceVoiceMissingFor, "announce.voiceMissingFor")  /* {language} */                     \
    /* Control bar (D4): the label on a button, and its tooltip with the key. For an action      \
       without a key show the label as the tooltip. */                                           \
    X(ControlBarPause, "controlBar.pause")                                                       \
    X(ControlBarPauseTip, "controlBar.pauseTip")  /* {key} */                                    \
    X(ControlBarResume, "controlBar.resume")                                                     \
    X(ControlBarResumeTip, "controlBar.resumeTip")  /* {key} */                                  \
    X(ControlBarSkip, "controlBar.skip")                                                         \
    X(ControlBarSkipTip, "controlBar.skipTip")  /* {key} */                                      \
    X(ControlBarStop, "controlBar.stop")                                                         \
    X(ControlBarStopTip, "controlBar.stopTip")  /* {key} */                                      \
    /* Control bar (D8.4): Help (opens the Settings window on its Help page), the size switch,   \
       the bar's accessible name */                                                              \
    X(ControlBarHelpTip, "controlBar.helpTip")                                                   \
    X(ControlBarSizeSmall, "controlBar.sizeSmall")                                               \
    X(ControlBarSizeLarge, "controlBar.sizeLarge")                                               \
    X(ControlBarName, "controlBar.name")                                                         \
    /* Tray menu (D2) */                                                                         \
    X(TrayOn, "tray.on")                                                                         \
    X(TrayOff, "tray.off")                                                                       \
    X(TrayOpen, "tray.open")                                                                     \
    X(TrayQuit, "tray.quit")                                                                     \
    X(TrayTooltipOn, "tray.tooltipOn")  /* the tray icon's tooltip while Lexia is On */          \
    X(TrayTooltipOff, "tray.tooltipOff")                                                         \
    /* Lexia could not start (a message box: the normal build has no console) */                 \
    X(AppStartFailed, "app.startFailed")                                                         \
    /* Settings window (D0 ui, D3, D8): the frame, Home, Voices, Settings, Report, Help, About */\
    X(WindowTitle, "window.title")  /* the title of the Settings window */                       \
    X(WindowNavMain, "window.nav.main")  /* accessible name of the top links */                  \
    X(WindowNavMore, "window.nav.more")  /* accessible name of the bottom links */               \
    X(WindowNavHome, "window.nav.home")                                                          \
    X(WindowNavVoices, "window.nav.voices")                                                      \
    X(WindowNavSettings, "window.nav.settings")                                                  \
    X(WindowNavReport, "window.nav.report")                                                      \
    X(WindowNavHelp, "window.nav.help")                                                          \
    X(WindowNavAbout, "window.nav.about")                                                        \
    X(WindowVolume, "window.volume")  /* accessible name of the Master volume slider */          \
    X(UiaSwitch, "uia.switch")  /* what a screen reader calls a switch (the localized control type) */ \
    X(WindowVolumeValue, "window.volumeValue")  /* {value}: percent */                           \
    X(WindowHomePower, "window.home.power")  /* the power button's accessible name */            \
    X(WindowHomeEnable, "window.home.enable")  /* the button under the power button while Lexia is Off */\
    X(WindowHomeDisable, "window.home.disable")  /* the same button while Lexia is On */         \
    /* Voices page (D0 ui 4, D8). Pitch is in semitones. */                                      \
    X(WindowVoiceVoice, "window.voice.voice")                                                    \
    X(WindowVoiceNone, "window.voice.none")  /* {language} */                                    \
    X(WindowVoiceDownload, "window.voice.download")  /* the icon button that opens the Voice download sheet */\
    X(WindowVoiceAddFromFile, "window.voice.addFromFile")  /* the icon button that adds a Voice from a file */\
    X(WindowVoiceDelete, "window.voice.delete")  /* the icon button that deletes the chosen Voice */\
    X(WindowVoiceDeleteShipped, "window.voice.deleteShipped")  /* its tooltip when the Voice comes with Lexia */\
    X(WindowVoiceControls, "window.voice.controls")                                              \
    X(WindowVoiceSpeed, "window.voice.speed")                                                    \
    X(WindowVoicePitch, "window.voice.pitch")                                                    \
    X(WindowVoiceVolume, "window.voice.volume")                                                  \
    X(WindowVoiceSpeedValue, "window.voice.speedValue")  /* {value} */                           \
    X(WindowVoicePitchValue, "window.voice.pitchValue")  /* {value}, several semitones */        \
    X(WindowVoicePitchValueOne, "window.voice.pitchValueOne")  /* {value}, one semitone */       \
    X(WindowVoicePercentValue, "window.voice.percentValue")  /* {value} */                       \
    X(WindowVoiceResetFor, "window.voice.resetFor")  /* {control}: the control's name */         \
    X(WindowVoiceSavePreset, "window.voice.savePreset")                                          \
    X(WindowVoiceLoadPreset, "window.voice.loadPreset")                                          \
    X(WindowDeleteVoiceTitle, "window.deleteVoice.title")  /* {voice} */                         \
    X(WindowDeleteVoiceText, "window.deleteVoice.text")                                          \
    X(WindowDeleteVoiceConfirm, "window.deleteVoice.confirm")                                    \
    X(WindowDeleteVoiceCancel, "window.deleteVoice.cancel")                                      \
    X(WindowDeleteVoiceFailedTitle, "window.deleteVoice.failedTitle")                        \
    X(WindowDeleteVoiceFailed, "window.deleteVoice.failed")  /* {voice} */                   \
    X(WindowDeleteVoiceFailedShipped, "window.deleteVoice.failedShipped")  /* {voice} */     \
    X(WindowPresetSavedTitle, "window.preset.savedTitle")                                    \
    X(WindowPresetSaved, "window.preset.saved")  /* {name}: the file name */                 \
    X(WindowPresetLoadedTitle, "window.preset.loadedTitle")                                  \
    X(WindowPresetLoaded, "window.preset.loaded")  /* {name}: the file name */               \
    X(WindowPresetFailedTitle, "window.preset.failedTitle")                                  \
    X(WindowPresetBadFile, "window.preset.badFile")  /* presetFailed badFile */              \
    X(WindowPresetCannotWrite, "window.preset.cannotWrite")  /* presetFailed cannotWrite */  \
    X(WindowPresetNoVoice, "window.preset.noVoice")  /* presetFailed noVoice */              \
    /* Settings page (D0 ui 2 and 3, D8) */                                                      \
    X(WindowKeysTitle, "window.keys.title")                                                      \
    X(WindowKeysReadRow, "window.keys.readRow")  /* the short name of a Key in the list */       \
    X(WindowKeysPauseRow, "window.keys.pauseRow")                                                \
    X(WindowKeysSpeechRow, "window.keys.speechRow")                                              \
    X(WindowKeysRead, "window.keys.read")  /* the full name of a Key: the title of its card */   \
    X(WindowKeysPause, "window.keys.pause")                                                      \
    X(WindowKeysSpeech, "window.keys.speech")                                                    \
    X(WindowKeysReadDesc, "window.keys.readDesc")  /* what a Key does; one paragraph per line */ \
    X(WindowKeysPauseDesc, "window.keys.pauseDesc")                                              \
    X(WindowKeysSpeechDesc, "window.keys.speechDesc")                                            \
    X(WindowKeysReadAloud, "window.keys.readAloud")  /* the key card's play button: {action} */  \
    X(WindowKeysChipLabel, "window.keys.chipLabel")  /* accessible name of the key chip: {action}, {key} */\
    X(WindowKeysCapturing, "window.keys.capturing")  /* the key chip while it waits for a key */ \
    X(WindowKeysCaptureHint, "window.keys.captureHint")                                          \
    X(WindowSettingsMisc, "window.settings.misc")                                                \
    X(WindowSettingsRunAtStartup, "window.settings.runAtStartup")                                \
    X(WindowSettingsStartMinimised, "window.settings.startMinimised")                            \
    X(WindowSettingsRunAsAdmin, "window.settings.runAsAdmin")                                    \
    X(WindowSettingsDarkMode, "window.settings.darkMode")                                        \
    X(WindowSettingsLeftHanded, "window.settings.leftHanded")                                    \
    /* Report, Help and About pages (D8) */                                                      \
    X(WindowReportTitle, "window.report.title")  /* one line per line */                         \
    X(WindowReportText, "window.report.text")                                                    \
    X(WindowReportMail, "window.report.mail")                                                    \
    X(WindowReportDiscord, "window.report.discord")                                              \
    X(WindowReportGithub, "window.report.github")                                                \
    X(WindowHelpTitle, "window.help.title")                                                      \
    X(WindowHelpSoon, "window.help.soon")                                                        \
    X(WindowAboutName, "window.about.name")                                                      \
    X(WindowAboutVersion, "window.about.version")  /* {version} */                               \
    X(WindowAboutIntro, "window.about.intro")  /* {games}, {comics}: the two words in bold */    \
    X(WindowAboutGames, "window.about.games")                                                    \
    X(WindowAboutComics, "window.about.comics")                                                  \
    X(WindowAboutHope, "window.about.hope")                                                      \
    X(WindowAboutLocal, "window.about.local")                                                    \
    X(WindowAboutHuman, "window.about.human")                                                    \
    X(WindowAboutThanks, "window.about.thanks")                                                  \
    X(WindowAboutItch, "window.about.itch")  /* accessible name of the itch.io link */           \
    X(WindowAboutGithub, "window.about.github")  /* accessible name of the GitHub link */        \
    X(WindowAboutLicence, "window.about.licence")                                                \
    X(WindowAboutNotices, "window.about.notices")  /* the link word in noticesLine ("here") */      \
    X(WindowAboutNoticesLine, "window.about.noticesLine")  /* {link}: the notices link goes there */ \
    X(WindowAboutNoticesLabel, "window.about.noticesLabel")  /* accessible name of that link */     \
    /* Voice download and Add from file sheets (D0 ui 5 and 6). window.problem.* is one text per   \
       VoiceProblem (voice_library.h); the host sends the code and the facts, the page picks. */ \
    X(WindowDownloadTitle, "window.download.title")                                                         \
    X(WindowDownloadNetwork, "window.download.network")  /* the line at the top of the sheet (ADR-0005) */  \
    X(WindowDownloadLoading, "window.download.loading")                                                     \
    X(WindowDownloadEmpty, "window.download.empty")                                                         \
    X(WindowDownloadSize, "window.download.size")  /* {mb}: megabytes, a number */                          \
    X(WindowDownloadGet, "window.download.get")                                                             \
    X(WindowDownloadGetFor, "window.download.getFor")  /* {voice}: the Voice's name; accessible name */     \
    X(WindowDownloadInstalled, "window.download.installed")                                                 \
    X(WindowDownloadProgress, "window.download.progress")  /* {done}, {total}: numbers */                   \
    X(WindowDownloadProgressLabel, "window.download.progressLabel")                                         \
    X(WindowDownloadCancel, "window.download.cancel")                                                       \
    X(WindowDownloadCancelFor, "window.download.cancelFor")                                                 \
    X(WindowDownloadCancelling, "window.download.cancelling")                                               \
    X(WindowDownloadDone, "window.download.done")                                                           \
    X(WindowDownloadCancelled, "window.download.cancelled")                                                 \
    X(WindowDownloadRetry, "window.download.retry")                                                         \
    X(WindowDownloadRetryFor, "window.download.retryFor")                                                   \
    X(WindowDownloadClose, "window.download.close")                                                         \
    X(WindowResultDone, "window.result.done")  /* lead word of a success message, with a check mark */      \
    X(WindowResultProblem, "window.result.problem")  /* lead word of an error message, with a warning mark */\
    X(WindowAddFileAddedTitle, "window.addFile.addedTitle")                                                 \
    X(WindowAddFileAdded, "window.addFile.added")  /* {language}: the Language's name */                    \
    X(WindowAddFileRefusedTitle, "window.addFile.refusedTitle")                                             \
    X(WindowAddFileAgain, "window.addFile.again")                                                           \
    X(WindowAddFileClose, "window.addFile.close")                                                           \
    X(WindowAddFileDialogTitle, "window.addFile.dialogTitle")  /* title of the file dialog (host) */        \
    X(WindowAddFileFilter, "window.addFile.filter")  /* the file dialog filter name (host) */               \
    X(WindowPresetSaveDialogTitle, "window.preset.saveDialogTitle")  /* title of the save dialog (host) */  \
    X(WindowPresetLoadDialogTitle, "window.preset.loadDialogTitle")  /* title of the open dialog (host) */  \
    X(WindowPresetFilter, "window.preset.filter")  /* the Preset file dialogs' filter name (host) */        \
    X(WindowProblemUnsupportedLanguage, "window.problem.unsupportedLanguage")  /* UnsupportedLanguage; {language}: English name of the voice's language */\
    X(WindowProblemThisLanguage, "window.problem.thisLanguage")  /* {language} when no name came */         \
    X(WindowProblemNotMedium, "window.problem.notMedium")  /* NotMedium; {quality}: lower case, e.g. low */ \
    X(WindowProblemNotMediumUnknown, "window.problem.notMediumUnknown")  /* NotMedium, quality not known */ \
    X(WindowProblemBadName, "window.problem.badName")  /* BadName, for a file */                            \
    X(WindowProblemBadNameDownload, "window.problem.badNameDownload")  /* BadName, for a download */        \
    X(WindowProblemMissingFile, "window.problem.missingFile")  /* MissingFile */                            \
    X(WindowProblemBadConfig, "window.problem.badConfig")  /* BadConfig */                                  \
    X(WindowProblemBadZip, "window.problem.badZip")  /* BadZip */                                           \
    X(WindowProblemCannotWrite, "window.problem.cannotWrite")  /* CannotWrite */                            \
    X(WindowProblemNotInCatalogue, "window.problem.notInCatalogue")  /* NotInCatalogue */                   \
    X(WindowProblemNetwork, "window.problem.network")  /* Network, for a download */                        \
    X(WindowProblemCatalogueNetwork, "window.problem.catalogueNetwork")  /* Network, for the list of voices */\
    X(WindowProblemBadDownload, "window.problem.badDownload")  /* BadDownload */                            \
    X(WindowProblemUnknown, "window.problem.unknown")  /* a problem code the page does not know */          \
    /* A Language's name in a sentence, one per Language id (language.h); lower case where the   \
       language writes it that way. */                                                           \
    X(LanguageEng, "language.eng")                                                               \
    X(LanguageBul, "language.bul")

namespace strings {
namespace id {
#define LEXIA_STRING_ID_CONSTANT(name, text) inline constexpr const char* k##name = text;
LEXIA_STRING_IDS(LEXIA_STRING_ID_CONSTANT)
#undef LEXIA_STRING_ID_CONSTANT
}  // namespace id

// All the ids above, in order.
const std::vector<const char*>& AllIds();
}  // namespace strings
