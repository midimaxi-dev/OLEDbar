#include "platform/desktop_icons.h"

#include <windows.h>

namespace dimmer {
namespace {

// Explorer's own "Show desktop icons" menu command. Undocumented, but unchanged from Windows XP to 11.
constexpr WPARAM kToggleDesktopIconsCommand = 0x7402;

HWND FindViewIn(HWND parent) {
    return FindWindowExW(parent, nullptr, L"SHELLDLL_DefView", nullptr);
}

// The desktop view normally lives under Progman. After a wallpaper slideshow or certain animations, Explorer moves
// it into a WorkerW window: a top-level one on older builds, or a child of Progman on Windows 11 24H2 and later.
HWND FindDesktopView() {
    const HWND progman = FindWindowW(L"Progman", nullptr);
    if (!progman) {
        return nullptr;
    }
    if (const HWND view = FindViewIn(progman)) {
        return view;
    }
    const HWND parents[] = {progman, nullptr};
    for (const HWND parent : parents) {
        HWND worker = nullptr;
        while ((worker = FindWindowExW(parent, worker, L"WorkerW", nullptr)) != nullptr) {
            if (const HWND view = FindViewIn(worker)) {
                return view;
            }
        }
    }
    return nullptr;
}

}  // namespace

bool ToggleDesktopIcons() {
    const HWND view = FindDesktopView();
    if (!view) {
        SetLastError(ERROR_NOT_FOUND);
        return false;
    }
    // Posted, not sent, so a busy Explorer can't stall our message loop.
    return PostMessageW(view, WM_COMMAND, kToggleDesktopIconsCommand, 0) != FALSE;
}

}  // namespace dimmer
