#pragma once
// A sample UiState for lexia_settingsshot (dev tool, not packaged): the values of the old ui/dev/mock_host.js,
// so the native pages can be compared with the HTML reference shots in wireframe/html-reference/.
#include <string>
#include <vector>

#include "ui_host.h"
#include "voice_events.h"

namespace shot {

// `on`: Lexia is On (Home shows the enabled look); `dark`: the theme in use. `voices` is the Voices page's variant
// (the mock host's ?voices= and ?active=): "" = two Voices installed, the shipped one active; "none" = no Voice;
// "changed" = the shipped Voice with its pitch moved off the default (a reset button shows); "user" = the Voice the
// person added is active (Delete is on).
UiState SampleState(bool on, bool dark, const std::string& voices = std::string());

// The downloadable Voices of the mock host (six Voices of 63 MB; the two installed ones marked installed).
std::vector<CatalogueEntry> SampleCatalogue();

}  // namespace shot
