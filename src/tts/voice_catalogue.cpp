#include "tts/voice_catalogue.h"

// Can Lexia use this Voice? The same rules as for a Voice from a file (name, Language Lexia reads, a known
// quality), so a listed Voice is refused before any download if its entry is wrong.
VoiceOutcome CheckCatalogueVoice(const CatalogueVoice& voice) {
    return CheckVoiceAccepted(voice.name, voice.languageCode, voice.languageName, voice.quality);
}
