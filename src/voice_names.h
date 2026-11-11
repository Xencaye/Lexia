#pragma once
// How a Voice is named on screen (owner, Nov 11): its short name and its quality, "LJ Speech (Medium)",
// instead of the file name "en_US-ljspeech-medium". The file name stays the Voice's identity everywhere
// else (settings.json, Presets, messages to the App) and is shown as the tooltip, so two Voices with the
// same short name can still be told apart. Pure (strings/ only), tested by voice_names_test.cpp.
#include <string>

// The short name of a Voice. A Piper name "<locale>-<id>-<quality>" ("en_US-ljspeech-medium") gives its
// <id> part (everything between the first and the last '-'), written as a person would read it: a few
// datasets have a fixed spelling ("LJ Speech", "Jenny (Dioco)", "LibriTTS-R"), any other id is split at
// '_' and each word gets a capital ("northern_english_male" -> "Northern English Male"). A name not in that
// form (a Voice the user named "myvoice") is shown as it is. Never empty for a non-empty name.
std::string VoiceShortName(const std::string& voiceName);

// The word for a Piper quality in the Interface language ("medium" -> "Medium", "x_low" -> "X-low");
// "" for an empty or unknown quality.
std::string VoiceQualityWord(const std::string& interfaceLanguage, const std::string& quality);

// What a list shows for a Voice: "<short name> (<quality word>)", or the short name alone when the quality
// is not stated or not known. `quality` is lower case ("medium"); pass the config's quality when it is known
// (InstalledVoice::quality), else VoiceQualityFromName(voiceName).
std::string VoiceLabel(const std::string& interfaceLanguage, const std::string& voiceName, const std::string& quality);

// VoiceLabel with the quality read from the name (VoiceQualityFromName): for messages and the catalogue,
// where only the name is at hand.
std::string VoiceLabelFromName(const std::string& interfaceLanguage, const std::string& voiceName);
