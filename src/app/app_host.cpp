#include "app/app_host.h"

#include <shellapi.h>
#include <windowsx.h>
#include <wtsapi32.h>
#include <commctrl.h>

#include <algorithm>
#include <chrono>
#include <format>
#include <iterator>
#include <string>
#include <string_view>

#include "platform/desktop_icons.h"
#include "platform/log.h"
#include "platform/startup.h"
#include "platform/win_util.h"
#include "version.h"

namespace dimmer {
namespace {

constexpr const wchar_t* kWindowClass = AppHost::kWindowClassName;
constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT kAppBarMessage = WM_APP + 2;

constexpr UINT_PTR kTimerPointer = 1;
constexpr UINT_PTR kTimerAnimate = 2;
constexpr UINT_PTR kTimerDeadline = 3;
constexpr UINT_PTR kTimerGuard = 4;
constexpr UINT_PTR kTimerPause = 5;
constexpr UINT_PTR kTimerSettings = 6;
constexpr UINT_PTR kTimerPreview = 7;
constexpr UINT_PTR kTimerTrim = 8;

constexpr UINT kPointerThrottleMs = 30;
constexpr UINT kAnimationFrameMs = 16;
// Cursor/z-order/fullscreen polling: fast while hovering (to notice leaving), slow while dimmed.
constexpr UINT kGuardHoverMs = 250;
constexpr UINT kGuardDimmedMs = 1000;
// Editors often save in several steps (truncate, write, rename); wait for them to finish.
constexpr UINT kSettingsDebounceMs = 300;
// How long the dim-level preview stays visible after the slider last moved.
constexpr UINT kPreviewMs = 1500;
// Trim the working set once things settle after startup or after the settings panel closes.
constexpr UINT kTrimDelayMs = 5000;
constexpr ULONGLONG kTicksPerMs = 10'000;  // FILETIME units are 100 ns.

constexpr int kHotkeyDesktopIcons = 1;
static_assert(Hotkey::kAlt == MOD_ALT && Hotkey::kCtrl == MOD_CONTROL && Hotkey::kShift == MOD_SHIFT);

// GUID_SESSION_DISPLAY_STATUS; defined locally to avoid INITGUID.
constexpr GUID kSessionDisplayStatus = {0x2B84C20E, 0xAD23, 0x4DDF, {0x93, 0xDB, 0x05, 0xFF, 0xBD, 0x7E, 0xFC, 0xA5}};

enum MenuCommand : UINT {
    kCmdPause15 = 100,
    kCmdPause60,
    kCmdPauseUntilResumed,
    kCmdResume,
    kCmdDimNow,
    kCmdToggleDesktopIcons,
    kCmdSettings,
    kCmdOpenLog,
    kCmdExit,
};

constexpr const wchar_t* kShellUiProcesses[] = {
    L"StartMenuExperienceHost.exe",
    L"SearchHost.exe",
    L"ShellExperienceHost.exe",
    L"ShellHost.exe",
};

bool IsShellUiProcess(std::wstring_view image) {
    return std::any_of(std::begin(kShellUiProcesses), std::end(kShellUiProcesses),
                       [image](const wchar_t* name) { return EqualsIgnoreCase(image, name); });
}

void* AsPointer(HWND hwnd) noexcept {
    return static_cast<void*>(hwnd);
}

SYSTEMTIME LocalTimeAfterMinutes(UINT minutes) {
    SYSTEMTIME now{};
    GetLocalTime(&now);
    FILETIME fileTime{};
    SystemTimeToFileTime(&now, &fileTime);
    ULARGE_INTEGER ticks{};
    ticks.LowPart = fileTime.dwLowDateTime;
    ticks.HighPart = fileTime.dwHighDateTime;
    ticks.QuadPart += static_cast<ULONGLONG>(minutes) * 60'000ULL * kTicksPerMs;
    fileTime.dwLowDateTime = ticks.LowPart;
    fileTime.dwHighDateTime = ticks.HighPart;
    SYSTEMTIME end{};
    FileTimeToSystemTime(&fileTime, &end);
    return end;
}

// UTC wall clock in FILETIME units; unlike timers it keeps counting during sleep.
ULONGLONG UtcNowTicks() {
    FILETIME now{};
    GetSystemTimeAsFileTime(&now);
    ULARGE_INTEGER ticks{};
    ticks.LowPart = now.dwLowDateTime;
    ticks.HighPart = now.dwHighDateTime;
    return ticks.QuadPart;
}

}  // namespace

int AppHost::Run(HINSTANCE instance) {
    instance_ = instance;
    if (!Initialize()) {
        log::Info(L"Initialization failed");
        if (hwnd_) {
            DestroyWindow(hwnd_);
        }
        Shutdown();
        return 1;
    }

    // Like GetMessage, but also wakes when the settings directory changes.
    for (;;) {
        HANDLE change = settingsStore_.ChangeEvent();
        const DWORD handleCount = change ? 1 : 0;
        const DWORD wait = MsgWaitForMultipleObjectsEx(handleCount, change ? &change : nullptr, INFINITE,
                                                       QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        if (handleCount == 1 && wait == WAIT_OBJECT_0) {
            settingsStore_.Rearm();
            SetTimer(hwnd_, kTimerSettings, kSettingsDebounceMs, nullptr);
            continue;
        }
        if (wait == WAIT_FAILED) {
            log::Info(L"Settings watch failed ({}); live reload disabled", GetLastError());
            settingsStore_.StopWatching();
        }
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                Shutdown();
                return static_cast<int>(message.wParam);
            }
            if (settingsDialog_.Hwnd() && IsDialogMessageW(settingsDialog_.Hwnd(), &message)) {
                continue;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
}

bool AppHost::Initialize() {
    WNDCLASSEXW windowClass{sizeof(windowClass)};
    windowClass.lpfnWndProc = &AppHost::WndProc;
    windowClass.hInstance = instance_;
    windowClass.lpszClassName = kWindowClass;
    if (!RegisterClassExW(&windowClass)) {
        log::Info(L"RegisterClassExW failed: {}", GetLastError());
        return false;
    }
    // Hidden top-level (not message-only) window so broadcasts like TaskbarCreated and WM_DISPLAYCHANGE arrive.
    if (!CreateWindowExW(WS_EX_TOOLWINDOW, kWindowClass, L"OLEDbar", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr,
                         instance_, this)) {
        log::Info(L"CreateWindowExW failed: {}", GetLastError());
        return false;
    }

    taskbarCreatedMessage_ = RegisterWindowMessageW(L"TaskbarCreated");
    ChangeWindowMessageFilterEx(hwnd_, taskbarCreatedMessage_, MSGFLT_ALLOW, nullptr);
    showSettingsMessage_ = RegisterWindowMessageW(kShowSettingsMessageName);
    ChangeWindowMessageFilterEx(hwnd_, showSettingsMessage_, MSGFLT_ALLOW, nullptr);

    if (startup::RefreshPath()) {
        log::Info(L"Run-at-startup entry updated to this EXE's location");
    }

    if (!overlay_.Create(instance_)) {
        log::Info(L"Overlay creation failed: {}", GetLastError());
        return false;
    }

    rawInputActive_ = pointer_.Register(hwnd_);
    if (!rawInputActive_) {
        log::Info(L"Raw input registration failed ({}); using polling only", GetLastError());
    }

    shell_.Start(ShellEventMonitor::Callbacks{
        .foregroundChanged =
            [this](HWND foreground) {
                OnForeground(foreground);
                Sync();
            },
        .taskbarMoved =
            [this] {
                RefreshTaskbar(L"location change");
                Sync();
            },
    });

    RegisterAppBar();
    UpdateTray();
    if (!tray_.Add(hwnd_, kTrayMessage)) {
        log::Info(L"Tray icon could not be added");
    }

    RefreshTaskbar(L"startup");
    if (!settingsStore_.Initialize()) {
        log::Info(L"Settings file unavailable ({}); using defaults", GetLastError());
    }
    ReloadSettings(true);

    // Registration immediately delivers the current display state, so the taskbar must be known by now.
    sessionNotifications_ = WTSRegisterSessionNotification(hwnd_, NOTIFY_FOR_THIS_SESSION) != FALSE;
    powerNotify_ = RegisterPowerSettingNotification(hwnd_, &kSessionDisplayStatus, DEVICE_NOTIFY_WINDOW_HANDLE);
    if (!sessionNotifications_ || !powerNotify_) {
        log::Info(L"Session/power notifications: session={} display={}", sessionNotifications_,
                  powerNotify_ != nullptr);
    }

    OnForeground(GetForegroundWindow());
    Sync();
    SetTimer(hwnd_, kTimerTrim, kTrimDelayMs, nullptr);
    tray_.Notify(L"OLEDbar is running",
                 std::format(L"The taskbar dims after {} s without the mouse on it. Click the OLEDbar icon in "
                             L"the system tray (it may be behind ^) for settings.",
                             settings_.idleTimeoutSeconds),
                 false);
    return true;
}

void AppHost::Shutdown() {
    if (shutDown_) {
        return;
    }
    shutDown_ = true;
    settingsDialog_.Close();
    SetDesktopIconsHotkey(Hotkey{});
    tray_.Remove();
    UnregisterAppBar();
    shell_.Stop();
    pointer_.Unregister();
    settingsStore_.StopWatching();
    if (sessionNotifications_) {
        WTSUnRegisterSessionNotification(hwnd_);
        sessionNotifications_ = false;
    }
    if (powerNotify_) {
        UnregisterPowerSettingNotification(powerNotify_);
        powerNotify_ = nullptr;
    }
    overlay_.SetOpacity(0.0);
    log::Info(L"Shutdown");
}

void AppHost::ReloadSettings(bool startup) {
    SettingsStore::LoadResult result = settingsStore_.Load();
    for (const std::wstring& message : result.messages) {
        log::Info(L"Settings: {}", message);
    }
    if (result.settings) {
        ApplySettings(*result.settings);
    }
    if (result.messages.empty()) {
        return;
    }

    std::wstring text;
    if (!result.settings) {
        text = startup ? L"settings.json has an error; using defaults." : L"settings.json has an error; keeping previous settings.";
    }
    text += (text.empty() ? L"" : L" ") + result.messages.front();
    if (result.messages.size() > 1) {
        text += std::format(L" (+{} more, see log)", result.messages.size() - 1);
    }
    tray_.Notify(L"OLEDbar settings", text, true);
}

void AppHost::ApplySettings(const Settings& settings) {
    settings_ = settings;
    log::SetDebugEnabled(settings_.debugLogging);
    machine_.SetPolicy(ToPolicy(settings_));
    if (!overlay_.SetExcludeFromCapture(settings_.excludeFromCapture)) {
        log::Info(L"Capture exclusion could not be changed ({})", GetLastError());
    }
    if (!settings_.wakeOnShellUi) {
        machine_.SetInteraction(false);
    }
    RecomputeSuppression();
    UpdateTray();
    if (!SetDesktopIconsHotkey(settings_.toggleDesktopIconsHotkey)) {
        tray_.Notify(L"OLEDbar shortcut unavailable",
                     std::format(L"{} is already used by another app. Choose a different shortcut in Settings.",
                                 Widen(FormatHotkey(settings_.toggleDesktopIconsHotkey))),
                     true);
    }
    log::Info(L"Settings applied: enabled={} idle={}s redim={}s dim={}% fadeToDim={}ms fadeToWake={}ms "
              L"taskbarFocus={} shellUi={} hideInFullscreen={} excludeFromCapture={} logLevel={} "
              L"desktopIconsHotkey=\"{}\"",
              settings_.enabled, settings_.idleTimeoutSeconds, settings_.redimDelaySeconds,
              settings_.dimLevelPercent, settings_.fadeToDimMs, settings_.fadeToWakeMs,
              settings_.wakeOnTaskbarFocus, settings_.wakeOnShellUi, settings_.hideInFullscreen,
              settings_.excludeFromCapture, settings_.debugLogging ? L"debug" : L"info",
              Widen(FormatHotkey(settings_.toggleDesktopIconsHotkey)));
    Sync();
}

bool AppHost::SetDesktopIconsHotkey(const Hotkey& hotkey) {
    if (hotkey == registeredHotkey_) {
        return true;
    }
    if (registeredHotkey_.IsSet()) {
        UnregisterHotKey(hwnd_, kHotkeyDesktopIcons);
        registeredHotkey_ = {};
    }
    if (!hotkey.IsSet()) {
        return true;
    }
    // MOD_NOREPEAT: holding the keys down toggles once, not repeatedly.
    if (!RegisterHotKey(hwnd_, kHotkeyDesktopIcons, hotkey.modifiers | MOD_NOREPEAT, hotkey.key)) {
        log::Info(L"Shortcut {} could not be registered ({})", Widen(FormatHotkey(hotkey)), GetLastError());
        return false;
    }
    registeredHotkey_ = hotkey;
    return true;
}

void AppHost::OnToggleDesktopIcons(const wchar_t* source) {
    if (ToggleDesktopIcons()) {
        log::Info(L"Desktop icons toggled ({})", source);
    } else {
        log::Info(L"Desktop icons could not be toggled ({}): {}", source, GetLastError());
    }
}

void AppHost::Pause(PauseMode mode, UINT minutes) {
    pause_ = mode;
    KillTimer(hwnd_, kTimerPause);
    if (mode == PauseMode::Timed) {
        pauseEnd_ = LocalTimeAfterMinutes(minutes);
        pauseEndTicks_ = UtcNowTicks() + static_cast<ULONGLONG>(minutes) * 60'000ULL * kTicksPerMs;
        SetTimer(hwnd_, kTimerPause, minutes * 60'000, nullptr);
        log::Info(L"Paused for {} min (until {:02}:{:02})", minutes, pauseEnd_.wHour, pauseEnd_.wMinute);
    } else {
        log::Info(L"Paused until resumed");
    }
    RecomputeSuppression();
    UpdateTray();
}

void AppHost::Resume() {
    KillTimer(hwnd_, kTimerPause);
    pause_ = PauseMode::None;
    log::Info(L"Resumed");
    RecomputeSuppression();
    UpdateTray();
}

void AppHost::UpdateTray() {
    const bool active = pause_ == PauseMode::None && settings_.enabled;
    std::wstring tip;
    if (!settings_.enabled) {
        tip = L"OLEDbar - disabled in settings";
    } else if (pause_ == PauseMode::Timed) {
        tip = std::format(L"OLEDbar - paused until {:02}:{:02}", pauseEnd_.wHour, pauseEnd_.wMinute);
    } else if (pause_ == PauseMode::UntilResumed) {
        tip = L"OLEDbar - paused";
    } else {
        tip = std::format(L"OLEDbar - dims to {}% after {}s", 100.0 - settings_.dimLevelPercent,
                          settings_.idleTimeoutSeconds);
    }
    const int size = GetSystemMetricsForDpi(SM_CXSMICON, GetDpiForSystem());
    tray_.Update(CreateStatusIcon(size, IsTaskbarLight(), active), tip);
}

void AppHost::ShowSettings() {
    log::Debug(L"Settings panel requested");
    // comctl32 is delay-loaded, so it is only mapped once the panel is first opened.
    static const bool commonControlsReady = [] {
        INITCOMMONCONTROLSEX init{sizeof(init), ICC_BAR_CLASSES | ICC_UPDOWN_CLASS | ICC_HOTKEY_CLASS};
        return InitCommonControlsEx(&init) != FALSE;
    }();
    if (!commonControlsReady) {
        log::Info(L"Common controls could not be initialized");
    }
    const bool shown = settingsDialog_.Show(instance_, settings_, startup::IsEnabled(),
                         SettingsDialog::Callbacks{
                             .apply = [this](const Settings& settings, bool runAtStartup) {
                                 return ApplyFromDialog(settings, runAtStartup);
                             },
                             .preview =
                                 [this](double dimLevel) {
                                     previewLevel_ = dimLevel;
                                     SetTimer(hwnd_, kTimerPreview, kPreviewMs, nullptr);
                                     Sync();
                                 },
                             .openFile = [this] { OpenFile(settingsStore_.Path()); },
                             .closed =
                                 [this] {
                                     KillTimer(hwnd_, kTimerPreview);
                                     previewLevel_.reset();
                                     SetTimer(hwnd_, kTimerTrim, kTrimDelayMs, nullptr);
                                     Sync();
                                 },
                         });
    if (!shown) {
        log::Info(L"Settings panel could not be opened ({})", GetLastError());
    }
}

bool AppHost::ApplyFromDialog(const Settings& settings, bool runAtStartup) {
    // Claim the shortcut first, so a conflict keeps the dialog open and nothing is saved.
    if (!SetDesktopIconsHotkey(settings.toggleDesktopIconsHotkey)) {
        SetDesktopIconsHotkey(settings_.toggleDesktopIconsHotkey);
        MessageBoxW(settingsDialog_.Hwnd(),
                    std::format(L"{} is already used by another app. Choose a different shortcut.",
                                Widen(FormatHotkey(settings.toggleDesktopIconsHotkey)))
                        .c_str(),
                    L"OLEDbar", MB_OK | MB_ICONWARNING);
        return false;
    }
    if (!settingsStore_.Save(settings)) {
        log::Info(L"Saving settings failed ({})", GetLastError());
        SetDesktopIconsHotkey(settings_.toggleDesktopIconsHotkey);
        MessageBoxW(settingsDialog_.Hwnd(), (L"Could not save " + settingsStore_.Path()).c_str(), L"OLEDbar",
                    MB_OK | MB_ICONERROR);
        return false;
    }
    log::Info(L"Settings saved from the settings panel");
    ApplySettings(settings);

    if (runAtStartup != startup::IsEnabled()) {
        if (!startup::SetEnabled(runAtStartup)) {
            log::Info(L"Changing run at startup failed ({})", GetLastError());
            MessageBoxW(settingsDialog_.Hwnd(), L"Could not change the \"Start OLEDbar when I sign in\" setting.",
                        L"OLEDbar", MB_OK | MB_ICONERROR);
            return false;
        }
        log::Info(L"Run at startup: {}", runAtStartup);
    }
    return true;
}

void AppHost::OpenFile(const std::wstring& path) {
    if (path.empty()) {
        return;
    }
    const auto result = reinterpret_cast<INT_PTR>(
        ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    // No app associated with the extension: fall back to Notepad.
    if (result <= 32) {
        const std::wstring quoted = L"\"" + path + L"\"";
        ShellExecuteW(nullptr, L"open", L"notepad.exe", quoted.c_str(), nullptr, SW_SHOWNORMAL);
    }
}

void AppHost::RegisterAppBar() {
    // Registered without ABM_SETPOS so no screen space is reserved; used only for ABN_FULLSCREENAPP.
    APPBARDATA data{sizeof(data)};
    data.hWnd = hwnd_;
    data.uCallbackMessage = kAppBarMessage;
    appBarRegistered_ = SHAppBarMessage(ABM_NEW, &data) != 0;
    log::Info(L"AppBar registration: {}", appBarRegistered_);
}

void AppHost::UnregisterAppBar() {
    if (!appBarRegistered_) {
        return;
    }
    APPBARDATA data{sizeof(data)};
    data.hWnd = hwnd_;
    SHAppBarMessage(ABM_REMOVE, &data);
    appBarRegistered_ = false;
}

LRESULT CALLBACK AppHost::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        auto* host = static_cast<AppHost*>(create->lpCreateParams);
        host->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(host));
    }
    auto* host = reinterpret_cast<AppHost*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (host && host->hwnd_ == hwnd) {
        return host->HandleMessage(message, wParam, lParam);
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT AppHost::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    if (showSettingsMessage_ != 0 && message == showSettingsMessage_) {
        ShowSettings();
        return 0;
    }
    if (taskbarCreatedMessage_ != 0 && message == taskbarCreatedMessage_) {
        log::Info(L"TaskbarCreated: explorer restarted");
        UpdateTray();
        tray_.Readd();
        UnregisterAppBar();
        RegisterAppBar();
        fullscreenNotified_ = false;
        RefreshTaskbar(L"TaskbarCreated");
        Sync();
        return 0;
    }

    switch (message) {
    case WM_INPUT:
        ++rawInputEvents_;
        // Trailing-edge throttle: the latest cursor position is evaluated once per interval.
        if (!pointerTimerArmed_) {
            SetTimer(hwnd_, kTimerPointer, kPointerThrottleMs, nullptr);
            pointerTimerArmed_ = true;
        }
        break;

    case WM_TIMER:
        OnTimer(static_cast<UINT_PTR>(wParam));
        return 0;

    case WM_HOTKEY:
        if (wParam == kHotkeyDesktopIcons) {
            OnToggleDesktopIcons(L"shortcut");
        }
        return 0;

    case kTrayMessage:
        switch (LOWORD(lParam)) {
        case NIN_SELECT:
        case NIN_KEYSELECT:
            ShowSettings();
            break;
        case WM_CONTEXTMENU:
            ShowTrayMenu(POINT{GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam)});
            break;
        default:
            break;
        }
        return 0;

    case kAppBarMessage:
        switch (static_cast<UINT>(wParam)) {
        case ABN_FULLSCREENAPP:
            fullscreenNotified_ = lParam != 0;
            log::Info(L"ABN_FULLSCREENAPP: {}", fullscreenNotified_);
            RecomputeSuppression();
            break;
        case ABN_STATECHANGE:
        case ABN_POSCHANGED:
            RefreshTaskbar(L"appbar notification");
            break;
        default:
            break;
        }
        Sync();
        return 0;

    case WM_DISPLAYCHANGE:
        RefreshTaskbar(L"WM_DISPLAYCHANGE");
        UpdateTray();
        Sync();
        return 0;

    case WM_WTSSESSION_CHANGE:
        switch (static_cast<UINT>(wParam)) {
        case WTS_SESSION_LOCK:
            locked_ = true;
            log::Info(L"Session locked");
            break;
        case WTS_SESSION_UNLOCK:
            locked_ = false;
            log::Info(L"Session unlocked");
            RefreshTaskbar(L"unlock");
            break;
        case WTS_CONSOLE_CONNECT:
            log::Info(L"Console session connected");
            RefreshTaskbar(L"session connect");
            break;
        case WTS_REMOTE_CONNECT:
            log::Info(L"Remote session connected");
            RefreshTaskbar(L"session connect");
            break;
        default:
            break;
        }
        RecomputeSuppression();
        Sync();
        return 0;

    case WM_POWERBROADCAST:
        switch (static_cast<UINT>(wParam)) {
        case PBT_POWERSETTINGCHANGE: {
            const auto* setting = reinterpret_cast<const POWERBROADCAST_SETTING*>(lParam);
            if (setting && setting->PowerSetting == kSessionDisplayStatus && setting->DataLength >= sizeof(DWORD)) {
                // 0 = off, 1 = on, 2 = dimmed by power management.
                const DWORD state = *reinterpret_cast<const DWORD*>(setting->Data);
                const bool off = state == 0;
                if (off != displayOff_) {
                    displayOff_ = off;
                    log::Info(L"Display {}", off ? L"off" : L"on");
                }
            }
            break;
        }
        case PBT_APMRESUMEAUTOMATIC:
        case PBT_APMRESUMESUSPEND:
            log::Info(L"Resumed from sleep");
            RefreshTaskbar(L"resume");
            machine_.Poke();
            if (pause_ == PauseMode::Timed) {
                const ULONGLONG now = UtcNowTicks();
                if (now >= pauseEndTicks_) {
                    Resume();
                } else {
                    SetTimer(hwnd_, kTimerPause, static_cast<UINT>((pauseEndTicks_ - now) / kTicksPerMs) + 1, nullptr);
                }
            }
            break;
        default:
            break;
        }
        RecomputeSuppression();
        Sync();
        return TRUE;

    case WM_SETTINGCHANGE:
        if (lParam != 0 && std::wstring_view(reinterpret_cast<const wchar_t*>(lParam)) == L"ImmersiveColorSet") {
            UpdateTray();
        }
        RefreshTaskbar(L"WM_SETTINGCHANGE");
        Sync();
        return 0;

    case WM_ENDSESSION:
        // Sign-out/shutdown, or an installer using Restart Manager (ENDSESSION_CLOSEAPP) asking us to exit.
        if (wParam) {
            log::Info(L"End of session requested (flags 0x{:X}); exiting", static_cast<UINT>(lParam));
            DestroyWindow(hwnd_);
        }
        return 0;

    case WM_DESTROY:
        Shutdown();
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }
    return DefWindowProcW(hwnd_, message, wParam, lParam);
}

void AppHost::OnTimer(UINT_PTR id) {
    switch (id) {
    case kTimerPointer:
        KillTimer(hwnd_, kTimerPointer);
        pointerTimerArmed_ = false;
        UpdateHover();
        break;
    case kTimerAnimate:
        machine_.Update();
        break;
    case kTimerDeadline:
        KillTimer(hwnd_, kTimerDeadline);
        armedDeadline_.reset();
        UpdateFullscreen();
        machine_.Update();
        break;
    case kTimerGuard:
        UpdateHover();
        UpdateFullscreen();
        if (overlay_.EnsureAbove(taskbar_.Info().hwnd)) {
            log::Info(L"Z-order: re-raised overlay above taskbar");
        }
        break;
    case kTimerPause:
        Resume();
        break;
    case kTimerPreview:
        KillTimer(hwnd_, kTimerPreview);
        previewLevel_.reset();
        break;
    case kTimerTrim:
        KillTimer(hwnd_, kTimerTrim);
        // Pages that are touched again (timer and hover paths) fault back in cheaply.
        SetProcessWorkingSetSizeEx(GetCurrentProcess(), static_cast<SIZE_T>(-1), static_cast<SIZE_T>(-1), 0);
        return;
    case kTimerSettings:
        KillTimer(hwnd_, kTimerSettings);
        if (settingsStore_.HasChanged()) {
            log::Info(L"Settings file changed; reloading");
            ReloadSettings(false);
        }
        break;
    default:
        return;
    }
    Sync();
}

void AppHost::RefreshTaskbar(const wchar_t* reason) {
    const bool changed = taskbar_.Refresh();
    const TaskbarInfo& info = taskbar_.Info();
    if (changed) {
        log::Info(L"Taskbar [{}]: hwnd={} pid={} window={} visible={} monitor={} autoHide={} shown={}", reason,
                  AsPointer(info.hwnd), info.processId, ToString(info.windowRect), ToString(info.visibleRect),
                  ToString(info.monitorRect), info.autoHide, info.visible);
    }
    overlay_.SetBounds(info.visibleRect);
    shell_.WatchTaskbar(info.hwnd, info.processId);
    RecomputeSuppression();
    UpdateHover();
}

void AppHost::UpdateHover() {
    POINT cursor{};
    if (!GetCursorPos(&cursor)) {
        return;
    }
    const TaskbarInfo& info = taskbar_.Info();
    const bool hover = info.visible && PtInRect(&info.visibleRect, cursor);
    if (hover != hover_) {
        hover_ = hover;
        log::Debug(L"Hover: {} (raw input events so far: {})", hover, rawInputEvents_);
        machine_.SetHover(hover);
    }
}

void AppHost::OnForeground(HWND foreground) {
    if (!foreground) {
        return;
    }
    const TaskbarInfo& info = taskbar_.Info();
    const std::wstring windowClass = WindowClassName(foreground);
    const bool isTaskbar =
        foreground == info.hwnd || windowClass == L"Shell_TrayWnd" || windowClass == L"Shell_SecondaryTrayWnd";
    const std::wstring image = isTaskbar ? std::wstring{} : ProcessImageName(foreground);
    const bool shellUi = !isTaskbar && IsShellUiProcess(image);
    log::Debug(L"Foreground: class={} image={} taskbar={} shellUi={}", windowClass, image, isTaskbar, shellUi);

    // Taskbar focus is a pulse, not a hold: explorer can keep focus long after the user moved away.
    if (isTaskbar && settings_.wakeOnTaskbarFocus) {
        machine_.Poke();
    }
    machine_.SetInteraction(shellUi && settings_.wakeOnShellUi);
    UpdateFullscreen();
    if (overlay_.EnsureAbove(info.hwnd)) {
        log::Info(L"Z-order: re-raised overlay above taskbar");
    }
}

void AppHost::UpdateFullscreen() {
    const bool fullscreen = IsForegroundFullscreen();
    if (fullscreen != fullscreenForeground_) {
        fullscreenForeground_ = fullscreen;
        log::Info(L"Foreground fullscreen: {}", fullscreen);
    }
    RecomputeSuppression();
}

bool AppHost::IsForegroundFullscreen() const {
    QUERY_USER_NOTIFICATION_STATE state{};
    if (SUCCEEDED(SHQueryUserNotificationState(&state)) &&
        (state == QUNS_BUSY || state == QUNS_RUNNING_D3D_FULL_SCREEN || state == QUNS_PRESENTATION_MODE)) {
        return true;
    }

    const HWND foreground = GetForegroundWindow();
    const TaskbarInfo& info = taskbar_.Info();
    if (!foreground || !info.hwnd || foreground == GetShellWindow() || foreground == GetDesktopWindow()) {
        return false;
    }
    const std::wstring windowClass = WindowClassName(foreground);
    if (windowClass == L"Progman" || windowClass == L"WorkerW" || windowClass == L"Shell_TrayWnd") {
        return false;
    }
    if (MonitorFromWindow(foreground, MONITOR_DEFAULTTONULL) != MonitorFromWindow(info.hwnd, MONITOR_DEFAULTTOPRIMARY)) {
        return false;
    }
    RECT rect{};
    if (!GetWindowRect(foreground, &rect)) {
        return false;
    }
    const RECT& monitor = info.monitorRect;
    return rect.left <= monitor.left && rect.top <= monitor.top && rect.right >= monitor.right &&
           rect.bottom >= monitor.bottom;
}

void AppHost::RecomputeSuppression() {
    const TaskbarInfo& info = taskbar_.Info();
    std::wstring reasons;
    const auto add = [&reasons](bool active, const wchar_t* reason) {
        if (active) {
            reasons += reasons.empty() ? L"" : L", ";
            reasons += reason;
        }
    };
    add(pause_ != PauseMode::None, L"paused");
    add(!settings_.enabled, L"disabled in settings");
    add(info.hwnd == nullptr, L"taskbar not found");
    add(info.hwnd != nullptr && !info.visible, L"taskbar hidden");
    add(settings_.hideInFullscreen && (fullscreenNotified_ || fullscreenForeground_), L"fullscreen app");
    add(locked_, L"session locked");
    add(displayOff_, L"display off");

    if (reasons != suppressionReasons_) {
        log::Info(L"Suppression: {}", reasons.empty() ? L"none" : reasons);
        suppressionReasons_ = reasons;
    }
    machine_.SetSuppressed(!reasons.empty());
}

void AppHost::Sync() {
    if (shutDown_) {
        return;
    }
    overlay_.SetOpacity(previewLevel_.value_or(machine_.Opacity()));

    const bool animating = machine_.IsAnimating();
    if (animating != animationTimerArmed_) {
        if (animating) {
            SetTimer(hwnd_, kTimerAnimate, kAnimationFrameMs, nullptr);
        } else {
            KillTimer(hwnd_, kTimerAnimate);
        }
        animationTimerArmed_ = animating;
    }

    const std::optional<TimePoint> deadline = machine_.NextDeadline();
    if (deadline != armedDeadline_) {
        KillTimer(hwnd_, kTimerDeadline);
        if (deadline) {
            const long long remaining =
                std::chrono::duration_cast<std::chrono::milliseconds>(*deadline - clock_.Now()).count();
            const auto delay =
                static_cast<UINT>(std::clamp<long long>(remaining + 1, USER_TIMER_MINIMUM, 24LL * 60 * 60 * 1000));
            SetTimer(hwnd_, kTimerDeadline, delay, nullptr);
        }
        armedDeadline_ = deadline;
    }

    // Nothing to watch while locked or with the display off; GetCursorPos also fails on the secure desktop.
    UINT guardInterval = 0;
    if (!locked_ && !displayOff_) {
        if (hover_) {
            guardInterval = kGuardHoverMs;
        } else if (overlay_.IsShown() || !rawInputActive_) {
            guardInterval = kGuardDimmedMs;
        }
    }
    if (guardInterval != guardIntervalMs_) {
        if (guardInterval != 0) {
            SetTimer(hwnd_, kTimerGuard, guardInterval, nullptr);
        } else {
            KillTimer(hwnd_, kTimerGuard);
        }
        guardIntervalMs_ = guardInterval;
    }

    if (machine_.State() != loggedState_) {
        log::Info(L"State {} -> {} (opacity {:.2f})", ToString(loggedState_), ToString(machine_.State()),
                  machine_.Opacity());
        loggedState_ = machine_.State();
    }
}

void AppHost::ShowTrayMenu(POINT anchor) {
    unique_menu menu(CreatePopupMenu());
    if (!menu) {
        return;
    }
    AppendMenuW(menu.get(), MF_STRING | MF_GRAYED, 0, L"OLEDbar " OLEDBAR_VERSION_WSTRING);
    AppendMenuW(menu.get(), MF_SEPARATOR, 0, nullptr);
    if (pause_ != PauseMode::None) {
        AppendMenuW(menu.get(), MF_STRING, kCmdResume, L"Resume");
    } else {
        // Owned by `menu` once appended; destroyed with it.
        HMENU pauseMenu = CreatePopupMenu();
        AppendMenuW(pauseMenu, MF_STRING, kCmdPause15, L"For 15 minutes");
        AppendMenuW(pauseMenu, MF_STRING, kCmdPause60, L"For 1 hour");
        AppendMenuW(pauseMenu, MF_STRING, kCmdPauseUntilResumed, L"Until I resume");
        AppendMenuW(menu.get(), MF_POPUP | (settings_.enabled ? MF_ENABLED : MF_GRAYED),
                    reinterpret_cast<UINT_PTR>(pauseMenu), L"Pause");
    }
    const bool canDim = machine_.State() != DimState::Suppressed;
    AppendMenuW(menu.get(), MF_STRING | (canDim ? MF_ENABLED : MF_GRAYED), kCmdDimNow, L"Dim now");
    AppendMenuW(menu.get(), MF_SEPARATOR, 0, nullptr);
    // The shortcut is shown right-aligned after the tab.
    std::wstring toggleText = L"Toggle &desktop icons";
    if (registeredHotkey_.IsSet()) {
        toggleText += L"\t" + Widen(FormatHotkey(registeredHotkey_));
    }
    AppendMenuW(menu.get(), MF_STRING, kCmdToggleDesktopIcons, toggleText.c_str());
    AppendMenuW(menu.get(), MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu.get(), MF_STRING, kCmdSettings, L"&Settings...");
    SetMenuDefaultItem(menu.get(), kCmdSettings, FALSE);
    AppendMenuW(menu.get(), MF_STRING, kCmdOpenLog, L"Open log");
    AppendMenuW(menu.get(), MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu.get(), MF_STRING, kCmdExit, L"Exit");

    // Required so the menu closes when the user clicks elsewhere.
    SetForegroundWindow(hwnd_);
    const auto command = static_cast<UINT>(TrackPopupMenuEx(
        menu.get(), TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, anchor.x, anchor.y, hwnd_,
        nullptr));
    PostMessageW(hwnd_, WM_NULL, 0, 0);

    switch (command) {
    case kCmdPause15:
        Pause(PauseMode::Timed, 15);
        break;
    case kCmdPause60:
        Pause(PauseMode::Timed, 60);
        break;
    case kCmdPauseUntilResumed:
        Pause(PauseMode::UntilResumed, 0);
        break;
    case kCmdResume:
        Resume();
        break;
    case kCmdDimNow:
        UpdateFullscreen();
        machine_.DimNow();
        break;
    case kCmdToggleDesktopIcons:
        OnToggleDesktopIcons(L"tray menu");
        break;
    case kCmdSettings:
        ShowSettings();
        break;
    case kCmdOpenLog:
        OpenFile(log::Path());
        break;
    case kCmdExit:
        DestroyWindow(hwnd_);
        return;
    default:
        break;
    }
    Sync();
}

}  // namespace dimmer
