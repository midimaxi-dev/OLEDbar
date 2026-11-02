#include "platform/overlay_window.h"

#include <algorithm>
#include <cmath>

#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif

namespace dimmer {
namespace {

constexpr wchar_t kClassName[] = L"OLEDbar.Overlay";
constexpr DWORD kExStyle = WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;

int Width(const RECT& rect) noexcept {
    return rect.right - rect.left;
}

int Height(const RECT& rect) noexcept {
    return rect.bottom - rect.top;
}

}  // namespace

bool OverlayWindow::Create(HINSTANCE instance) {
    WNDCLASSEXW windowClass{sizeof(windowClass)};
    windowClass.lpfnWndProc = &OverlayWindow::WndProc;
    windowClass.hInstance = instance;
    windowClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    windowClass.lpszClassName = kClassName;
    if (!RegisterClassExW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    hwnd_.reset(CreateWindowExW(kExStyle, kClassName, L"OLEDbar Overlay", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr,
                                instance, nullptr));
    if (!hwnd_) {
        return false;
    }
    return SetLayeredWindowAttributes(hwnd_.get(), 0, 0, LWA_ALPHA) != FALSE;
}

void OverlayWindow::SetBounds(const RECT& bounds) {
    if (EqualRect(&bounds, &bounds_)) {
        return;
    }
    bounds_ = bounds;
    if (hwnd_ && shown_) {
        SetWindowPos(hwnd_.get(), nullptr, bounds_.left, bounds_.top, Width(bounds_), Height(bounds_),
                     SWP_NOACTIVATE | SWP_NOZORDER);
    }
}

void OverlayWindow::SetOpacity(double opacity) {
    if (!hwnd_) {
        return;
    }
    const auto alpha = static_cast<BYTE>(std::lround(std::clamp(opacity, 0.0, 1.0) * 255.0));
    if (alpha == 0) {
        if (shown_) {
            ShowWindow(hwnd_.get(), SW_HIDE);
            shown_ = false;
        }
        alpha_ = 0;
        return;
    }
    if (alpha != alpha_) {
        SetLayeredWindowAttributes(hwnd_.get(), 0, alpha, LWA_ALPHA);
        alpha_ = alpha;
    }
    if (!shown_) {
        SetWindowPos(hwnd_.get(), HWND_TOPMOST, bounds_.left, bounds_.top, Width(bounds_), Height(bounds_),
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
        shown_ = true;
    }
}

bool OverlayWindow::EnsureAbove(HWND other) {
    if (!hwnd_ || !shown_ || !other) {
        return false;
    }
    for (HWND above = GetWindow(hwnd_.get(), GW_HWNDPREV); above; above = GetWindow(above, GW_HWNDPREV)) {
        if (above == other) {
            SetWindowPos(hwnd_.get(), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            return true;
        }
    }
    return false;
}

bool OverlayWindow::SetExcludeFromCapture(bool exclude) {
    return hwnd_ &&
           SetWindowDisplayAffinity(hwnd_.get(), exclude ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE) != FALSE;
}

LRESULT CALLBACK OverlayWindow::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCHITTEST) {
        return HTTRANSPARENT;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

}  // namespace dimmer
