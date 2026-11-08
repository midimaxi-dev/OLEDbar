#pragma once

#include <windows.h>

#include <optional>
#include <string>

#include "app/about_dialog.h"
#include "app/settings_dialog.h"
#include "app/tray_icon.h"
#include "core/clock.h"
#include "core/dim_state_machine.h"
#include "core/settings.h"
#include "platform/overlay_window.h"
#include "platform/pointer_monitor.h"
#include "platform/settings_store.h"
#include "platform/shell_event_monitor.h"
#include "platform/taskbar_locator.h"
#include "platform/update_checker.h"

namespace dimmer {

// Owns the hidden host window and message loop, and wires platform signals into the state machine.
class AppHost {
public:
    // Used by a second instance to find this one and ask it to show the settings panel.
    static constexpr wchar_t kWindowClassName[] = L"OLEDbar.Host";
    static constexpr wchar_t kShowSettingsMessageName[] = L"OLEDbar.ShowSettings";

    AppHost() = default;
    AppHost(const AppHost&) = delete;
    AppHost& operator=(const AppHost&) = delete;

    int Run(HINSTANCE instance);

private:
    enum class PauseMode { None, Timed, UntilResumed };

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    bool Initialize();
    void Shutdown();
    void RegisterAppBar();
    void UnregisterAppBar();

    void ReloadSettings(bool startup);
    void ApplySettings(const Settings& settings);
    // Registers `hotkey` in place of the current one (unset = none). On failure no shortcut is registered.
    bool SetDesktopIconsHotkey(const Hotkey& hotkey);
    void OnToggleDesktopIcons(const wchar_t* source);
    void Pause(PauseMode mode, UINT minutes);
    void Resume();
    void UpdateTray();
    void OpenFile(const std::wstring& path);
    void ShowSettings();
    void ShowAbout();
    void CheckForUpdates();
    void FinishUpdateCheck();
    bool ApplyFromDialog(const Settings& settings, bool runAtStartup);

    void RefreshTaskbar(const wchar_t* reason);
    void UpdateHover(bool buttonActivity = false);
    void OnForeground(HWND foreground);
    void UpdateFullscreen();
    bool IsForegroundFullscreen() const;
    void RecomputeSuppression();
    void OnTimer(UINT_PTR id);
    void ShowTrayMenu(POINT anchor);
    // Pushes state machine output to the overlay and (re)arms timers.
    void Sync();

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    UINT taskbarCreatedMessage_ = 0;
    UINT showSettingsMessage_ = 0;

    SteadyClock clock_;
    DimStateMachine machine_{clock_};
    TaskbarLocator taskbar_;
    OverlayWindow overlay_;
    PointerMonitor pointer_;
    ShellEventMonitor shell_;
    TrayIcon tray_;
    SettingsStore settingsStore_;
    Settings settings_;
    Hotkey registeredHotkey_;
    SettingsDialog settingsDialog_;
    AboutDialog aboutDialog_;
    UpdateChecker updateChecker_;
    std::optional<UpdateCheckResult> lastUpdateResult_;
    std::optional<double> previewLevel_;

    PauseMode pause_ = PauseMode::None;
    SYSTEMTIME pauseEnd_{};      // Local time, for the tooltip.
    ULONGLONG pauseEndTicks_ = 0;  // UTC FILETIME units, survives sleep.
    bool hover_ = false;
    bool pointerHeld_ = false;
    std::optional<POINT> pointerPosition_;
    bool fullscreenNotified_ = false;
    bool fullscreenForeground_ = false;
    bool rawInputActive_ = false;
    bool appBarRegistered_ = false;
    bool pointerTimerArmed_ = false;
    bool animationTimerArmed_ = false;
    UINT guardIntervalMs_ = 0;
    bool locked_ = false;
    bool displayOff_ = false;
    bool sessionNotifications_ = false;
    HPOWERNOTIFY powerNotify_ = nullptr;
    std::wstring suppressionReasons_;
    bool shutDown_ = false;
    bool updateWaitFallback_ = false;
    unsigned long long rawInputEvents_ = 0;
    std::optional<TimePoint> armedDeadline_;
    DimState loggedState_ = DimState::Countdown;
};

}  // namespace dimmer
