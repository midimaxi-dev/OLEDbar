#include "platform/shell_event_monitor.h"

#include <utility>

namespace dimmer {

ShellEventMonitor* ShellEventMonitor::s_instance = nullptr;

ShellEventMonitor::~ShellEventMonitor() {
    Stop();
}

bool ShellEventMonitor::Start(Callbacks callbacks) {
    callbacks_ = std::move(callbacks);
    s_instance = this;
    foregroundHook_.reset(SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr,
                                          &ShellEventMonitor::OnEvent, 0, 0,
                                          WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS));
    return static_cast<bool>(foregroundHook_);
}

void ShellEventMonitor::WatchTaskbar(HWND taskbar, DWORD processId) {
    taskbar_ = taskbar;
    if (processId == watchedProcessId_ && (locationHook_ || processId == 0)) {
        return;
    }
    watchedProcessId_ = processId;
    locationHook_.reset();
    if (processId != 0) {
        locationHook_.reset(SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE, nullptr,
                                            &ShellEventMonitor::OnEvent, processId, 0, WINEVENT_OUTOFCONTEXT));
    }
}

void ShellEventMonitor::Stop() {
    foregroundHook_.reset();
    locationHook_.reset();
    watchedProcessId_ = 0;
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

void CALLBACK ShellEventMonitor::OnEvent(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG objectId, LONG childId, DWORD,
                                         DWORD) {
    ShellEventMonitor* self = s_instance;
    if (!self) {
        return;
    }
    if (event == EVENT_SYSTEM_FOREGROUND) {
        if (self->callbacks_.foregroundChanged) {
            self->callbacks_.foregroundChanged(hwnd);
        }
    } else if (event == EVENT_OBJECT_LOCATIONCHANGE && hwnd == self->taskbar_ && objectId == OBJID_WINDOW &&
               childId == CHILDID_SELF) {
        if (self->callbacks_.taskbarMoved) {
            self->callbacks_.taskbarMoved();
        }
    }
}

}  // namespace dimmer
