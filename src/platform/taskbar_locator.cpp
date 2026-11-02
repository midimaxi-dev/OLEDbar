#include "platform/taskbar_locator.h"

#include <shellapi.h>

#include <algorithm>

namespace dimmer {
namespace {

// An auto-hidden taskbar leaves a sliver of ~2 px on screen; treat that as hidden.
constexpr LONG kMinVisibleThickness = 4;

bool SameInfo(const TaskbarInfo& a, const TaskbarInfo& b) noexcept {
    return a.hwnd == b.hwnd && a.processId == b.processId && EqualRect(&a.windowRect, &b.windowRect) &&
           EqualRect(&a.visibleRect, &b.visibleRect) && EqualRect(&a.monitorRect, &b.monitorRect) &&
           a.autoHide == b.autoHide && a.visible == b.visible;
}

}  // namespace

bool TaskbarLocator::Refresh() {
    TaskbarInfo next;
    next.hwnd = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (next.hwnd) {
        GetWindowThreadProcessId(next.hwnd, &next.processId);
        GetWindowRect(next.hwnd, &next.windowRect);

        MONITORINFO monitor{sizeof(monitor)};
        if (GetMonitorInfoW(MonitorFromWindow(next.hwnd, MONITOR_DEFAULTTOPRIMARY), &monitor)) {
            next.monitorRect = monitor.rcMonitor;
        }
        IntersectRect(&next.visibleRect, &next.windowRect, &next.monitorRect);

        APPBARDATA appBar{sizeof(appBar)};
        next.autoHide = (SHAppBarMessage(ABM_GETSTATE, &appBar) & ABS_AUTOHIDE) != 0;

        const LONG thickness = std::min(next.visibleRect.right - next.visibleRect.left,
                                        next.visibleRect.bottom - next.visibleRect.top);
        next.visible = IsWindowVisible(next.hwnd) && thickness > kMinVisibleThickness;
    }

    const bool changed = !SameInfo(info_, next);
    info_ = next;
    return changed;
}

}  // namespace dimmer
