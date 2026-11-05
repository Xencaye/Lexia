#include "tts/piper_voice.h"

#include <windows.h>

#include <piper.h>

#include <exception>
#include <vector>

#include "common.h"
#include "log.h"
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

// Loads <name>.onnx with its <name>.onnx.json beside it; espeakData is the espeak-ng-data folder that
// turns text into phonemes. nullptr and *error on failure. The Voice's name is the file name without
// ".onnx".
std::unique_ptr<PiperVoice> PiperVoice::Load(const fs::path& model, const fs::path& espeakData,
                                             std::string* error) {
    auto fail = [&](const std::string& why) {
        if (error) *error = why;
        return std::unique_ptr<PiperVoice>();
    };
    std::error_code ec;
    if (!fs::is_regular_file(model, ec)) return fail("Voice file not found: " + model.u8string());
    const fs::path config = model.wstring() + L".json";
    if (!fs::is_regular_file(config, ec)) return fail("Voice config not found: " + config.u8string());
    if (!fs::is_directory(espeakData, ec)) return fail("espeak-ng-data not found: " + espeakData.u8string());

    const std::string modelA = NarrowPath(model);
    const std::string configA = NarrowPath(config);
    const std::string dataA = NarrowPath(espeakData);
    if (modelA.empty() || configA.empty() || dataA.empty()) {
        return fail("The Voice's path cannot be handed to Piper (not in the ANSI code page, and no 8.3 short name): " +
                    model.u8string());
    }
    piper_synthesizer* synth = nullptr;
    try {  // libpiper throws on a corrupt model or config
        synth = piper_create(modelA.c_str(), configA.c_str(), dataA.c_str());
    } catch (const std::exception& e) {
        return fail(std::string("Could not load the Voice: ") + e.what());
    }
    if (!synth) return fail("Could not load the Voice " + model.filename().u8string());
    auto impl = std::make_unique<Impl>();
    impl->synth = synth;
    impl->name = model.stem().u8string();
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
            if (c.phonemes && c.alignments) {
                out.phonemes = PhonemeTimings(std::vector<char32_t>(c.phonemes, c.phonemes + c.num_phonemes),
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
