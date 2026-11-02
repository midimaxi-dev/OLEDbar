#pragma once

#include <windows.h>

namespace dimmer {

struct TaskbarInfo {
    HWND hwnd = nullptr;
    DWORD processId = 0;
    RECT windowRect{};
    RECT visibleRect{};  // windowRect clipped to its monitor.
    RECT monitorRect{};
    bool autoHide = false;
    bool visible = false;
};

// Locates the primary taskbar (Shell_TrayWnd).
class TaskbarLocator {
public:
    // Returns true if anything changed since the previous call.
    bool Refresh();
    const TaskbarInfo& Info() const noexcept { return info_; }

private:
    TaskbarInfo info_;
};

}  // namespace dimmer
