#include "settingsshot_sample.h"

// The values are test data of a developer tool: they stay literals, outside strings/.
namespace shot {

UiState SampleState(bool on, bool dark) {
    UiState s;
    s.on = on;
    s.language = "eng";
    s.theme = dark ? "dark" : "light";
    s.dark = dark;
    s.version = "0.5.0";
    s.runAtStartup = false;
    s.startMinimised = true;
    s.runAsAdmin = false;
    s.elevated = false;
    s.leftHanded = false;
    s.masterVolume = {50, 0, 100, 100, 5};  // the thumb at the middle of the track, as in the reference shots
    s.keys = {{"read", "F8"}, {"pause", "F7"}, {"speech", "F6"}};
    return s;
}

}  // namespace shot
