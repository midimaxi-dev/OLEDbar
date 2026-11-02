#pragma once

#include <windows.h>

#include <functional>

#include "platform/win_util.h"

namespace dimmer {

// Out-of-context WinEvent hooks: foreground changes (all processes) and taskbar moves (explorer only).
// Only one instance may be started at a time.
class ShellEventMonitor {
public:
    struct Callbacks {
        std::function<void(HWND)> foregroundChanged;
        std::function<void()> taskbarMoved;
    };

    ShellEventMonitor() = default;
    ~ShellEventMonitor();
    ShellEventMonitor(const ShellEventMonitor&) = delete;
    ShellEventMonitor& operator=(const ShellEventMonitor&) = delete;

    bool Start(Callbacks callbacks);
    void WatchTaskbar(HWND taskbar, DWORD processId);
    void Stop();

private:
    static void CALLBACK OnEvent(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG objectId, LONG childId,
                                 DWORD threadId, DWORD timeMs);

    static ShellEventMonitor* s_instance;

    Callbacks callbacks_;
    unique_wineventhook foregroundHook_;
    unique_wineventhook locationHook_;
    HWND taskbar_ = nullptr;
    DWORD watchedProcessId_ = 0;
};

}  // namespace dimmer
