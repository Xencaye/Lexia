#pragma once
// A sample UiState for lexia_settingsshot (dev tool, not packaged): the values of the old ui/dev/mock_host.js,
// so the native pages can be compared with the HTML reference shots in wireframe/html-reference/.
#include "ui_host.h"

namespace shot {

// `on`: Lexia is On (Home shows the enabled look); `dark`: the theme in use.
UiState SampleState(bool on, bool dark);

}  // namespace shot
