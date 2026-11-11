#include "cli.h"

#include <algorithm>
#include <charconv>
#include <climits>
#include <cmath>
#include <sstream>

#include <nlohmann/json.hpp>  // ordered_json: keys stay in the order written, which reads better

#include "language.h"
#include "version.h"

// lexia_cli's pure parts: the argument parser, the usage text and the output formats (the expected.txt
// text, the Blocks as JSON, the Fixture score, the Voice catalogue). They are in
// lexia_core so the tests can call them; the entry point that does the I/O is cli_main.cpp.
namespace {

// The whole string is a whole number ("-3" is one; "12px", " 12" and "+12" are refused).
bool ParseInt(const std::string& s, int* out) {
    if (s.empty()) return false;
    const char* end = s.data() + s.size();
    const auto r = std::from_chars(s.data(), end, *out);
    return r.ec == std::errc() && r.ptr == end;
}

// A number for a message, as short as it can be ("0.5", not "0.500000").
std::string Num(float v) {
    std::ostringstream os;
    os << v;
    return os.str();
}

// The whole string is a finite number ("1.2"; "nan", "inf" and "1.2x" are refused).
bool ParseNumber(const std::string& s, float* out) {
    if (s.empty()) return false;
    char* end = nullptr;
    const double v = std::strtod(s.c_str(), &end);
    if (end != s.c_str() + s.size() || !std::isfinite(v)) return false;
    *out = static_cast<float>(v);
    return true;
}

// The Voice control an option sets (--speed ...), or nullptr.
const VoiceControl* ControlOption(const std::string& option) {
    static const VoiceControl kSpeed = VoiceControl::Speed, kPitch = VoiceControl::Pitch,
                              kVolume = VoiceControl::Volume;
    if (option == "--speed") return &kSpeed;
    if (option == "--pitch") return &kPitch;
    if (option == "--volume") return &kVolume;
    return nullptr;
}

// "x,y,w,h" -> Rect. x and y from 0, w and h above 0.
bool ParseRect(const std::string& s, Rect* out) {
    int v[4];
    size_t start = 0;
    for (int i = 0; i < 4; ++i) {
        const size_t comma = s.find(',', start);
        if (i < 3 && comma == std::string::npos) return false;
        if (i == 3 && comma != std::string::npos) return false;
        const std::string part = s.substr(start, comma == std::string::npos ? comma : comma - start);
        if (!ParseInt(part, &v[i])) return false;
        start = comma + 1;
    }
    if (v[0] < 0 || v[1] < 0 || v[2] <= 0 || v[3] <= 0) return false;
    if (v[2] > INT_MAX - v[0] || v[3] > INT_MAX - v[1]) return false;  // right or bottom would overflow
    *out = {v[0], v[1], v[0] + v[2], v[1] + v[3]};
    return true;
}

// "en|bg": the codes --lang takes, for the usage text and its error message.
std::string LanguageCodes() {
    std::string ids;
    for (const Language& l : AllLanguages()) {
        if (!ids.empty()) ids += '|';
        ids += l.code;
    }
    return ids;
}

nlohmann::ordered_json BoxToJson(const Rect& r) {
    return {{"left", r.left}, {"top", r.top}, {"right", r.right}, {"bottom", r.bottom}};
}

}  // namespace

// Two steps: the loop reads every option and its value (and stops at the first bad one); then the
// checks below make sure the options form one of the commands in CliUsage (an image, --say,
// or one Voice library command), and say which option is out of place.
CliParse ParseCliArgs(const std::vector<std::string>& args) {
    CliParse p;
    auto fail = [&](const std::string& msg) {
        p.ok = false;
        p.error = msg;
        return p;
    };

    bool hasControl = false;  // a Voice control option was given
    for (size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        if (a == "--help" || a == "-h") {
            p.ok = true;
            p.help = true;
            return p;
        }
        if (a == "--voices-catalogue") {
            p.args.listVoices = true;
            continue;
        }
        if (a == "--voice-download" || a == "--voice-add" || a == "--voices-dir") {
            if (i + 1 >= args.size() || Trim(args[i + 1]).empty()) return fail(a + " needs a value");
            const std::string& v = args[++i];
            (a == "--voice-download" ? p.args.downloadVoice : a == "--voice-add" ? p.args.addVoicePath : p.args.voicesDir) = v;
            continue;
        }
        if (a == "--version") {
            p.ok = true;
            p.version = true;
            return p;
        }
        if (a == "--lang" || a == "--rect" || a == "--json" || a == "--draw" ||
            a == "--score" || a == "--say" || a == "--wav" || ControlOption(a)) {
            if (i + 1 >= args.size()) return fail(a + " needs a value");
            const std::string& v = args[++i];
            if (a == "--lang") {
                if (!FindLanguageByCode(v)) {
                    return fail("unknown language '" + v + "' (use " + LanguageCodes() + ")");
                }
                p.args.langCode = v;
            } else if (a == "--rect") {
                if (!ParseRect(v, &p.args.rect)) {
                    return fail("--rect wants x,y,w,h as whole numbers, w and h above 0 (got '" + v + "')");
                }
                p.args.hasRect = true;
            } else if (a == "--json") {
                p.args.jsonPath = v;
            } else if (a == "--draw") {
                p.args.drawPath = v;
            } else if (a == "--say") {
                if (Trim(v).empty()) return fail("--say needs some text to speak");
                p.args.sayText = v;
            } else if (a == "--wav") {
                p.args.wavPath = v;
            } else if (const VoiceControl* control = ControlOption(a)) {
                const ControlRange r = RangeOf(*control);
                float value = 0.f;
                if (!ParseNumber(v, &value) || value < r.min || value > r.max) {
                    return fail(a + " wants a number from " + Num(r.min) + " to " + Num(r.max) + " (got '" + v + "')");
                }
                Set(p.args.voice, *control, value);
                hasControl = true;
            } else {
                p.args.scorePath = v;
            }
        } else if (a.size() > 1 && a[0] == '-' && a[1] == '-') {
            return fail("unknown option " + a);
        } else {  // not an option: the image
            if (!p.args.imagePath.empty()) return fail("more than one image given ('" + a + "')");
            p.args.imagePath = a;
        }
    }
    // Which command is it? Each command refuses the options of the others.
    const bool imageOptions = !p.args.imagePath.empty() || p.args.hasRect || !p.args.jsonPath.empty() ||
                              !p.args.drawPath.empty() || !p.args.scorePath.empty();  // an image or its options
    const int libraryAsks = (p.args.listVoices ? 1 : 0) + (p.args.downloadVoice.empty() ? 0 : 1) +
                            (p.args.addVoicePath.empty() ? 0 : 1);
    if (libraryAsks > 1) return fail("--voices-catalogue, --voice-download and --voice-add are separate commands; give one");
    if (libraryAsks == 1) {
        if (!p.args.sayText.empty() || !p.args.wavPath.empty() || imageOptions || hasControl) {
            return fail("the Voice library options cannot be combined with an image, --say or their options");
        }
        if (p.args.listVoices && !p.args.voicesDir.empty()) return fail("--voices-dir is for --voice-download and --voice-add");
        p.ok = true;
        return p;
    }
    if (!p.args.voicesDir.empty()) return fail("--voices-dir needs --voice-download or --voice-add");
    if (!p.args.sayText.empty()) {
        if (p.args.wavPath.empty()) return fail("--say needs --wav out.wav");
        if (imageOptions) return fail("--say speaks text; it cannot be combined with an image or its options");
        p.ok = true;
        return p;
    }
    if (!p.args.wavPath.empty()) return fail("--wav needs --say \"text\"");
    if (hasControl) return fail("--speed, --pitch and --volume need --say \"text\"");
    if (p.args.imagePath.empty()) return fail("no image given");
    p.ok = true;
    return p;
}

std::string CliUsage() {
    std::ostringstream os;
    os << "usage: lexia_cli <image.png> [--lang " << LanguageCodes() << "] [--rect x,y,w,h]\n"
       << "                 [--json out.json] [--draw out.png] [--score expected.txt]\n"
       << "       lexia_cli --say \"text\" --wav out.wav [--lang " << LanguageCodes() << "]\n"
       << "                 [--speed n] [--pitch n] [--volume n]\n"
       << "       lexia_cli --voices-catalogue\n"
       << "       lexia_cli --voice-download <name> [--voices-dir folder]\n"
       << "       lexia_cli --voice-add <file.zip|file.onnx|file.onnx.json> [--voices-dir folder]\n"
       << "\n"
       << "Runs Lexia's OCR and layout on a saved Shot.\n"
       << "  --lang    Language of the text (default " << DefaultLanguage().code << ")\n"
       << "  --rect    read only this area, like a Drag (default: the whole Shot, like the Analysis)\n"
       << "  --json    write the Blocks (box, text, Lines) in Reading order, with Language and ms\n"
       << "  --draw    write the Shot with each Block outlined and numbered in Reading order\n"
       << "  --score   compare the Blocks with an expected.txt and print the Fixture score as one line of\n"
       << "            JSON (distance, editDistance, expectedBlocks, producedBlocks, blockCountError)\n"
       << "  --version print Lexia's version and exit\n"
       << "  --say     speak this text with the Language's Voice instead of reading an image\n"
       << "  --wav     the file --say writes (16-bit mono WAV); --say needs it\n"
       << "  --speed, --pitch, --volume\n"
       << "            the Voice controls for --say; ranges and defaults: speed 0.5-2 (1), pitch -6 to 6\n"
       << "            semitones (0), volume 0-2 (1)\n"
       << "  --voices-catalogue\n"
       << "            list the Voices on Hugging Face (rhasspy/piper-voices) for Lexia's Languages in Medium\n"
       << "            quality: name, Language, size, \"installed\" if you have it. This goes online, once.\n"
       << "  --voice-download name\n"
       << "            download that Voice (checked against the catalogue's size and MD5) into your Voice folder\n"
       << "  --voice-add path\n"
       << "            add a Voice from a .zip (holding the .onnx and .onnx.json), a .onnx or a .onnx.json\n"
       << "  --voices-dir folder\n"
       << "            where --voice-download and --voice-add put the Voice (default %LOCALAPPDATA%\\Lexia\\voices)\n"
       << "Without --json, --draw or --score, the Blocks' text goes to stdout, one Block per paragraph,\n"
       << "separated by blank lines (the expected.txt format).\n";
    return os.str();
}

std::string CliVersionText() {
    return std::string("lexia_cli ") + LEXIA_VERSION + "\n";
}

// The expected.txt format of a Fixture: each Block's text, Blocks separated by one blank line.
std::string BlocksToExpectedText(const std::vector<TextBlock>& blocks) {
    std::string out;
    for (size_t i = 0; i < blocks.size(); ++i) {
        if (i > 0) out += '\n';
        out += blocks[i].text;
        out += '\n';
    }
    return out;
}

// --json: { "language", "ms", "blocks": [ { "index" (from 1, the Reading order), "box", "text",
// "lines": [ { "box", "text", "conf" } ] } ] }. Boxes are in Shot pixels.
std::string BlocksToJson(const std::vector<TextBlock>& blocks, const std::string& languageCode,
                         long long ms) {
    nlohmann::ordered_json jBlocks = nlohmann::ordered_json::array();
    int index = 0;
    for (const TextBlock& b : blocks) {
        nlohmann::ordered_json jLines = nlohmann::ordered_json::array();
        for (const TextLine& l : b.lines) {
            const double conf = std::round(static_cast<double>(l.conf) * 10.0) / 10.0;  // 91.2, not 91.19999
            jLines.push_back({{"box", BoxToJson(l.box)}, {"text", l.text}, {"conf", conf}});
        }
        jBlocks.push_back({{"index", ++index},
                           {"box", BoxToJson(b.box)},
                           {"text", b.text},
                           {"lines", std::move(jLines)}});
    }
    const nlohmann::ordered_json doc = {
        {"language", languageCode}, {"ms", ms}, {"blocks", std::move(jBlocks)}};
    // replace: a stray non-UTF-8 byte from the OCR becomes U+FFFD instead of an exception.
    return doc.dump(2, ' ', false, nlohmann::ordered_json::error_handler_t::replace) + "\n";
}

std::string ScoreToJson(const LayoutScore& score) {
    const nlohmann::ordered_json doc = {{"distance", score.distance},
                                        {"editDistance", score.editDistance},
                                        {"expectedBlocks", score.expectedBlocks},
                                        {"producedBlocks", score.producedBlocks},
                                        {"blockCountError", score.blockCountError}};
    return doc.dump() + "\n";
}

// --voices-catalogue: one tab-separated line per Voice: name, Language, size in MB, "installed".
std::string VoiceCatalogueToText(const std::vector<CatalogueVoice>& voices, const std::vector<std::string>& installed) {
    std::ostringstream os;
    os.setf(std::ios::fixed);
    os.precision(1);
    for (const CatalogueVoice& v : voices) {
        const double mb = static_cast<double>(v.model.sizeBytes + v.config.sizeBytes) / (1024.0 * 1024.0);
        os << v.name << '\t' << v.languageName << '\t' << mb << " MB";
        if (std::find(installed.begin(), installed.end(), v.name) != installed.end()) os << "\tinstalled";
        os << '\n';
    }
    return os.str();
}

// The result of --voice-download or --voice-add: "added <name> (<Language>)", or the problem id with
// its detail.
std::string VoiceOutcomeToText(const VoiceOutcome& o) {
    if (o.ok()) return "added " + o.voiceName + " (" + o.languageName + ")\n";
    std::string text = ProblemId(o.problem);
    if (o.problem == VoiceProblem::UnsupportedLanguage) text += " (" + o.languageName + ")";
    if ((o.problem == VoiceProblem::NotMedium || o.problem == VoiceProblem::UnknownQuality) && !o.quality.empty()) text += " (" + o.quality + ")";
    return text + ": " + o.detail + "\n";
}
