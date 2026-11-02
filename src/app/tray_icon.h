#pragma once

#include <windows.h>
#include <shellapi.h>

#include <string>

#include <wil/resource.h>

namespace dimmer {

class TrayIcon {
public:
    // Call Update() first so the icon appears with its image and tooltip.
    bool Add(HWND owner, UINT callbackMessage);
    // Re-adds the icon after explorer restarts (TaskbarCreated).
    bool Readd();
    void Remove();
    void Update(wil::unique_hicon icon, const std::wstring& tip);
    // Shows a notification (a toast on Windows 11).
    void Notify(const std::wstring& title, const std::wstring& text, bool warning);

private:
    NOTIFYICONDATAW data_{};
    wil::unique_hicon icon_;
    bool added_ = false;
};

wil::unique_hicon CreateStatusIcon(int size, bool lightTaskbar, bool active);
bool IsTaskbarLight();

}  // namespace dimmer
