#include "platform/pointer_monitor.h"

#include <cstddef>
#include <iterator>

namespace dimmer {
namespace {

struct DeviceUsage {
    USHORT page;
    USHORT usage;
};

constexpr DeviceUsage kUsages[] = {
    {0x01, 0x02},  // Mouse
    {0x0D, 0x05},  // Precision touchpad
    {0x0D, 0x04},  // Touch screen
    {0x0D, 0x02},  // Pen
};

bool RegisterUsage(const DeviceUsage& usage, DWORD flags, HWND target) {
    RAWINPUTDEVICE device{};
    device.usUsagePage = usage.page;
    device.usUsage = usage.usage;
    device.dwFlags = flags;
    device.hwndTarget = target;
    return RegisterRawInputDevices(&device, 1, sizeof(device)) != FALSE;
}

}  // namespace

bool PointerMonitor::Register(HWND target) {
    static_assert(std::size(kUsages) == std::tuple_size_v<decltype(registered_)>);
    for (std::size_t i = 0; i < std::size(kUsages); ++i) {
        registered_[i] = RegisterUsage(kUsages[i], RIDEV_INPUTSINK, target);
    }
    return registered_[0];
}

void PointerMonitor::Unregister() {
    for (std::size_t i = 0; i < std::size(kUsages); ++i) {
        if (registered_[i]) {
            RegisterUsage(kUsages[i], RIDEV_REMOVE, nullptr);
            registered_[i] = false;
        }
    }
}

}  // namespace dimmer
