#pragma once

#include <windows.h>

#include <functional>
#include <optional>

#include <wil/resource.h>

#include "platform/update_checker.h"

namespace dimmer {

class AboutDialog {
public:
    AboutDialog() = default;
    ~AboutDialog() { Close(); }
    AboutDialog(const AboutDialog&) = delete;
    AboutDialog& operator=(const AboutDialog&) = delete;

    bool Show(HINSTANCE instance, HWND owner, bool checking, const std::optional<UpdateCheckResult>& result,
              std::function<void()> checkForUpdates);
    void Close();
    HWND Hwnd() const noexcept { return hwnd_; }
    void SetChecking();
    void SetResult(const UpdateCheckResult& result);

private:
    static INT_PTR CALLBACK DialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    INT_PTR HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void UpdateControls();

    HWND hwnd_ = nullptr;
    bool checking_ = false;
    std::optional<UpdateCheckResult> result_;
    std::function<void()> checkForUpdates_;
    wil::unique_hicon smallIcon_;
    wil::unique_hicon largeIcon_;
};

}  // namespace dimmer
