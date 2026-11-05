#include "tts/stretch_speech.h"

#include <vector>

#include "common.h"
#include "stretch.h"

// Speaks one Block as its stretches of one script (E5), each with the Voice of its Language. A stretch
// whose Language has no Voice is spoken with the Block's main Voice: the Voice of its longest stretch.
// To the Reading queue this is still one Block: the chunks of all stretches go to onChunk in order,
// and only the last chunk of the last stretch has isLast set.
// Result: Cancelled as soon as onChunk asks to stop; Done if any stretch was spoken; else Failed (also
// when the main Language has no Voice, as then there is nothing to fall back to).
SynthResult SpeakStretches(const std::string& text, const VoiceAvailable& available,
                           const VoiceSynthesizer& synthesize,
                           const std::function<bool(const SpeechChunk&)>& onChunk) {
    struct Piece {
        size_t offset;            // the byte offset of the stretch in `text`
        const Stretch* stretch;
    };
    const std::vector<Stretch> stretches = SplitIntoStretches(text);
    std::vector<Piece> pieces;  // the stretches with something to say (not only spaces)
    const Language* main = nullptr;  // the Language of the longest stretch
    size_t mainSize = 0;
    size_t offset = 0;
    for (const Stretch& s : stretches) {
        if (!Trim(s.text).empty()) {
            pieces.push_back({offset, &s});
            if (!main || s.text.size() > mainSize) {
                main = s.language;
                mainSize = s.text.size();
            }
        }
        offset += s.text.size();
    }
    if (pieces.empty() || !available(*main)) return SynthResult::Failed;

    bool spokeAny = false;
    for (size_t n = 0; n < pieces.size(); ++n) {
        const Piece& p = pieces[n];
        const bool lastPiece = n + 1 == pieces.size();
        const Language& voice = available(*p.stretch->language) ? *p.stretch->language : *main;
        // Mark each chunk with where its stretch starts in the Block, and keep isLast for the very end.
        const SynthResult result = synthesize(voice, p.stretch->text, [&](const SpeechChunk& chunk) {
            SpeechChunk out = chunk;
            out.textOffset = p.offset;
            out.isLast = chunk.isLast && lastPiece;
            return onChunk(out);
        });
        if (result == SynthResult::Cancelled) return SynthResult::Cancelled;
        if (result == SynthResult::Done) spokeAny = true;  // a Failed stretch is skipped; the rest go on
    }
    return spokeAny ? SynthResult::Done : SynthResult::Failed;
}
