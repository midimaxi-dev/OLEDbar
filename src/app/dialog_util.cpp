#include "app/dialog_util.h"

#include <commctrl.h>

#include "app/resource.h"

namespace dimmer {

void SetAppDialogIcons(HWND hwnd, wil::unique_hicon& smallIcon, wil::unique_hicon& largeIcon) {
    HICON icon = nullptr;
    const HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hwnd, GWLP_HINSTANCE));
    if (SUCCEEDED(LoadIconMetric(instance, MAKEINTRESOURCEW(IDI_APP), LIM_SMALL, &icon))) {
        smallIcon.reset(icon);
    }
    if (SUCCEEDED(LoadIconMetric(instance, MAKEINTRESOURCEW(IDI_APP), LIM_LARGE, &icon))) {
        largeIcon.reset(icon);
    }
    SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(smallIcon.get()));
    SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(largeIcon.get()));
}

}  // namespace dimmer
