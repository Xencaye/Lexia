// lexia_cli - runs Lexia's OCR and layout on a saved Shot, so a change to either can be checked
// without looking at the screen, and speaks text to a WAV file (--say). A developer tool: it is built next to lexia.exe but is not
// packaged. See cli.h for the output formats and CliUsage() for the command line.
//
// Exit codes: 0 done, 1 an unexpected failure, 2 bad arguments, 3 models missing,
// 4 image unreadable, 5 an output file could not be written, 6 the Voice is missing
// or failed, 7 the text gave no audio, 8 the Voice library refused the Voice (the problem id says why),
// 9 a download failed or was cancelled. Finding no text is not an error,
// and neither is a bad --score: it still exits 0 (the script reads the numbers); --score exits
// 2 only when the expected.txt cannot be read.
//
// ADR-0005 (recognised text never reaches the log) still holds: the text goes to stdout or to
// the files the user asked for, never through the logger.

#include <windows.h>

#include <fcntl.h>
#include <io.h>

#include <algorithm>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "cli.h"
#include "common.h"
#include "language.h"
#include "log.h"
#include "net/winhttp_fetch.h"
#include "ocr.h"
#include "recognize.h"
#include "tts/piper_voice.h"
#include "tts/voice_catalogue.h"
#include "tts/voice_download.h"
#include "tts/voice_library.h"
#include "tts/wav.h"

namespace fs = std::filesystem;

namespace {

// lexia_cli's exit codes, so a script can tell what went wrong.
enum ExitCode { kOk = 0, kFailed = 1, kBadArgs = 2, kNoModels = 3, kBadImage = 4, kBadOutput = 5, kNoVoice = 6, kNoSpeech = 7, kVoiceRefused = 8, kNetwork = 9 };

// cv::imread / imwrite take narrow paths in the ANSI code page, which breaks on Cyrillic file
// names, so the bytes go through std::filesystem and imdecode / imencode.
cv::Mat ReadImage(const fs::path& path) {
    std::string bytes;
    if (!ReadFileToString(path, &bytes) || bytes.empty()) return {};
    return cv::imdecode(std::vector<uchar>(bytes.begin(), bytes.end()), cv::IMREAD_COLOR);
}

// Writes the bytes to a new file (any file there is replaced).
bool WriteBytes(const fs::path& path, const char* data, size_t size) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(data, static_cast<std::streamsize>(size));
    return static_cast<bool>(out);
}

// Writes `img` as a PNG, through WriteBytes for the same reason as ReadImage.
bool WriteImage(const fs::path& path, const cv::Mat& img) {
    std::vector<uchar> png;
    if (!cv::imencode(".png", img, png)) return false;
    return WriteBytes(path, reinterpret_cast<const char*>(png.data()), png.size());
}

// stdout carries UTF-8 bytes untouched: binary mode, no code page conversion.
void WriteStdout(const std::string& utf8) {
    _setmode(_fileno(stdout), _O_BINARY);
    std::fwrite(utf8.data(), 1, utf8.size(), stdout);
    std::fflush(stdout);
}

// The usage text goes to stderr after a bad-argument error, so stdout stays clean for output.
void WriteUsageToStderr() {
    const std::string usage = CliUsage();
    std::fwrite(usage.data(), 1, usage.size(), stderr);
}

// The Shot with every Block outlined and numbered in Reading order (the same numbers as
// "index" in the JSON), and its Lines as thin outlines so a split or merged Line is visible.
cv::Mat DrawBlocks(const cv::Mat& shot, const std::vector<TextBlock>& blocks) {
    cv::Mat out = shot.clone();
    static const cv::Scalar kColours[] = {  // BGR; dark enough to read white text on
        {200, 60, 30}, {40, 140, 30}, {30, 30, 200}, {160, 40, 160}, {20, 140, 200}, {120, 120, 20}};
    const size_t nColours = sizeof(kColours) / sizeof(kColours[0]);
    const double scale = std::max(0.6, out.rows / 1500.0);
    const int thick = std::max(2, out.rows / 540);
    for (size_t i = 0; i < blocks.size(); ++i) {
        const TextBlock& b = blocks[i];
        const cv::Scalar colour = kColours[i % nColours];
        for (const TextLine& l : b.lines) {
            cv::rectangle(out, {l.box.left, l.box.top}, {l.box.right - 1, l.box.bottom - 1}, colour, 1);
        }
        cv::rectangle(out, {b.box.left, b.box.top}, {b.box.right - 1, b.box.bottom - 1}, colour, thick);

        const std::string label = std::to_string(i + 1);
        int baseline = 0;
        const cv::Size ts = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, scale, thick, &baseline);
        const int pad = 4;
        const int w = ts.width + 2 * pad, h = ts.height + baseline + 2 * pad;
        // Above the Block's top-left corner if there is room, else inside it.
        const int x = b.box.left;
        const int y = b.box.top - h >= 0 ? b.box.top - h : b.box.top;
        cv::rectangle(out, {x, y}, {x + w, y + h}, colour, cv::FILLED);
        cv::putText(out, label, {x + pad, y + pad + ts.height}, cv::FONT_HERSHEY_SIMPLEX, scale,
                    {255, 255, 255}, thick, cv::LINE_AA);
    }
    return out;
}

// Models: where the app looks (beside the exe), then the dev models/ folder. The dev path is
// compiled into this tool only, never into lexia_core or lexia.exe.
bool FindModels(const Language& lang, std::string* triedOut) {
    *triedOut = OcrEngine::ModelsDir();
    if (OcrEngine::ModelsPresent(lang)) return true;
#ifdef LEXIA_DEV_MODELS_DIR
    OcrEngine::SetModelsDir(LEXIA_DEV_MODELS_DIR);
    *triedOut += " and " + OcrEngine::ModelsDir();
    if (OcrEngine::ModelsPresent(lang)) return true;
#endif
    return false;
}

// The Voice's files: beside the exe like the app, else the dev folders. The dev paths are
// compiled into this tool only, never into lexia_core or lexia.exe.
VoiceFolders FindVoiceFolders() {
    VoiceFolders f = DefaultVoiceFolders();
#ifdef LEXIA_DEV_VOICES_DIR
    if (!fs::exists(f.voices / "voices.json")) f.voices = fs::u8path(LEXIA_DEV_VOICES_DIR);
#endif
#ifdef LEXIA_DEV_ESPEAK_DATA_DIR
    if (!fs::is_directory(f.espeakData)) f.espeakData = fs::u8path(LEXIA_DEV_ESPEAK_DATA_DIR);
#endif
    return f;
}

// --say: speaks the text with the Language's Voice into a WAV file.
int Say(const CliArgs& args) {
    const std::string code = args.langCode.empty() ? DefaultLanguage().code : args.langCode;
    const VoiceFolders folders = FindVoiceFolders();
    std::string error;
    const auto voice = LoadVoiceForLanguage(code, folders, &error);
    if (!voice) {
        LogError("No Voice for ", code, ": ", error, ". Run scripts/fetch_deps.ps1 (voices go in ",
                 folders.voices.u8string(), ").");
        return kNoVoice;
    }
    std::vector<float> samples;
    int rate = 0;
    const SynthResult r = voice->Synthesize(args.sayText, args.voice, [&](const SpeechChunk& c) {
        rate = c.sampleRate;
        samples.insert(samples.end(), c.samples.begin(), c.samples.end());
        return true;
    });
    if (r != SynthResult::Done) {
        LogError("The Voice failed to speak the text.");
        return kNoVoice;
    }
    if (samples.empty()) {
        LogError("The text gave no speech (nothing to say in it?); no WAV was written.");
        return kNoSpeech;
    }
    const std::string wav = EncodeWav16(samples, rate);
    if (!WriteBytes(fs::u8path(args.wavPath), wav.data(), wav.size())) {
        LogError("Could not write: ", args.wavPath);
        return kBadOutput;
    }
    return kOk;
}

// ---------------------------------------------------------------- Voice library ---
// The only network use in Lexia (ADR-0005): it happens here only because the user ran one of these.

fs::path VoiceLibraryDir(const CliArgs& args) {
    return args.voicesDir.empty() ? UserVoicesDir() : fs::u8path(args.voicesDir);
}

// A network or download problem is kNetwork; a refused Voice is kVoiceRefused.
int VoiceExitCode(const VoiceOutcome& o) {
    if (o.ok()) return kOk;
    return o.problem == VoiceProblem::Network || o.problem == VoiceProblem::BadDownload || o.problem == VoiceProblem::Cancelled
               ? kNetwork
               : kVoiceRefused;
}

// Prints the outcome (stdout on success, the log's error line otherwise) and returns its exit code.
int ReportVoiceOutcome(const VoiceOutcome& o) {
    const std::string text = VoiceOutcomeToText(o);
    if (o.ok()) {
        WriteStdout(text);
    } else {
        LogError(Trim(text));
    }
    return VoiceExitCode(o);
}

// Where the catalogue is fetched to, briefly (it is deleted again).
fs::path CatalogueTempDir() { return fs::temp_directory_path() / "LexiaVoices"; }

// --voices-catalogue: the downloadable Voices, marked "installed" when the user has them.
int ListVoices(const CliArgs& args) {
    const CatalogueResult cat = FetchVoiceCatalogue(WinHttpFetch, CatalogueTempDir());
    if (!cat.error.ok()) return ReportVoiceOutcome(cat.error);
    std::vector<std::string> installed;
    for (const InstalledVoice& v : ListInstalledVoices(FindVoiceFolders().voices, VoiceLibraryDir(args))) {
        installed.push_back(v.name);
    }
    WriteStdout(VoiceCatalogueToText(cat.voices, installed));
    return kOk;
}

// --voice-download <name>: the Voice must be in the filtered catalogue. The progress goes to stderr as
// one line that is rewritten.
int DownloadVoiceCommand(const CliArgs& args) {
    const CatalogueResult cat = FetchVoiceCatalogue(WinHttpFetch, CatalogueTempDir());
    if (!cat.error.ok()) return ReportVoiceOutcome(cat.error);
    const auto it = std::find_if(cat.voices.begin(), cat.voices.end(),
                                 [&](const CatalogueVoice& v) { return v.name == args.downloadVoice; });
    if (it == cat.voices.end()) {
        VoiceOutcome o;
        o.problem = VoiceProblem::NotInCatalogue;
        o.voiceName = args.downloadVoice;
        o.detail = "'" + args.downloadVoice + "' is not a Medium Voice for one of Lexia's Languages in the catalogue; try --voices-catalogue";
        return ReportVoiceOutcome(o);
    }
    int lastPercent = -1;
    const VoiceOutcome o = DownloadVoice(*it, WinHttpFetch, [&](uint64_t done, uint64_t total) {
        const int percent = total == 0 ? 0 : static_cast<int>(done * 100 / total);
        if (percent != lastPercent) {
            lastPercent = percent;
            std::fprintf(stderr, "\r%s: %d%%", it->name.c_str(), percent);
        }
        return true;
    }, VoiceLibraryDir(args));
    std::fprintf(stderr, "\n");
    return ReportVoiceOutcome(o);
}

// --voice-add <file>.
int AddVoiceCommand(const CliArgs& args) {
    return ReportVoiceOutcome(AddVoice(fs::u8path(args.addVoicePath), VoiceLibraryDir(args)));
}

// Runs the one command the arguments name (ParseCliArgs made sure there is exactly one).
int Run(const CliArgs& args) {
    if (!args.stringsLang.empty()) {
        WriteStdout(StringsToJson(args.stringsLang));
        return kOk;
    }
    if (args.listVoices) return ListVoices(args);
    if (!args.downloadVoice.empty()) return DownloadVoiceCommand(args);
    if (!args.addVoicePath.empty()) return AddVoiceCommand(args);
    if (!args.sayText.empty()) return Say(args);
    const Language& lang = args.langCode.empty() ? DefaultLanguage() : *FindLanguageByCode(args.langCode);

    std::string tried;
    if (!FindModels(lang, &tried)) {
        LogError("The OCR models for ", lang.name, " were not found in: ", tried,
                 ". Run scripts\\fetch_deps.ps1, or put a models folder next to lexia_cli.exe.");
        return kNoModels;
    }

    const fs::path imagePath = fs::u8path(args.imagePath);
    const cv::Mat shot = ReadImage(imagePath);
    if (shot.empty()) {
        LogError("Could not read the image (missing, or not a valid PNG/JPEG/BMP): ", args.imagePath);
        return kBadImage;
    }

    // The area: the whole Shot (the Analysis), or --rect cut to the Shot (a Drag).
    Rect area = {0, 0, shot.cols, shot.rows};
    if (args.hasRect) {
        area.left = args.rect.left;
        area.top = args.rect.top;
        area.right = std::min(args.rect.right, shot.cols);
        area.bottom = std::min(args.rect.bottom, shot.rows);
        if (area.left >= area.right || area.top >= area.bottom) {
            LogError("--rect is outside the image (", shot.cols, "x", shot.rows, ").");
            WriteUsageToStderr();
            return kBadArgs;
        }
    }

    // The expected.txt is read before the OCR runs, so a wrong path fails fast.
    std::string expectedText;
    if (!args.scorePath.empty() && !ReadFileToString(fs::u8path(args.scorePath), &expectedText)) {
        LogError("Could not read the expected text: ", args.scorePath);
        return kBadArgs;
    }

    OcrEngine ocr;
    if (!ocr.Init(lang)) {
        LogError("Could not load the OCR models for ", lang.name, " (details above).");
        return kNoModels;
    }

    const Recognition r = RecognizeBlocks(ocr, shot, area, args.hasRect);

    if (!args.jsonPath.empty()) {
        const std::string json = BlocksToJson(r.blocks, lang.code, r.ms);
        if (!WriteBytes(fs::u8path(args.jsonPath), json.data(), json.size())) {
            LogError("Could not write: ", args.jsonPath);
            return kBadOutput;
        }
    }
    if (!args.drawPath.empty()) {
        if (!WriteImage(fs::u8path(args.drawPath), DrawBlocks(shot, r.blocks))) {
            LogError("Could not write: ", args.drawPath);
            return kBadOutput;
        }
    }
    if (!args.scorePath.empty()) {
        WriteStdout(ScoreToJson(ScoreLayout(ParseExpectedBlocks(expectedText), BlockTexts(r.blocks))));
    }
    if (args.jsonPath.empty() && args.drawPath.empty() && args.scorePath.empty()) {
        WriteStdout(BlocksToExpectedText(r.blocks));
    }
    return kOk;
}

}  // namespace

// lexia_cli starts here: wide arguments (any file name), UTF-8 console output, then Run.
int wmain(int argc, wchar_t** argv) {
    // First: DLLs loaded from now on come only from the exe's folder and System32, never from
    // the current folder or PATH, where a planted DLL could be picked up.
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    SetConsoleOutputCP(CP_UTF8);
    // Warnings and errors go to stderr, Info to stdout. Keeping Info off the console means
    // stdout holds only the recognised text.
    SetLogLevel(LogLevel::Warn);

    std::vector<std::string> raw;
    for (int i = 1; i < argc; ++i) raw.push_back(WideToUtf8(argv[i]));

    const CliParse parsed = ParseCliArgs(raw);
    if (parsed.help) {
        WriteStdout(CliUsage());
        return kOk;
    }
    if (parsed.version) {
        WriteStdout(CliVersionText());
        return kOk;
    }
    if (!parsed.ok) {
        LogError("lexia_cli: ", parsed.error);
        WriteUsageToStderr();
        return kBadArgs;
    }
    try {
        return Run(parsed.args);
    } catch (const std::exception& e) {
        LogError("lexia_cli failed: ", e.what());
        return kFailed;
    }
}
