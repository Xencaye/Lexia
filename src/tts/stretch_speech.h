#pragma once
// Speaks one Block as its stretches (stretch.h), each with the Voice of its Language, as a single
// unit for the Reading queue: the queue sees one Block, so Skip skips all of it and Stop cuts it
// off wherever it is. Pure and Win32-free; the Speaker supplies the real Voices, the tests fakes.
#include <functional>
#include <string>

#include "language.h"
#include "tts/reading_queue.h"
#include "tts/speech_chunk.h"

// Is there a Voice for this Language (installed and loadable)?
using VoiceAvailable = std::function<bool(const Language&)>;

// Speaks `text` with the Voice of `language`; `onChunk` as in BlockSynthesizer. Only called for
// a Language VoiceAvailable said yes to.
using VoiceSynthesizer = std::function<SynthResult(
    const Language& language, const std::string& text, const std::function<bool(const SpeechChunk&)>& onChunk)>;

// Speaks the stretches of `text` in order. A stretch whose Language has no Voice is spoken by
// the Block's main Voice, that of the longest stretch (the first, if equal). If that has no Voice
// either, nothing is spoken and the result is Failed.
//
// The chunks reach `onChunk` as the Voice made them, with two changes: `textOffset` is the byte
// offset of the chunk's stretch in `text`, and `isLast` is true only for the last chunk of the
// Block's last stretch. A chunk lies inside one stretch, and its phoneme timings stay relative to
// the start of its own audio, as always. A stretch that failed is skipped (the others still
// speak); a cut-off (`onChunk` false) ends the Block at once. The result is Cancelled for a
// cut-off, Done if at least one stretch was spoken, else Failed.
SynthResult SpeakStretches(const std::string& text, const VoiceAvailable& available,
                           const VoiceSynthesizer& synthesize,
                           const std::function<bool(const SpeechChunk&)>& onChunk);
