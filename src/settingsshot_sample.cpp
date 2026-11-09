#include "settingsshot_sample.h"

#include "version.h"  // LEXIA_VERSION, generated from vcpkg.json

// The values are test data of a developer tool: they stay literals, outside strings/.
namespace shot {

namespace {
constexpr const char* kShipped = "en_GB-cori-medium";  // comes with Lexia
constexpr const char* kUser = "en_GB-alba-medium";         // one a person added: it can be deleted
constexpr uint64_t kMiB = 1024 * 1024;
}  // namespace

UiState SampleState(bool on, bool dark, const std::string& voices) {
    UiState s;
    s.on = on;
    s.language = "eng";
    s.theme = dark ? "dark" : "light";
    s.dark = dark;
    s.version = LEXIA_VERSION;  // the real version, so the About shot shows what the release says
    s.runAtStartup = false;
    s.startMinimised = true;
    s.runAsAdmin = false;
    s.elevated = false;
    s.leftHanded = false;
    s.readAlong = false;
    s.masterVolume = {50, 0, 100, 100, 5};  // the thumb at the middle of the track, as in the reference shots
    s.keys = {{"read", "F8"}, {"pause", "F7"}, {"speech", "F6"}};

    // The Voices page: the mock host's ranges (voice_controls.h RangeOf) and the steps of the sliders.
    UiVoices v;
    if (voices != "none") {
        v.installed = {kShipped, kUser};
        v.active = voices == "user" ? kUser : kShipped;
        v.removable = {kUser};
        const double pitch = voices == "changed" ? 3.0 : 0.0;
        s.controls["speed"] = {1.0, 0.5, 2.0, 1.0, 0.05};
        s.controls["pitch"] = {pitch, -6, 6, 0, 0.5};
        s.controls["volume"] = {1.0, 0, 2.0, 1.0, 0.05};
    }
    s.voices["eng"] = v;
    return s;
}

std::vector<CatalogueEntry> SampleCatalogue() {
    return {{"en_GB-alba-medium", "eng", 63 * kMiB, true},
            {"en_GB-jenny_dioco-medium", "eng", 63 * kMiB, false},
            {"en_GB-northern_english_male-medium", "eng", 63 * kMiB, false},
            {"en_US-amy-medium", "eng", 63 * kMiB, false},
            {"en_US-lessac-medium", "eng", 63 * kMiB, false},
            {"en_GB-cori-medium", "eng", 63 * kMiB, true}};
}

}  // namespace shot
