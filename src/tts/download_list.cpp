#include "tts/download_list.h"

#include <cstdint>

// The Voices of the Download sheet. ADD A VOICE HERE: one row in kListed, nothing else in the code.
// How to find the values and check them: docs/notes/voice-download-list.md, "Adding a Voice to the list".
namespace {

// Hugging Face rhasspy/piper-voices at one pinned commit (cf910fb, 2026-11-09), the same one as voices/voices.json's
// "source": the files of every Voice below are unchanged up to the repository's main branch at that time. With the
// trailing slash. A row that wants another commit has its own `baseUrl` (a 40-digit commit, never a branch).
constexpr const char* kPinnedCommitUrl =
    "https://huggingface.co/rhasspy/piper-voices/resolve/cf910fb59f1d79ab3b625a63993a108751c6c25d/";

// One row = one Voice. The file names follow Piper's layout:
//   <family>/<code>/<dataset>/<quality>/<code>-<dataset>-<quality>.onnx  (and the same with ".json" added)
// where <family> is the part of <code> before '_' ("en"). The sizes and MD5 digests are those of the pinned
// commit's voices.json; the SHA-256 of the .onnx (the Git LFS id) is kept in a comment for the record.
struct Listed {
    const char* code;          // Piper's language code, "en_US"
    const char* languageName;  // "English"
    const char* dataset;       // "kristin"
    const char* quality;       // "medium": x_low, low, medium or high
    const char* baseUrl;       // where the files are (kPinnedCommitUrl)
    uint64_t modelSize;
    const char* modelMd5;
    uint64_t configSize;
    const char* configMd5;
};

// The owner's order is the order of the sheet. All three are Medium, en_US, public domain (no label is shown).
const Listed kListed[] = {
    // en_US-kristin-medium.onnx  sha256 5849957f929cbf720c258f8458692d6103fff2f0e3d3b19c8259474bb06a18d4
    {"en_US", "English", "kristin", "medium", kPinnedCommitUrl,
     63531379, "5fed42d2296baca042e2bf74785db725", 4968, "70bc97d350c796c64ea5e4d08241afac"},
    // en_US-john-medium.onnx  sha256 789c6c875726e627ddee93d51d8727859abe9c091c3d141591f4b83c2072e988
    {"en_US", "English", "john", "medium", kPinnedCommitUrl,
     63531379, "70480857f21f2560f3a232722023b36d", 4965, "f2d04611b498e14d394385d1ec8a2d2d"},
    // en_US-ljspeech-medium.onnx  sha256 6f52a751e2349abe7a76735eb09dc1875298c77ea2342ffd2fef79ff81b87f22
    {"en_US", "English", "ljspeech", "medium", kPinnedCommitUrl,
     63531379, "109d552e9dd78d92d1169a7edd6de38d", 4972, "0668112b8b3ac5bb4b12c2b1a366776a"},
};

// A row as the CatalogueVoice the rest of the code uses: the name and the two repository paths are made from
// the row's parts.
CatalogueVoice MakeVoice(const Listed& row) {
    CatalogueVoice v;
    const std::string code = row.code;
    const std::string family = code.substr(0, code.find('_'));  // "en"
    v.name = code + "-" + row.dataset + "-" + row.quality;
    v.languageCode = code;
    v.languageName = row.languageName;
    v.quality = row.quality;
    v.baseUrl = row.baseUrl;
    const std::string folder = family + "/" + code + "/" + row.dataset + "/" + row.quality + "/" + v.name;
    v.model = {folder + ".onnx", row.modelSize, row.modelMd5};
    v.config = {folder + ".onnx.json", row.configSize, row.configMd5};
    return v;
}

}  // namespace

const std::vector<CatalogueVoice>& DownloadList() {
    // Built on first use (a function-local static: thread-safe, and nothing runs at startup).
    static const std::vector<CatalogueVoice> list = [] {
        std::vector<CatalogueVoice> out;
        for (const Listed& row : kListed) out.push_back(MakeVoice(row));
        return out;
    }();
    return list;
}
