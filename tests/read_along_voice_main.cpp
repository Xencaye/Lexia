// A check of the read-along against REAL Piper Voices (ctest `read_along_voice`, exe `lexia_voice_test`).
// Every other read-along test uses chunks that a test made by hand. This one loads each English Voice that
// voices.json ships (from the dev `voices\` folder, with the built espeak-ng data), speaks hard Blocks through
// the same path as the app (SpeakStretches -> PiperVoice::Synthesize -> ReadingQueue::Add / Pump, which cuts
// the words and calls SpokenWordRanges), and reads the heard word, piece by piece, from a fake sink that
// "plays" one word at a time. Then it checks that the heard ranges agree with the text.
//
// The invariants for one Block (T = its text words: runs between spaces, without the punctuation at their
// ends, that hold a letter or a digit):
//   1. every heard range is exactly one text word (so it lies inside the Block and on word edges);
//   2. the ranges never go back, and a range never jumps over a text word (the next heard word is the same
//      text word or the next one);
//   3. the first heard word is the first text word and the last one is the last text word, so with 2 every
//      text word is covered at least once.
// A Voice may say one text word as several words ("1,250" is "one thousand two hundred and fifty"): then
// several heard words stand on one text word, which 2 allows. A text word that is never heard is not allowed,
// with one known exception: libpiper leaves out the space after an ellipsis, so "Wait... then go." has two
// spoken words, "Wait then" and "go". The text word after an ellipsis may then be skipped (one range is one
// text word), and nothing else may.
//
// A Voice must also give phoneme timings at all. libpiper reads them from a second output of the model (the
// w_ceil tensor, which piper's patch_voice_with_alignment.py exposes); the files of rhasspy/piper-voices have
// none. PiperVoice::Load therefore loads a patched copy (onnx_alignment.h), and this test loads every Voice
// through that production path with a cache folder of its own, so the shipped Voices must pass. If a Voice still
// gives no timings, the patch did not happen, and that is a failure of its own.
//
// No network, no new input: the Voice files come from the dev folder named at build time. Without a loadable
// Voice it prints `SKIP read_along_voice: ...` and exits 0 (ctest reads that line as a skip).
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "common.h"
#include "language.h"
#include "read_along.h"
#include "tts/piper_voice.h"
#include "tts/reading_queue.h"
#include "tts/stretch_speech.h"
#include "tts/voices.h"

#include "fake_audio_sink.h"

#ifndef LEXIA_DEV_VOICES_DIR
#error "LEXIA_DEV_VOICES_DIR must be defined by CMake"
#endif
#ifndef LEXIA_DEV_ESPEAK_DATA_DIR
#error "LEXIA_DEV_ESPEAK_DATA_DIR must be defined by CMake"
#endif

namespace {

namespace fs = std::filesystem;

// The Blocks. They are the cases where the sentence rules of SpokenWordRanges and the way espeak-ng speaks
// numbers, abbreviations and punctuation could disagree. The game-text one is in capitals, as an OCR of a
// game gives it.
const std::vector<std::string> kBlocks = {
    "The library keeps an anonymized account of every book.",
    "It costs 1,250 coins.",
    "Dr. Smith and e.g. Mr. Jones",
    "Wait... then go.",
    "\xE2\x80\x9C" "Hello,\xE2\x80\x9D she said.",  // “Hello,” she said.
    "It is a well-known fact.",
    "Really?! Yes.",
    "It is 5 p.m. now.",
    "The cat sat. The dog ran away.",
    "HOW DO YOU FEEL ABOUT YOUR CREATION?",
    "In 2024 the old mill on the hill was sold for 3.5 million pounds, and the new owners, who had never seen "
    "it, said that they would keep every stone.",
    "Stop! Look around. Who is there? Nobody answers, so you walk on.",
};

// Is this code point punctuation or a symbol rather than a letter or digit? The Blocks hold ASCII and the
// curly quotes only, so the General Punctuation block is enough here (this is the test's own definition,
// not read_along.cpp's).
bool IsPunctuationCodePoint(char32_t cp) {
    if (cp < 0x80) return !((cp >= '0' && cp <= '9') || (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z'));
    return cp >= 0x2000 && cp <= 0x206F;
}

// The code point that starts at text[i]; `len` gets its length in bytes (valid UTF-8 only: the Blocks are).
char32_t CodePointAt(const std::string& text, size_t i, size_t* len) {
    const unsigned char c = static_cast<unsigned char>(text[i]);
    size_t n = 1;
    char32_t cp = c;
    if (c >= 0xF0) { n = 4; cp = c & 0x07; }
    else if (c >= 0xE0) { n = 3; cp = c & 0x0F; }
    else if (c >= 0xC0) { n = 2; cp = c & 0x1F; }
    for (size_t k = 1; k < n && i + k < text.size(); ++k) cp = (cp << 6) | (static_cast<unsigned char>(text[i + k]) & 0x3F);
    *len = n;
    return cp;
}

// The text words of a Block: runs of non-space characters, cut at the first and last letter or digit. A run
// with none (a lone "-") is not a word. Written here on its own, so it is a check of read_along.cpp, not a copy.
std::vector<TextRange> TextWords(const std::string& text) {
    std::vector<TextRange> words;
    size_t i = 0;
    while (i < text.size()) {
        while (i < text.size() && (text[i] == ' ' || text[i] == '\n' || text[i] == '\t')) ++i;
        const size_t tokenStart = i;
        while (i < text.size() && !(text[i] == ' ' || text[i] == '\n' || text[i] == '\t')) ++i;
        size_t first = std::string::npos, last = 0;
        for (size_t k = tokenStart; k < i;) {
            size_t len = 1;
            const char32_t cp = CodePointAt(text, k, &len);
            if (!IsPunctuationCodePoint(cp)) {
                if (first == std::string::npos) first = k;
                last = k + len;
            }
            k += len;
        }
        if (first != std::string::npos) words.push_back({first, last});
    }
    return words;
}

std::string Snippet(const std::string& text, TextRange r) {
    if (r.end > text.size() || r.start >= r.end) return "<bad range>";
    return text.substr(r.start, r.end - r.start);
}

// Speaks `block` through a ReadingQueue (the app's path, with a real Voice) and returns the heard words in
// the order they were heard, each distinct heard range once in a row. The fake sink holds one word at a time
// (`sinkSeconds` is tiny), and "plays" it between two looks at HeardWordNow.
std::vector<TextRange> HeardRanges(PiperVoice& voice, const std::string& block, std::string* problem) {
    FakeAudioSink sink;
    // The synthesiser is the Speaker's: one Voice for English, the controls at their defaults.
    BlockSynthesizer synthesize = [&voice](const std::string& text, const std::function<bool(const SpeechChunk&)>& onChunk) {
        return SpeakStretches(
            text, [](const Language& lang) { return lang.code == "en"; },
            [&voice](const Language&, const std::string& stretch, const std::function<bool(const SpeechChunk&)>& send) {
                return voice.Synthesize(stretch, VoiceControls{}, send);
            },
            onChunk);
    };
    ReadingQueue queue(sink, synthesize, /*aheadSeconds=*/1e9, /*sinkSeconds=*/0.001);
    const uint64_t id = queue.Add({block});
    std::vector<TextRange> heard;
    if (id == 0) {
        *problem = "the queue ignored the Block";
        return heard;
    }
    // The first Pump synthesises the whole Block (the queue is told to stay as far ahead as it likes); each
    // later Pump gives the sink the next word. A bound on the loop turns a stuck queue into a failure.
    for (int step = 0; step < 100000 && queue.Active(); ++step) {
        queue.Pump();
        const std::optional<HeardWord> now = queue.HeardWordNow();
        if (now && now->readingId == id && now->block == 0) {
            const bool same = !heard.empty() && heard.back().start == now->range.start && heard.back().end == now->range.end;
            if (!same) heard.push_back(now->range);
        }
        sink.Drain();  // the sound card plays the word
    }
    if (queue.Active()) *problem = "the queue did not finish";
    return heard;
}

// Does the text between the text words `before` and `word` hold an ellipsis ("..." or U+2026)?
bool EllipsisBefore(const std::string& text, TextRange before, TextRange word) {
    const std::string gap = text.substr(before.end, word.start - before.end);
    return gap.find("...") != std::string::npos || gap.find("\xE2\x80\xA6") != std::string::npos;
}

// Checks the invariants above; returns "" when they hold, else a line saying which one failed.
std::string CheckRanges(const std::string& block, const std::vector<TextRange>& heard) {
    const std::vector<TextRange> words = TextWords(block);
    if (words.empty()) return "the Block has no text word";
    if (heard.empty()) return "nothing was heard";
    size_t previous = 0;
    for (size_t i = 0; i < heard.size(); ++i) {
        size_t index = words.size();
        for (size_t w = 0; w < words.size(); ++w) {
            if (words[w].start == heard[i].start && words[w].end == heard[i].end) index = w;
        }
        if (index == words.size()) {
            return "heard word " + std::to_string(i) + " is not a text word: [" + std::to_string(heard[i].start) + "," +
                   std::to_string(heard[i].end) + ") = \"" + Snippet(block, heard[i]) + "\"";
        }
        if (i == 0 && index != 0) return "the first heard word is text word " + std::to_string(index) + ", not 0";
        if (i > 0 && index < previous) return "heard word " + std::to_string(i) + " goes back to text word " + std::to_string(index);
        if (i > 0 && index > previous + 1) {
            // The one jump that is explained: Piper says the words around an ellipsis as ONE word ("Wait... then"
            // is one spoken word, libpiper leaves the space out), so the text word after an ellipsis has no
            // sound of its own and is skipped. Every other skipped word is a failure.
            for (size_t skipped = previous + 1; skipped < index; ++skipped) {
                if (!EllipsisBefore(block, words[skipped - 1], words[skipped])) {
                    return "heard word " + std::to_string(i) + " jumps from text word " + std::to_string(previous) + " to " +
                           std::to_string(index) + ", over \"" + Snippet(block, words[skipped]) + "\"";
                }
            }
        }
        previous = index;
    }
    if (previous + 1 != words.size()) {
        return "the last heard word is text word " + std::to_string(previous) + " of " + std::to_string(words.size()) +
               ": the words after it were never heard";
    }
    return "";
}

// Prints how the Voice cut the Block: for each chunk (about one per sentence) its offset in the Block and the
// spoken phonemes of each of its word pieces (0 = silence or punctuation only). This is the input of
// SpokenWordRanges, so it shows where a wrong range comes from.
void PrintChunks(PiperVoice& voice, const std::string& block) {
    std::printf("  Chunks (spoken phonemes of each word piece):");
    int chunkNumber = 0;
    voice.Synthesize(block, VoiceControls{}, [&chunkNumber](const SpeechChunk& chunk) {
        std::printf("\n    chunk %d:", chunkNumber++);
        for (const WordPiece& piece : CutIntoWords(chunk)) std::printf(" %d", piece.phonemes);
        std::printf("   [");
        for (const PhonemeTiming& p : chunk.phonemes) {
            if (p.phoneme < 0x80) std::printf("%c", static_cast<char>(p.phoneme));
            else std::printf("<%X>", static_cast<unsigned>(p.phoneme));
        }
        std::printf("]");
        return true;
    });
    std::printf("\n");
}

// Prints what was expected and what was heard, for a failed Block, and how the Voice cut it.
void Report(PiperVoice& voice, const std::string& voiceName, const std::string& block, const std::string& why,
            const std::vector<TextRange>& heard) {
    std::printf("FAIL: read_along_voice\n  Voice: %s\n  Block: %s\n  Why: %s\n  Text words:", voiceName.c_str(), block.c_str(), why.c_str());
    for (const TextRange& w : TextWords(block)) std::printf(" [%zu,%zu)\"%s\"", w.start, w.end, Snippet(block, w).c_str());
    std::printf("\n  Heard:     ");
    for (const TextRange& h : heard) std::printf(" [%zu,%zu)\"%s\"", h.start, h.end, Snippet(block, h).c_str());
    std::printf("\n");
    PrintChunks(voice, block);
}

// Does this Voice give phoneme timings with its chunks? (Only a model with the alignment output does.)
bool SpeaksWithTimings(PiperVoice& voice) {
    bool any = false;
    voice.Synthesize("One two three.", VoiceControls{}, [&any](const SpeechChunk& chunk) {
        if (!chunk.phonemes.empty()) any = true;
        return true;
    });
    return any;
}

// The English Voices of the dev voices.json that are shipped (a "ship": false entry is skipped) and whose files
// are in the dev folder. `why` says why there are none.
std::vector<std::string> ShippedEnglishVoices(const fs::path& voicesDir, std::string* why) {
    std::string text;
    if (!ReadFileToString(voicesDir / "voices.json", &text)) {
        *why = "no voices.json in " + voicesDir.string();
        return {};
    }
    std::set<std::string> notShipped;
    const nlohmann::json json = nlohmann::json::parse(text, nullptr, /*allow_exceptions=*/false);
    if (json.is_object() && json.contains("voices") && json["voices"].is_array()) {
        for (const nlohmann::json& v : json["voices"]) {
            if (v.is_object() && v.value("ship", true) == false && v.contains("name") && v["name"].is_string()) {
                notShipped.insert(v["name"].get<std::string>());
            }
        }
    }
    std::string error;
    std::vector<std::string> names;
    for (const VoiceEntry& entry : ParseVoiceList(text, &error)) {
        if (entry.language != "en" || notShipped.count(entry.name)) continue;
        std::error_code ec;
        if (fs::exists(voicesDir / (entry.name + ".onnx"), ec) && fs::exists(voicesDir / (entry.name + ".onnx.json"), ec)) {
            names.push_back(entry.name);
        }
    }
    if (names.empty()) *why = "no shipped English Voice with its files in " + voicesDir.string() + " (run scripts\\fetch_deps.ps1)";
    return names;
}

}  // namespace

int main() {
    const fs::path voicesDir = fs::path(LEXIA_DEV_VOICES_DIR);
    const fs::path espeakData = fs::path(LEXIA_DEV_ESPEAK_DATA_DIR);

    std::string why;
    const std::vector<std::string> names = ShippedEnglishVoices(voicesDir, &why);
    if (names.empty()) {
        std::printf("SKIP read_along_voice: %s\n", why.c_str());
        return 0;
    }

    // The Voices are loaded through the production path, PiperVoice::Load: a model without the alignment output
    // is replaced by a patched copy in `alignedDir` (onnx_alignment.h). The folder is the test's own, in the temp
    // folder and made new, so the first load makes the copies (the time is printed) and %LOCALAPPDATA% is not
    // touched. All the Voices are loaded before any is used and freed together at the end: libpiper shuts
    // espeak-ng down when any one Voice is freed (piper_voice.h).
    const fs::path alignedDir = fs::temp_directory_path() / "lexia_voice_test_aligned";
    std::error_code ec;
    fs::remove_all(alignedDir, ec);
    std::vector<std::unique_ptr<PiperVoice>> voices;
    for (const std::string& name : names) {
        std::string error;
        const auto started = std::chrono::steady_clock::now();
        std::unique_ptr<PiperVoice> voice = PiperVoice::Load(voicesDir / (name + ".onnx"), espeakData, &error, alignedDir);
        if (!voice) {
            std::printf("SKIP read_along_voice: %s could not be loaded: %s\n", name.c_str(), error.c_str());
            return 0;
        }
        const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
        // What the copy of this Voice weighs on disk (the model and its config).
        std::uintmax_t copyBytes = 0;
        for (fs::directory_iterator it(alignedDir, ec), end; !ec && it != end; it.increment(ec)) {
            if (it->path().filename().u8string().rfind(name + ".", 0) == 0) copyBytes += it->file_size(ec);
        }
        std::printf("INFO read_along_voice: %s loaded in %lld ms, aligned copy %.1f MB\n", name.c_str(), ms,
                    static_cast<double>(copyBytes) / (1024.0 * 1024.0));
        voices.push_back(std::move(voice));
    }

    int failures = 0;
    for (size_t v = 0; v < voices.size(); ++v) {
        // The Voice must give the phoneme timings the read-along (and the word-boundary Announcements) are made
        // from. If it does not, the production patch did not happen (or does not fit this model): the Block
        // checks would only say "nothing was heard", so the Voice fails here and is not checked further.
        if (!SpeaksWithTimings(*voices[v])) {
            ++failures;
            std::printf("FAIL: read_along_voice\n  Voice: %s\n  Why: the Voice gives no phoneme timings: its .onnx has no alignment output "
                        "and PiperVoice::Load did not make the patched copy, so the Reading queue cannot cut words and the "
                        "read-along has nothing to show.\n",
                        names[v].c_str());
            continue;
        }
        PiperVoice* voice = voices[v].get();
        const std::string& label = names[v];
        int voiceFailures = 0;
        for (const std::string& block : kBlocks) {
            std::string problem;
            const std::vector<TextRange> heard = HeardRanges(*voice, block, &problem);
            const std::string why2 = problem.empty() ? CheckRanges(block, heard) : problem;
            if (!why2.empty()) {
                Report(*voice, label, block, why2, heard);
                ++voiceFailures;
            }
        }
        if (voiceFailures == 0) std::printf("OK read_along_voice: %s, %zu Blocks\n", label.c_str(), kBlocks.size());
        failures += voiceFailures;
    }

    // The copies are reused: a second load of the first Voice finds its copy (fast, nothing new is written).
    if (!voices.empty() && failures == 0) {
        std::size_t before = 0, after = 0;
        for (fs::directory_iterator it(alignedDir, ec), end; !ec && it != end; it.increment(ec)) ++before;
        std::string error;
        const auto started = std::chrono::steady_clock::now();
        std::unique_ptr<PiperVoice> again = PiperVoice::Load(voicesDir / (names[0] + ".onnx"), espeakData, &error, alignedDir);
        const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
        for (fs::directory_iterator it(alignedDir, ec), end; !ec && it != end; it.increment(ec)) ++after;
        if (!again || !SpeaksWithTimings(*again) || before != after) {
            ++failures;
            std::printf("FAIL: read_along_voice\n  Voice: %s\n  Why: the second load did not reuse the aligned copy (%zu files before, %zu after)\n",
                        names[0].c_str(), before, after);
        } else {
            std::printf("INFO read_along_voice: the second load of %s took %lld ms and reused its copy\n", names[0].c_str(), ms);
        }
    }
    voices.clear();
    fs::remove_all(alignedDir, ec);
    return failures == 0 ? 0 : 1;
}
