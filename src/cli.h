#pragma once
// The pure parts of lexia_cli: reading its arguments and writing its text and JSON output.
// Win32-free and OpenCV-free, so lexia_tests covers them; src/cli_main.cpp does the file and
// image handling around them.
#include <string>
#include <vector>

#include "common.h"
#include "score.h"
#include "tts/voice_catalogue.h"
#include "tts/voice_controls.h"
#include "tts/voice_library.h"

struct CliArgs {
    std::string imagePath;  // UTF-8
    std::string langCode;   // a Language code ("en", "bg"); empty = the default Language
    bool hasRect = false;   // --rect given: recognise it like a Drag, else the whole Shot like the Analysis
    Rect rect;              // from --rect x,y,w,h
    std::string jsonPath;   // --json; empty = not asked for
    std::string drawPath;   // --draw; empty = not asked for
    std::string scorePath;  // --score; an expected.txt to score the Blocks against; empty = not asked for
    std::string sayText;    // --say; UTF-8 text to speak and write as a WAV instead of reading an image; empty = not asked for
    std::string wavPath;    // --wav; the file --say writes
    VoiceControls voice;    // --speed, --pitch, --volume (with --say only); the defaults when not given
    bool listVoices = false;      // --voices-catalogue: list the Voices that can be downloaded
    std::string downloadVoice;    // --voice-download <name>; empty = not asked for
    std::string addVoicePath;     // --voice-add <path>: a .zip, .onnx or .onnx.json; empty = not asked for
    std::string voicesDir;        // --voices-dir: where the Voice library writes; empty = the user's Voice folder
    std::string stringsLang;      // --strings-json <code>: print the Interface language's strings as JSON; empty = not asked for
};

struct CliParse {
    bool ok = false;
    bool help = false;  // --help: ok, but the caller prints CliUsage() and stops
    bool version = false;  // --version: ok, but the caller prints CliVersionText() and stops
    CliArgs args;
    std::string error;  // when !ok: one line saying what is wrong
};

// `args` are the command-line arguments without the program name, as UTF-8.
CliParse ParseCliArgs(const std::vector<std::string>& args);

// The usage text, ending in a newline.
std::string CliUsage();

// "lexia_cli <Lexia's version>" and a newline (version.h).
std::string CliVersionText();

// The Blocks' text in the expected.txt format: one Block per paragraph, a blank line between,
// a newline at the end. Empty when there are no Blocks.
std::string BlocksToExpectedText(const std::vector<TextBlock>& blocks);

// The --json document: {"language", "ms", "blocks": [{"index", "box", "text", "lines": [...]}]}.
// Blocks are in Reading order; "index" counts from 1 and is the number --draw writes on the
// Block. Boxes are {left, top, right, bottom} in Shot pixels; a Line has box, text and conf (0..100).
std::string BlocksToJson(const std::vector<TextBlock>& blocks, const std::string& languageCode,
                         long long ms);

// The --voices-catalogue document: one line per Voice, "name<TAB>Language<TAB>size in MB", plus
// "<TAB>installed" for the names in `installed`.
std::string VoiceCatalogueToText(const std::vector<CatalogueVoice>& voices, const std::vector<std::string>& installed);

// What --voice-download and --voice-add print: "added <name> (<Language>)", or the problem id,
// the fact it is about (the language's name, the quality) and the detail. A line ending in a newline.
std::string VoiceOutcomeToText(const VoiceOutcome& outcome);

// The --strings-json document: one JSON object, every string id -> its text in the Interface
// language `code` (an unknown code gives English; a text the language lacks is the English one).
// A {name} placeholder stays as written. Keys are in the order of strings::AllIds(); ends in a newline.
// The Settings window's preview and the page's tests take their text from this.
std::string StringsToJson(const std::string& code);

// The --score document, one JSON object on one line:
// {"distance", "editDistance", "expectedBlocks", "producedBlocks", "blockCountError"}.
// See LayoutScore (score.h) for what each number means.
std::string ScoreToJson(const LayoutScore& score);
