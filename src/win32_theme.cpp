#include "win32_theme.h"

#include <windows.h>

// Windows' "app mode" setting (Settings > Personalisation > Colours), which the Settings window and
// the Control bar follow. 0 = dark, 1 = light. A missing value (older Windows) counts as light.
bool SystemUsesLightTheme() {
    DWORD value = 1;
    DWORD size = sizeof(value);
    const LSTATUS rc =
        RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &size);
    return rc != ERROR_SUCCESS || value != 0;
}
