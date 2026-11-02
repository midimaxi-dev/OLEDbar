#pragma once

#include <windows.h>

#include <wil/resource.h>

namespace dimmer {

// Black, layered, click-through, topmost window placed over the taskbar.
class OverlayWindow {
public:
    bool Create(HINSTANCE instance);
    void SetBounds(const RECT& bounds);
    // 0 = invisible (window hidden), 1 = opaque black.
    void SetOpacity(double opacity);
    // Re-raises the overlay if `other` is above it in z-order. Returns true if it re-raised.
    bool EnsureAbove(HWND other);
    bool SetExcludeFromCapture(bool exclude);
    bool IsShown() const noexcept { return shown_; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    wil::unique_hwnd hwnd_;
    RECT bounds_{};
    BYTE alpha_ = 0;
    bool shown_ = false;
};

}  // namespace dimmer
