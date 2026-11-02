#include <windows.h>

#include <wil/resource.h>

#include "app/app_host.h"
#include "platform/log.h"
#include "version.h"

namespace {

constexpr wchar_t kSingleInstanceMutex[] = L"Local\\OLEDbar-2f6b8c1e-5a7d-4c39-9e0b-7d1f3a4c8e52";

}  // namespace

int WINAPI wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ PWSTR, _In_ int) {
    wil::unique_handle mutex(CreateMutexW(nullptr, FALSE, kSingleInstanceMutex));
    if (!mutex || GetLastError() == ERROR_ALREADY_EXISTS) {
        // Already running: ask that instance to open its settings panel.
        if (const HWND running = FindWindowW(dimmer::AppHost::kWindowClassName, nullptr)) {
            AllowSetForegroundWindow(ASFW_ANY);
            PostMessageW(running, RegisterWindowMessageW(dimmer::AppHost::kShowSettingsMessageName), 0, 0);
        }
        return 0;
    }

    dimmer::log::Init();
    dimmer::log::Info(L"OLEDbar {} starting", OLEDBAR_VERSION_WSTRING);
    int exitCode = 0;
    {
        dimmer::AppHost app;
        exitCode = app.Run(instance);
    }
    dimmer::log::Shutdown();
    return exitCode;
}
