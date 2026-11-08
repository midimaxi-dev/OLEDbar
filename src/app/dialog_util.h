#pragma once

#include <windows.h>

#include <wil/resource.h>

namespace dimmer {

void SetAppDialogIcons(HWND hwnd, wil::unique_hicon& smallIcon, wil::unique_hicon& largeIcon);

}  // namespace dimmer
