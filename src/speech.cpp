#include "speech.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "language.h"
#include "log.h"
#include "strings/string_ids.h"
#include "strings/strings.h"
#include "tts/piper_voice.h"
#include "tts/reading_queue.h"
#include "tts/speech_key.h"
#include "tts/stretch_speech.h"
#include "tts/waveout_sink.h"

namespace {
// How far synthesis may run ahead of what is heard, and how much of that the sound card holds:
// the rest waits in the queue, so that an Announcement can interrupt at a word boundary. The
// shorter the second, the sooner an Announcement is heard; below about half a second the audio
// could run dry between words if the machine is busy.
constexpr double kSynthesisAheadSeconds = 3.0;
constexpr double kSinkSeconds = 0.5;
}  // namespace

struct Speaker::Impl {
    Impl()
        : queue(sink, [this](const std::string& text, const std::function<bool(const SpeechChunk&)>& onChunk) {
              return SpeakBlock(text, onChunk);
          }, kSynthesisAheadSeconds, kSinkSeconds) {}

    ~Impl() {
        {
            std::lock_guard<std::mutex> lk(wakeMutex);
            quitting = true;
        }
        wake.notify_all();
        queue.Stop();  // cuts off a Block that is being synthesised, so the thread can end
        if (worker.joinable()) worker.join();
    }

    bool Init() {
        folders = DefaultVoiceFolders();
        std::error_code ec;
        if (!std::filesystem::is_regular_file(folders.voices / "voices.json", ec) ||
            !std::filesystem::is_directory(folders.espeakData, ec)) {
            LogError("The Voices were not found: expected the voices folder (with voices.json) and espeak-ng-data next to lexia.exe.");
            return false;
        }
        ready = true;  // before the thread starts: it reads `ready` (AnnounceText, inside Pump)
        worker = std::thread([this] { Work(); });
        return true;
    }

    uint64_t Add(std::vector<std::string> blocks, bool append) {
        if (!ready) return 0;  // Init failed: Lexia runs without speech
        if (!append) {
            speechKey.Cancel();
            queue.Stop();
        }
        const uint64_t id = queue.Add(std::move(blocks));
        wake.notify_all();
        return id;
    }

    void Work() {
        std::unique_lock<std::mutex> lk(wakeMutex);
        while (!quitting) {
            lk.unlock();
            if (releaseRequested) {  // between two Pumps: nothing uses a Voice now
                voices.clear();
                {
                    std::lock_guard<std::mutex> rl(releaseMutex);
                    releaseRequested = false;
                }
                releaseDone.notify_all();
            }
            const bool didWork = queue.Pump();
            lk.lock();
            // Nothing to do: wait for a new Pick, or look again soon at whether playback ended.
            if (!didWork && !quitting) wake.wait_for(lk, std::chrono::milliseconds(20));
        }
    }

    // Speech thread, at the start of what it speaks: a changed choice of Voices drops every loaded Voice.
    void RefreshVoices() {
        const unsigned epoch = voiceEpoch.load();
        if (epoch == seenVoiceEpoch) return;
        seenVoiceEpoch = epoch;
        voices.clear();
    }

    std::string ActiveVoiceFor(const Language& lang) {
        std::lock_guard<std::mutex> lk(activeVoicesMutex);
        const auto it = activeVoices.find(lang.code);
        return it == activeVoices.end() ? std::string() : it->second;
    }

    // Loaded on the speech thread the first time a Language needs it; null if it can't be.
    PiperVoice* VoiceFor(const Language& lang) {
        auto it = voices.find(lang.code);
        if (it != voices.end()) return it->second.get();
        std::string error;
        auto voice = LoadVoiceForLanguage(lang.code, folders, &error, ActiveVoiceFor(lang));
        if (!voice) {
            LogWarn("[voice] No ", lang.name, " Voice, so ", lang.name, " text is read with the Voice of the rest of its Block, or not at all if that has none: ", error);
        }
        return (voices[lang.code] = std::move(voice)).get();
    }

    // The Reading queue's synthesiser: the Block is spoken as its stretches of one script, each by the
    // Voice of its Language (stretch_speech.h), with that Voice's controls (and the global rate
    // and volume) applied. A Language whose Voice is missing is announced, once.
    SynthResult SpeakBlock(const std::string& text, const std::function<bool(const SpeechChunk&)>& onChunk) {
        RefreshVoices();
        const SynthResult result = SpeakStretches(
            text,
            [this](const Language& lang) {
                if (VoiceFor(lang)) return true;
                if (announcedMissing.insert(lang.code).second) AnnounceVoiceMissing(lang);  // once per Language
                return false;
            },
            [this](const Language& lang, const std::string& stretch, const std::function<bool(const SpeechChunk&)>& send) {
                return SpeakStretch(*VoiceFor(lang), stretch, send);
            },
            onChunk);
        if (result == SynthResult::Failed) LogWarn("[voice] The Voice could not speak a Block.");
        return result;
    }

    SynthResult SpeakStretch(PiperVoice& voice, const std::string& text, const std::function<bool(const SpeechChunk&)>& onChunk) {
        VoiceControls controls;
        {
            std::lock_guard<std::mutex> lk(controlsMutex);
            const auto it = voiceControls.find(voice.Name());
            if (it != voiceControls.end()) controls = it->second;
        }
        // Each step of the global rate setting doubles or halves the Speed over ten steps.
        controls.speed *= static_cast<float>(std::pow(2.0, rate.load() / 10.0));
        controls.volume *= static_cast<float>(volume.load()) / 100.f;
        return voice.Synthesize(text, controls, onChunk);
    }

    std::string InterfaceLanguage() {
        std::lock_guard<std::mutex> lk(interfaceMutex);
        return interfaceLanguage;
    }

    // The Voice for Announcements: that of the Language whose code is the Interface language, else
    // any Voice that is there. Speech thread only. Null if there is none at all. The fallback is
    // logged once per Interface language, not at every Announcement.
    PiperVoice* InterfaceVoice() {
        const std::string code = InterfaceLanguage();
        const Language* wanted = FindLanguageByCode(code);
        if (wanted) {
            if (PiperVoice* voice = VoiceFor(*wanted)) return voice;
        }
        const bool log = interfaceFallbackLogged.insert(code).second;
        for (const Language& l : AllLanguages()) {
            if (PiperVoice* voice = VoiceFor(l)) {
                if (log) {
                    LogWarn("[voice] No Voice for the Interface language \"", code,
                            "\": Announcements are spoken with the ", l.name, " Voice.");
                }
                return voice;
            }
        }
        if (log) LogWarn("[voice] No Voice at all: Announcements are not spoken.");
        return nullptr;
    }

    // Speaks `text` with `voice` (its controls, the set rate and volume), for Announcements.
    SynthResult Say(PiperVoice* voice, const std::string& text,
                    const std::function<bool(const SpeechChunk&)>& onChunk) {
        if (!voice) return SynthResult::Failed;
        const SynthResult result = SpeakStretch(*voice, text, onChunk);
        if (result == SynthResult::Failed) LogWarn("[voice] The Voice could not speak.");
        return result;
    }

    // The three below run on the speech thread, inside Pump, with no queue lock held, so they
    // may call the queue.
    void AnnounceVoiceMissing(const Language& lang) { AnnounceText(VoiceMissingText(lang)); }

    // "No Voice for <the Language>", in the Interface language.
    std::string VoiceMissingText(const Language& lang) {
        const std::string code = InterfaceLanguage();
        return strings::Format(code, strings::id::kAnnounceVoiceMissingFor,
                               {{"language", strings::LanguageName(code, lang.id)}});
    }

    // Queues an Announcement of `text` (already in the Interface language), spoken by the
    // Interface language's Voice.
    void AnnounceText(std::string text) {
        if (!ready) return;
        queue.Announce([this, text = std::move(text)](const std::function<bool(const SpeechChunk&)>& onChunk) {
            RefreshVoices();
            return Say(InterfaceVoice(), text, onChunk);
        });
        wake.notify_all();
    }

    void AnnounceLanguageName(const Language& lang) {
        if (!ready) return;
        queue.Announce([this, lang](const std::function<bool(const SpeechChunk&)>& onChunk) {
            RefreshVoices();
            PiperVoice* voice = VoiceFor(lang);
            if (!voice) {  // its own name can't be said: say why instead (VoiceFor has logged it)
                return Say(InterfaceVoice(), VoiceMissingText(lang), onChunk);
            }
            return Say(voice, lang.ownName, onChunk);
        });
        wake.notify_all();
    }

    VoiceFolders folders;
    std::map<std::string, std::unique_ptr<PiperVoice>> voices;  // by Language code; speech thread only
    WaveOutSink sink;
    ReadingQueue queue;
    SpeechKey speechKey{queue};  // UI thread only
    std::atomic<int> rate{0};
    std::atomic<int> volume{100};
    std::mutex controlsMutex;  // guards voiceControls
    std::map<std::string, VoiceControls> voiceControls;  // by Voice name
    std::atomic<bool> releaseRequested{false};  // ReleaseVoices asks the speech thread to drop every loaded Voice
    std::mutex releaseMutex;  // goes with releaseDone
    std::condition_variable releaseDone;
    std::mutex activeVoicesMutex;  // guards activeVoices
    std::map<std::string, std::string> activeVoices;  // Language code -> Voice name
    std::atomic<unsigned> voiceEpoch{0};  // +1 when activeVoices changes or the Voice files changed (VoicesChanged)
    unsigned seenVoiceEpoch = 0;  // speech thread only
    std::mutex interfaceMutex;  // guards interfaceLanguage
    std::string interfaceLanguage = strings::DefaultInterfaceLanguage();
    std::set<std::string> announcedMissing;  // codes of Languages whose missing Voice was announced; speech thread only
    std::set<std::string> interfaceFallbackLogged;  // Interface languages whose missing Voice was logged; speech thread only

    bool ready = false;  // Init succeeded
    std::mutex wakeMutex;  // guards quitting, and goes with wake
    std::condition_variable wake;  // wakes the speech thread early: a new Pick, or quitting
    bool quitting = false;
    std::thread worker;
};

Speaker::Speaker() : impl_(std::make_unique<Impl>()) {}
Speaker::~Speaker() = default;

bool Speaker::Init() { return impl_->Init(); }

void Speaker::Configure(int rate, int volume) {
    impl_->rate = rate;
    impl_->volume = volume;
}

void Speaker::SetVoiceControls(std::map<std::string, VoiceControls> controls) {
    std::lock_guard<std::mutex> lk(impl_->controlsMutex);
    impl_->voiceControls = std::move(controls);
}

void Speaker::SetActiveVoices(std::map<std::string, std::string> byLanguageCode) {
    {
        std::lock_guard<std::mutex> lk(impl_->activeVoicesMutex);
        if (impl_->activeVoices == byLanguageCode) return;
        impl_->activeVoices = std::move(byLanguageCode);
    }
    ++impl_->voiceEpoch;
}

// Same pattern as a changed choice of Voices: the speech thread owns `voices`, so only the epoch is bumped
// here and RefreshVoices (speech thread) clears the map at the start of the next Block.
void Speaker::VoicesChanged() { ++impl_->voiceEpoch; }

void Speaker::ReleaseVoices() {
    if (!impl_->ready) return;  // no speech thread, nothing loaded
    Stop();  // cuts off a Block that is being synthesised, so the thread comes back to its loop
    std::unique_lock<std::mutex> lk(impl_->releaseMutex);
    impl_->releaseRequested = true;
    impl_->wake.notify_all();
    if (!impl_->releaseDone.wait_for(lk, std::chrono::seconds(3), [this] { return !impl_->releaseRequested.load(); })) {
        LogWarn("[voice] The speech thread did not let go of the Voices in time.");
    }
}

uint64_t Speaker::SpeakReading(const std::vector<std::string>& blocks, bool append) {
    return impl_->Add(blocks, append);
}

std::optional<HeardWord> Speaker::HeardWordNow() const { return impl_->queue.HeardWordNow(); }

bool Speaker::HasReading(uint64_t id) const { return impl_->queue.HasReading(id); }

void Speaker::SetInterfaceLanguage(const std::string& code) {
    std::lock_guard<std::mutex> lk(impl_->interfaceMutex);
    impl_->interfaceLanguage = code;
}
void Speaker::Announce(const char* stringId, const strings::Args& args) {
    impl_->AnnounceText(strings::Format(impl_->InterfaceLanguage(), stringId, args));
}
void Speaker::AnnounceLanguage(const Language& lang) { impl_->AnnounceLanguageName(lang); }

void Speaker::Pause() { impl_->queue.Pause(); }
void Speaker::Resume() { impl_->queue.Resume(); }
void Speaker::Skip() { impl_->queue.Skip(); }
void Speaker::Stop() {
    impl_->speechKey.Cancel();  // a first tap still waiting must not skip what comes next
    impl_->queue.Stop();
}
void Speaker::StopReading(uint64_t id) { impl_->queue.StopReading(id); }
void Speaker::SessionStarted() {
    // A first tap still waiting has paused the queue for its window; that Pause is not the
    // person's, so its Skip is done now and the queue's rule then sees a playing queue.
    impl_->speechKey.Flush();
    impl_->queue.SessionStarted();
}
long long Speaker::TapSpeechKey(long long nowMs) { return impl_->speechKey.Tap(nowMs); }
long long Speaker::SettleSpeechKey(long long nowMs) { return impl_->speechKey.Settle(nowMs); }
void Speaker::SetActivityNotify(std::function<void()> notify) {
    if (!notify) {
        impl_->queue.SetActiveListener(nullptr);
        return;
    }
    impl_->queue.SetActiveListener([notify = std::move(notify)](bool) { notify(); });
}
bool Speaker::Active() const { return impl_->queue.Active(); }
bool Speaker::Paused() const { return impl_->queue.Paused(); }
