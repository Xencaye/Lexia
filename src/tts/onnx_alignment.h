#pragma once
// Phoneme timings for the Voices that have none. libpiper (Piper's C library) gives the audio of a sentence
// with the length of each phoneme only if the model has a second output, the `w_ceil` tensor (piper's
// patch_voice_with_alignment.py exposes it). The files of rhasspy/piper-voices do not have it, and without it the
// Reading queue cannot cut words: no read-along mark, and an Announcement waits for the end of the sentence.
//
// So Lexia adds that one output to a copy of the model when it loads the Voice (PiperVoice::Load). The weights
// and every other byte are unchanged: one `output` entry is appended to the graph in the ONNX file, which is a
// protobuf. The copy lives in `%LOCALAPPDATA%\Lexia\aligned` and never leaves the PC. docs/third-party.md says so.
//
// Pure and Win32-free (std::filesystem only), so the tests drive it with small hand-built models.
//
// SECURITY. A model is untrusted input (a Voice the user added or downloaded). Every protobuf read is bounds
// checked (a length that runs past its parent ends the read), varints are at most 10 bytes, the walk is
// iterative with a fixed depth (model, graph, node), and a model over `kMaxAlignedModelBytes` is not read.
// Anything odd is a refusal with a reason, never undefined behaviour. The cache files are written next to each
// other in one folder of Lexia's own, through a temporary name and a rename, and the code never writes through a
// symbolic link or junction.
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

// The largest model that is read to be patched (the biggest Piper model, High quality, is about 115 MB).
constexpr std::uintmax_t kMaxAlignedModelBytes = 512ull * 1024 * 1024;

// Does the model already have more than one graph output (so libpiper gives alignments)? False for a model
// that is not a readable ONNX file.
bool HasAlignmentOutput(const std::string& onnxBytes);

// The model with its one Ceil tensor added as a graph output. `onnxBytes` itself, unchanged, when it has an
// alignment output already. An empty string and a reason in `*why` (if given) when the bytes are not a
// well-formed ONNX model, or the graph has no Ceil node or more than one Ceil tensor. Never throws.
std::string AddAlignmentOutput(const std::string& onnxBytes, std::string* why);

// `dataDir` / "aligned" (dataDir is `%LOCALAPPDATA%\Lexia`); empty when `dataDir` is empty (no cache).
std::filesystem::path AlignedDirIn(const std::filesystem::path& dataDir);

// What EnsureAlignedCopy found or made.
struct AlignedCopy {
    std::filesystem::path model;  // the model to load; empty = load the original
    bool created = false;         // the copy was made by this call (it took a while)
    bool alreadyAligned = false;  // the original has the output: nothing to do, load it
    std::string why;              // when `model` is empty and the original is not aligned: the reason (no user text)
};

// The aligned copy of `model` in `alignedDir`, made if it is not there. The copy is
// `<name>.<size>-<last write time>.onnx` with `<name>.<same>.onnx.json` beside it (libpiper reads the config
// from "<model>.json"), where `<name>` is the model's file name without ".onnx": a changed or replaced Voice file
// has another key, so it never uses an old copy, and the copies of its earlier keys are deleted. Both files are
// written under a `.part` name and renamed, the config first, so a copy with a model file is complete. Not made
// (empty `model`, with `why`) when `alignedDir` is empty, the model is bigger than `maxBytes` or unreadable, is
// not patchable, the folder or a file in it is a link, or a write fails. Speech thread: reading and writing the
// copy of a 60 MB Voice takes a moment (the first load of each Voice only).
AlignedCopy EnsureAlignedCopy(const std::filesystem::path& model, const std::filesystem::path& alignedDir,
                              std::uintmax_t maxBytes = kMaxAlignedModelBytes);

// Deletes every aligned copy (any key, and a leftover `.part` file) of the Voice `voiceName` in `alignedDir`,
// and nothing else. Best effort: a file in use stays. Call it after the Speaker has let go of the Voice.
void RemoveAlignedCopies(const std::filesystem::path& alignedDir, std::string_view voiceName);
