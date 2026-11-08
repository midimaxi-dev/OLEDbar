#pragma once

#include <windows.h>

#include <functional>

#include <wil/resource.h>

#include "core/settings.h"

namespace dimmer {

// Modeless settings dialog (OK / Cancel / Apply). The host must route messages through IsDialogMessage(Hwnd()).
class SettingsDialog {
public:
    struct Callbacks {
        // Returns false if applying failed; the dialog then stays open.
        std::function<bool(const Settings& settings, bool runAtStartup)> apply;
        // Temporary dim level to show while the slider moves (0..1).
        std::function<void(double dimLevel)> preview;
        std::function<void()> openFile;
        std::function<void()> showAbout;
        std::function<void()> closed;
    };

    SettingsDialog() = default;
    SettingsDialog(const SettingsDialog&) = delete;
    SettingsDialog& operator=(const SettingsDialog&) = delete;

    // Opens the dialog, or brings it to the front if already open. Returns false if it could not be created.
    bool Show(HINSTANCE instance, const Settings& settings, bool runAtStartup, Callbacks callbacks);
    void Close();
    HWND Hwnd() const noexcept { return hwnd_; }

private:
    static INT_PTR CALLBACK DialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    INT_PTR HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    void InitializeControls();
    void Load(const Settings& settings, bool runAtStartup);
    bool Read(Settings& settings, bool& runAtStartup);
    bool Apply();
    void Step(int spinId, int delta);
    int DimLevel() const;
    void UpdateDimLabel();

    HWND hwnd_ = nullptr;
    Callbacks callbacks_;
    Settings pending_;
    bool pendingRunAtStartup_ = false;
    wil::unique_hicon smallIcon_;
    wil::unique_hicon largeIcon_;
};

}  // namespace dimmer
