#pragma once
// A Piper Voice loaded in memory (piper1-gpl's libpiper: ONNX Runtime + espeak-ng, in this
// process). Synthesis is synchronous and not thread-safe: call every Voice from one thread.
// Win32-free interface; the implementation handles the Windows path encodings.
#include <filesystem>
#include <functional>
#include <memory>
#include <string>

#include "tts/speech_chunk.h"
#include "tts/voice_controls.h"

class PiperVoice {
public:
    // `model` is the Voice's .onnx file; its config is "<model>.json" beside it. `espeakData`
    // is the espeak-ng-data folder. nullptr and a line in `error` (if given) when a file is
    // missing or unreadable. Loading takes a moment, so do it off the UI thread.
    // Free all Voices together: libpiper shuts espeak-ng down when any one of them is freed.
    static std::unique_ptr<PiperVoice> Load(const std::filesystem::path& model,
                                            const std::filesystem::path& espeakData,
                                            std::string* error = nullptr);
    ~PiperVoice();
    PiperVoice(const PiperVoice&) = delete;
    PiperVoice& operator=(const PiperVoice&) = delete;

    // Speaks `utf8` as a series of chunks, about one per sentence, handing each to `onChunk`
    // as soon as it is ready. `onChunk` returns false to stop (then Cancelled). `controls` are
    // applied here: Speed scales the Voice's own length_scale, Pitch is shifted after synthesis
    // (the chunk keeps its length, so its phoneme timings stay valid) and Volume
    // multiplies the samples. They are used as given (clamp them with Clamped); a Speed of 0 or less counts as 1.
    // Text with nothing to say gives no chunks and Done.
    SynthResult Synthesize(const std::string& utf8, const VoiceControls& controls,
                           const std::function<bool(const SpeechChunk&)>& onChunk);

    // The Voice's name: its file name without ".onnx" (the key of its Presets in settings.json).
    const std::string& Name() const;

private:
    struct Impl;
    explicit PiperVoice(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

// Where a Voice's files are: voices.json and the .onnx files in `voices`, espeak-ng's data in
// `espeakData`, and the Voices the user added or downloaded in `userVoices` (voice_library.h;
// empty = none).
struct VoiceFolders {
    std::filesystem::path voices;
    std::filesystem::path espeakData;
    std::filesystem::path userVoices;
};

// Where the app looks: `voices` and `espeak-ng-data` next to the exe, and UserVoicesDir().
VoiceFolders DefaultVoiceFolders();

// Loads the Voice for a Language code: `preferredName` (the Voice tab's choice) if it is installed for
// it, else the one voices.json names, else the first the user added (PickVoice). nullptr and a line in `error` (if given) when the Language has no Voice or its files
// are missing.
std::unique_ptr<PiperVoice> LoadVoiceForLanguage(const std::string& languageCode,
                                                 const VoiceFolders& folders,
                                                 std::string* error = nullptr,
                                                 const std::string& preferredName = std::string());
