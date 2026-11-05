#pragma once
// Text to speech through Piper Voices (src/tts), played by Lexia itself. A Reading queue
// (tts/reading_queue.h) holds what is to be said.
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "language.h"
#include "strings/strings.h"
#include "tts/voice_controls.h"

class Speaker {
public:
    Speaker();
    ~Speaker();
    Speaker(const Speaker&) = delete;
    Speaker& operator=(const Speaker&) = delete;

    // Finds voices/ and espeak-ng-data/ next to the exe and starts the speech thread. False
    // (with the reason logged) if they are missing. Voices are loaded when first needed.
    bool Init();

    // rate: -10 (slow) .. 10 (fast); volume: 0 .. 100 (the master volume). These apply to every Voice on
    // top of its own controls (they are multiplied into the Voice's Speed and Volume).
    void Configure(int rate, int volume);

    // The controls of each Voice by its name (Settings::voices): the Voice tab's sliders. The next
    // Block is spoken with the controls of the Voice that speaks it (a Voice with no entry: the
    // defaults). Callable from any thread.
    void SetVoiceControls(std::map<std::string, VoiceControls> controls);

    // The Voice that speaks each Language, by Language code ("en") -> Voice name (Settings::activeVoices,
    // Voice tab). A Language with no entry, or whose Voice is not installed, uses the default one. When
    // it changes, every loaded Voice is dropped and loaded again at the next Block (libpiper shuts
    // espeak-ng down when any one Voice is freed). Callable from any thread.
    void SetActiveVoices(std::map<std::string, std::string> byLanguageCode);

    // Stops what is being said and waits (up to 3 s) until the speech thread has dropped every loaded
    // Voice, so that a Voice's files can be deleted: a loaded Voice keeps its model file open. Voices
    // load again at the next Block. UI thread.
    void ReleaseVoices();

    // Speaks a Reading: its Blocks one after another, in order. Each Block is spoken as its stretches
    // of one script, each with the Voice of the Language that script matches (see stretch.h).
    // append = true queues the Reading after whatever is already being spoken.
    void SpeakReading(const std::vector<std::string>& blocks, bool append = false);

    // Announcements (rules in tts/reading_queue.h: they hold the Reading at a word boundary, the
    // newest replaces any other). Spoken in the Interface language, by the Voice of the Language
    // with that code; if that Voice is missing, by any Voice that is there (logged). A Voice that
    // Lexia needs and does not have is announced here, once per Language.
    void SetInterfaceLanguage(const std::string& code);
    // Says the text `stringId` has in the Interface language (strings/string_ids.h).
    void Announce(const char* stringId, const strings::Args& args = {});
    // A Language was switched to: says its own name (Language::ownName) in its own Voice. If that
    // Voice is missing it announces that instead.
    void AnnounceLanguage(const Language& lang);

    // Reading queue controls (rules in tts/reading_queue.h). Like the Speech key, they are for
    // the UI thread. SessionStarted first does the Skip of a Speech key tap still in its window
    // (SpeechKey::Flush), so the tap's own Pause does not make the queue look paused and empty it.
    void Pause();
    void Resume();
    void Skip();
    void Stop();
    void SessionStarted();

    // The Speech key (tts/speech_key.h): a tap is Skip, a second tap in time is Stop. `nowMs` is a
    // steady clock in milliseconds. Both return how long until SettleSpeechKey is due, or 0.
    long long TapSpeechKey(long long nowMs);
    long long SettleSpeechKey(long long nowMs);

    // Called whenever Active() changes, from the speech thread or the UI thread: it must only
    // post a message or the like (see ReadingQueue::SetActiveListener). nullptr removes it.
    void SetActivityNotify(std::function<void()> notify);
    bool Active() const;  // something is being spoken or paused
    bool Paused() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
