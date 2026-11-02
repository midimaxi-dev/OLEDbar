#pragma once

#include <windows.h>

#include <array>

namespace dimmer {

// Raw input sink for mouse, precision touchpad, touch screen and pen.
// WM_INPUT reaches the target only while one of these devices is in use.
class PointerMonitor {
public:
    // Returns true if at least the mouse was registered.
    bool Register(HWND target);
    void Unregister();

private:
    std::array<bool, 4> registered_{};
};

}  // namespace dimmer
