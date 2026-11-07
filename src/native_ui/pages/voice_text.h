#pragma once
// The words and numbers of the Voices page, as the old ui/js/voice.js and voice_library.js formatted them
// (K.3). Pure and Win32-free, tested. Every text comes from strings/ (the number is put into the {value} of a
// string), never from a literal here.
#include <cstdint>
#include <string>

namespace nui {

// The value of a Voice control as the person reads it: Speed "1.0×" (1.0 is the Voice's own pace), Pitch in
// semitones with a real minus sign ("−1.5 semitones", "+1 semitone": the singular for exactly one), Volume as
// a percentage of the Voice's own. `name` is "speed" | "pitch" | "volume".
std::string VoiceControlText(const std::string& interfaceLanguage, const std::string& name, double value);

// "63 MB": megabytes (1024 x 1024 bytes), one decimal under 10 MB, else whole.
std::string DownloadSizeText(const std::string& interfaceLanguage, uint64_t bytes);

// "12 of 63 MB": the bytes done never read more than the total, and are rounded down (a Voice that has 99.9 MB
// of 100 does not say 100).
std::string DownloadProgressText(const std::string& interfaceLanguage, uint64_t done, uint64_t total);

}  // namespace nui
