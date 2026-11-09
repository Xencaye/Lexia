#pragma once
// The Voice library: which Piper Voices are installed, and adding one from files. Win32-free
// (std::filesystem only), so lexia_tests covers it. Downloading is in voice_download.h.
//
// Where Voices live. The Voices that ship are in `voices\` next to the exe, listed in its
// voices.json (voices.h); that folder may be read-only after an installer. Voices the user adds or
// downloads go to `%LOCALAPPDATA%\Lexia\voices` (UserVoicesDir), which needs no voices.json: each
// Voice there is `<name>.onnx` + `<name>.onnx.json`, and the Language it speaks is read from the
// config. Finding a Voice for a Language looks in both folders (ListInstalledVoices).
//
// Only Medium-quality Voices for a Language Lexia can read are accepted. A refusal is a
// VoiceOutcome with a VoiceProblem and the facts the message needs (the language's name, the
// quality). The words the user sees ("Lexia can't read German yet") are the Settings window's
// (D5) and the strings table's (I1), not this module's: `detail` is one English line for the log.
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

enum class VoiceProblem {
    None,
    UnsupportedLanguage,  // the Voice speaks a language Lexia cannot read yet; `languageName` says which ("German")
    NotMedium,            // quality is not Medium; `quality` says what it is ("low", "high"; empty = unknown)
    BadName,              // the Voice's name has characters other than letters, digits, '_', '-' and '.',
                          // or is a Windows device name (CON, NUL, COM1, ... before the first '.')
    MissingFile,          // the .onnx or the .onnx.json is missing or unreadable
    BadConfig,            // the .onnx.json is not a Piper Voice config (not JSON, or no language to find)
    BadZip,               // not a zip, an unsafe path in it, or not exactly the two files
    CannotWrite,          // the Voice's files could not be written into the Voice folder
    NotInCatalogue,       // download: the catalogue has no such Voice (for Lexia's Languages, Medium)
    Network,              // download: the network failed or the server said no
    BadDownload,          // download: wrong size or digest; nothing was kept
    Cancelled,            // download: the user cancelled; nothing was kept
    Busy,                 // a download or an add already runs; the request was not started (id "unknown": the page shows its general text)
};

// A stable id for a problem ("unsupported-language", ...), for the CLI and logs; not UI text.
const char* ProblemId(VoiceProblem problem);

struct VoiceOutcome {
    VoiceProblem problem = VoiceProblem::None;
    std::string voiceName;     // the Voice's name (its file name without ".onnx"); set when known
    std::string languageCode;  // Lexia's Language code ("en", "bg"); set when ok
    std::string languageName;  // UnsupportedLanguage: the language's English name, else its code ("German", "de_DE")
    std::string quality;       // NotMedium: the quality found, lower case
    std::string detail;        // one English line for the log
    bool ok() const { return problem == VoiceProblem::None; }
};

// Can Lexia use a Voice with these facts? `languageCode` is the Voice's own ("bg_BG", "en-us";
// the part before '_' or '-' is matched with the Languages' `code`), `languageName` its English
// name if known. Checks in this order: BadName, UnsupportedLanguage, NotMedium (so a low-quality
// German Voice is reported as German). ok(): voiceName and languageCode are set.
VoiceOutcome CheckVoiceAccepted(const std::string& name, const std::string& languageCode,
                                const std::string& languageName, const std::string& quality);

// The facts in a Piper config (".onnx.json"). The language is `language.code`, else the part of
// the Voice's name before the first '-' ("bg_BG" in "bg_BG-sample-medium": some configs, the
// Bulgarian one among them, have no "language" object), else `espeak.voice`. The quality is
// `audio.quality`, else the last '-' part of the name.
struct VoiceConfig {
    std::string languageCode;
    std::string languageName;  // `language.name_english`; may be empty
    std::string quality;       // lower case; may be empty
};
bool ParseVoiceConfig(const std::string& name, const std::string& configText, VoiceConfig* out);

// ParseVoiceConfig then CheckVoiceAccepted: BadConfig if the text is not a usable config.
VoiceOutcome CheckVoiceConfig(const std::string& name, const std::string& configText);

struct InstalledVoice {
    std::string name;
    std::string languageCode;  // Lexia's Language code
    std::filesystem::path model;  // the .onnx file
    bool shipped = false;      // from the exe's voices.json, else the user's folder
};

// Every installed Voice that can be used: first the entries of `voicesDir`\voices.json whose
// .onnx file is there (in voices.json's order), then each `<name>.onnx` with a valid, accepted
// `<name>.onnx.json` in `userDir` (in name order; empty `userDir` = none). A Voice in the user's
// folder with the name of a shipped one is left out. Never throws.
std::vector<InstalledVoice> ListInstalledVoices(const std::filesystem::path& voicesDir,
                                                const std::filesystem::path& userDir);

// Adds a Voice to the folder `destDir` (created if missing) from `model` (<name>.onnx) and
// `config` (<name>.onnx.json), which must have the same name. The config is checked first, the
// files are copied under temporary names and then put in place (InstallVoice), so a Voice is never
// half there. A Voice already there with this name is replaced, and kept if the new one cannot be put in.
VoiceOutcome AddVoiceFiles(const std::filesystem::path& model, const std::filesystem::path& config,
                           const std::filesystem::path& destDir);

// Renames `from` to `to`, replacing `to`. A seam so that tests can make a rename fail; the default
// is std::filesystem::rename.
using RenameFn = std::function<void(const std::filesystem::path& from, const std::filesystem::path& to,
                                    std::error_code& ec)>;

// Puts a Voice's two complete temporary files (modelPart, configPart, in destDir) in place as
// <name>.onnx and <name>.onnx.json. A Voice already there is moved aside (.old) first and
// comes back if anything fails, so it is replaced only when the new one is completely in place.
// ok() or CannotWrite. Shared by AddVoice* and DownloadVoice.
VoiceOutcome InstallVoice(const std::string& name, const std::filesystem::path& destDir,
                          const std::filesystem::path& modelPart, const std::filesystem::path& configPart,
                          const RenameFn& rename = nullptr);
// Adds a Voice from a zip holding exactly `<name>.onnx` and `<name>.onnx.json` (in one folder
// inside it is fine). Refused (BadZip): a path that climbs out (`..`), is absolute, has a drive or
// a stream, any other file, more than 1 GiB of unpacked data, an encrypted entry. Then as AddVoiceFiles.
VoiceOutcome AddVoiceZip(const std::filesystem::path& zip, const std::filesystem::path& destDir);

// Adds a Voice from one path: a `.zip`, a `.onnx` (its `.onnx.json` beside it) or a `.onnx.json`
// (its `.onnx` beside it).
VoiceOutcome AddVoice(const std::filesystem::path& path, const std::filesystem::path& destDir);

// Is `path` a safe relative path inside an archive or catalogue: no empty or `.`/`..` parts, no
// leading slash, no drive or stream colon, no control characters? '\' counts as a separator.
bool IsSafeRelativePath(const std::string& path);

// The Voice that speaks a Language: `preferredName` if it is installed for that Language
// (`languageCode` is Lexia's code, "en"), else the first installed one for it (the shipped Voice
// comes first in ListInstalledVoices). nullptr if the Language has none.
const InstalledVoice* PickVoice(const std::vector<InstalledVoice>& installed, const std::string& languageCode,
                                const std::string& preferredName);

// Deletes the user's Voice `name`: `<name>.onnx` and `<name>.onnx.json` in `userVoicesDir`, and
// nothing outside that folder. BadName for a name that is not a plain Voice name (path parts,
// odd characters, a device name; checked first); MissingFile when neither file is there;
// CannotWrite when a file could not be deleted (e.g. it is in use). A Voice with only one of its
// two files is removed too. `voiceName` of the result is `name`. The caller decides which Voices
// may be removed (shipped ones are not in the user's folder, and never reach here).
VoiceOutcome RemoveVoice(const std::filesystem::path& userVoicesDir, std::string_view name);
