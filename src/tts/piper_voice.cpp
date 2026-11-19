#include "tts/piper_voice.h"

#include <windows.h>

#include <piper.h>

#include <chrono>
#include <exception>
#include <set>
#include <vector>

#include "common.h"
#include "log.h"
#include "ort_runtime.h"
#include "tts/onnx_alignment.h"
#include "tts/pitch_shift.h"
#include "tts/voice_library.h"

namespace fs = std::filesystem;

namespace {

// libpiper (and espeak-ng) open files with narrow paths in the ANSI code page. A folder name
// the code page cannot hold (a Cyrillic user name on an English Windows) would turn into '?',
// so such a path is handed over as its 8.3 short name instead, which is always ASCII.
// "" if neither works: path::string() throws when the code page cannot hold the path.
std::string NarrowPath(const fs::path& p) {
    auto narrow = [](const fs::path& path) {
        try {
            return path.string();
        } catch (const std::exception&) {  // std::system_error: not in the code page
            return std::string();
        }
    };
    const std::wstring w = p.wstring();
    BOOL lossy = FALSE;
    const int n = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, w.c_str(), -1, nullptr, 0, nullptr, &lossy);
    if (n > 0 && !lossy) return narrow(p);
    const DWORD len = GetShortPathNameW(w.c_str(), nullptr, 0);
    if (len == 0) return narrow(p);  // no short name (8.3 names switched off): "" if it can't be held
    std::wstring shortPath(len, L'\0');
    shortPath.resize(GetShortPathNameW(w.c_str(), shortPath.data(), len));
    return narrow(fs::path(shortPath));
}

}  // namespace

// One loaded Piper Voice (the ONNX model in memory). libpiper is a C API; this hides its handle.
struct PiperVoice::Impl {
    ~Impl() {
        if (synth) piper_free(synth);
    }
    piper_synthesizer* synth = nullptr;
    std::string name;
};

PiperVoice::PiperVoice(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
PiperVoice::~PiperVoice() = default;

fs::path DefaultAlignedDir() { return AlignedDirIn(LexiaDataDir()); }

namespace {

// Creates the synthesiser for a model file and its config ("<model>.json"). nullptr and `why` when a path cannot
// be handed to libpiper, or libpiper refuses the files (it throws on a corrupt model or config).
piper_synthesizer* CreateSynth(const fs::path& model, const fs::path& espeakData, std::string* why) {
    const fs::path config = model.wstring() + L".json";
    const std::string modelA = NarrowPath(model);
    const std::string configA = NarrowPath(config);
    const std::string dataA = NarrowPath(espeakData);
    if (modelA.empty() || configA.empty() || dataA.empty()) {
        *why = "The Voice's path cannot be handed to Piper (not in the ANSI code page, and no 8.3 short name): " +
               model.u8string();
        return nullptr;
    }
    try {
        piper_synthesizer* synth = piper_create(modelA.c_str(), configA.c_str(), dataA.c_str());
        if (!synth) *why = "Could not load the Voice " + model.filename().u8string();
        return synth;
    } catch (const std::exception& e) {
        *why = std::string("Could not load the Voice: ") + e.what();
        return nullptr;
    }
}

// Names of the Voices whose aligned copy failed: the reason is logged once per Voice per run, not at every
// load (a Voice is loaded again after each change of the Voice list). Speech thread only, like the Voices.
std::set<std::string>& AlignedWarned() {
    static std::set<std::string> warned;
    return warned;
}

}  // namespace

// Loads <name>.onnx with its <name>.onnx.json beside it; espeakData is the espeak-ng-data folder that
// turns text into phonemes. nullptr and *error on failure. The Voice's name is the file name without
// ".onnx" (of the original file, also when a patched copy is what is loaded).
//
// Word timings: a model without the alignment output is replaced by a patched copy (EnsureAlignedCopy). The
// copy is only a help: when it cannot be made (no %LOCALAPPDATA%, a full disk, a model the patch does not fit)
// or libpiper refuses it, the original is loaded, so the person always hears the Voice. The log says that the
// copy was made and how long it took (the first load of a Voice pays for it), or why it was not.
std::unique_ptr<PiperVoice> PiperVoice::Load(const fs::path& model, const fs::path& espeakData,
                                             std::string* error, const fs::path& alignedDir) {
    auto fail = [&](const std::string& why) {
        if (error) *error = why;
        return std::unique_ptr<PiperVoice>();
    };
    // ADR-0005: ONNX Runtime telemetry must be off before piper.dll creates any session, and a Voice can load
    // in a process that has no OcrEngine (the tests, the tools). The switch is process-wide and this call is
    // cheap and safe to repeat, so every load makes sure of it. Never throws.
    StartOnnxRuntime();
    std::error_code ec;
    if (!fs::is_regular_file(model, ec)) return fail("Voice file not found: " + model.u8string());
    const fs::path config = model.wstring() + L".json";
    if (!fs::is_regular_file(config, ec)) return fail("Voice config not found: " + config.u8string());
    if (!fs::is_directory(espeakData, ec)) return fail("espeak-ng-data not found: " + espeakData.u8string());

    const std::string name = model.stem().u8string();
    piper_synthesizer* synth = nullptr;
    std::string why;
    if (!alignedDir.empty()) {
        const auto started = std::chrono::steady_clock::now();
        const AlignedCopy copy = EnsureAlignedCopy(model, alignedDir);
        if (!copy.model.empty()) {
            if (copy.created) {
                const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
                LogInfo("[voice] Made the aligned copy of the Voice ", name, " (word timings for the read-along) in ", ms.count(), " ms.");
            }
            std::string copyWhy;
            synth = CreateSynth(copy.model, espeakData, &copyWhy);
            if (!synth) {
                // A copy libpiper refuses (damaged on disk, say) is deleted, so the next start makes a new one.
                RemoveAlignedCopies(alignedDir, name);
                if (AlignedWarned().insert(name).second) {
                    LogWarn("[voice] The aligned copy of the Voice ", name, " could not be loaded, so the original is used: ", copyWhy);
                }
            }
        } else if (!copy.alreadyAligned && AlignedWarned().insert(name).second) {
            LogWarn("[voice] No aligned copy of the Voice ", name, ", so it speaks without word timings (no read-along mark): ", copy.why);
        }
    }
    if (!synth) synth = CreateSynth(model, espeakData, &why);
    if (!synth) return fail(why);
    auto impl = std::make_unique<Impl>();
    impl->synth = synth;
    impl->name = name;
    return std::unique_ptr<PiperVoice>(new PiperVoice(std::move(impl)));
}

const std::string& PiperVoice::Name() const { return impl_->name; }

// Speaks `utf8` with the Voice controls applied. Piper gives the speech a sentence (a chunk) at a time;
// each chunk gets Pitch and Volume applied and goes to onChunk at once, so playback can start before
// the whole text is synthesised. onChunk returns false to stop (Cancelled).
SynthResult PiperVoice::Synthesize(const std::string& utf8, const VoiceControls& controls,
                                   const std::function<bool(const SpeechChunk&)>& onChunk) {
    piper_synthesizer* synth = impl_->synth;
    piper_synthesize_options options = piper_default_synthesize_options(synth);
    // The defaults are the Voice's own values from its .onnx.json; Speed scales the length_scale,
    // the noise values stay the Voice's own.
    options.length_scale *= PaceForSpeed(controls.speed);
    try {  // libpiper may throw here too, as in piper_synthesize_next
        if (piper_synthesize_start(synth, utf8.c_str(), &options) != PIPER_OK) return SynthResult::Failed;
    } catch (const std::exception& e) {
        LogError("Speech synthesis failed: ", e.what());
        return SynthResult::Failed;
    }

    for (;;) {
        piper_audio_chunk c;
        int rc;
        try {
            rc = piper_synthesize_next(synth, &c);
        } catch (const std::exception& e) {  // an ONNX Runtime error
            LogError("Speech synthesis failed: ", e.what());
            return SynthResult::Failed;
        }
        if (rc < 0) return SynthResult::Failed;
        // The last chunk comes back together with PIPER_DONE, so it is handled before the check.
        if (c.num_samples > 0) {
            SpeechChunk out;
            out.samples.assign(c.samples, c.samples + c.num_samples);
            out.sampleRate = c.sample_rate;
            out.isLast = c.is_last;
            if (controls.pitch != 0.f) out.samples = ShiftPitch(out.samples, out.sampleRate, controls.pitch);
            if (controls.volume != 1.f) ApplyVolume(out.samples, controls.volume);
            // libpiper keeps the phonemes of earlier sentences in `phonemes`: keep this chunk's own (OwnPhonemes).
            if (c.phonemes && c.alignments) {
                out.phonemes = PhonemeTimings(OwnPhonemes(std::vector<char32_t>(c.phonemes, c.phonemes + c.num_phonemes), c.num_phoneme_ids),
                                              std::vector<int>(c.alignments, c.alignments + c.num_alignments));
            }
            if (!onChunk(out)) return SynthResult::Cancelled;
        }
        if (rc == PIPER_DONE) return SynthResult::Done;
    }
}

// The shipped Voices and espeak-ng-data beside lexia.exe, and the user's Voices in %LOCALAPPDATA%.
VoiceFolders DefaultVoiceFolders() { return {ExeDir() / "voices", ExeDir() / "espeak-ng-data", UserVoicesDir()}; }

std::unique_ptr<PiperVoice> LoadVoiceForLanguage(const std::string& languageCode,
                                                 const VoiceFolders& folders, std::string* error,
                                                 const std::string& preferredName) {
    auto fail = [&](const std::string& why) {
        if (error) *error = why;
        return std::unique_ptr<PiperVoice>();
    };
    // The shipped Voice for the Language first, else one the user added or downloaded.
    const std::vector<InstalledVoice> installed = ListInstalledVoices(folders.voices, folders.userVoices);
    if (const InstalledVoice* v = PickVoice(installed, languageCode, preferredName)) {
        return PiperVoice::Load(v->model, folders.espeakData, error);
    }
    return fail("no Voice for the Language '" + languageCode + "' in " + (folders.voices / "voices.json").u8string() +
                (folders.userVoices.empty() ? std::string() : " or " + folders.userVoices.u8string()));
}
