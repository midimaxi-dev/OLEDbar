#include "app/about_dialog.h"

#include <commctrl.h>
#include <shellapi.h>

#include <array>
#include <format>
#include <string>
#include <utility>

#include "app/dialog_util.h"
#include "app/resource.h"
#include "platform/log.h"
#include "version.h"

namespace dimmer {
namespace {

constexpr wchar_t kWebsiteUrl[] = L"https://midimaxi-dev.github.io/OLEDbar/";
constexpr wchar_t kRepositoryUrl[] = L"https://github.com/midimaxi-dev/OLEDbar";
constexpr wchar_t kReleasesUrl[] = L"https://github.com/midimaxi-dev/OLEDbar/releases/latest";
constexpr std::array kLinkUrls{kWebsiteUrl, kRepositoryUrl, kReleasesUrl};

void OpenUrl(HWND owner, const wchar_t* url) {
    const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(owner, L"open", url, nullptr, nullptr, SW_SHOWNORMAL));
    if (result <= 32) {
        log::Info(L"Could not open {} (ShellExecute result {})", url, result);
        const auto text = std::format(L"Windows could not open your browser. You can visit {} manually.", url);
        MessageBoxW(owner, text.c_str(), L"OLEDbar", MB_OK | MB_ICONWARNING);
    }
}

std::wstring StatusText(bool checking, const std::optional<UpdateCheckResult>& result) {
    if (checking) {
        return L"Checking for updates...";
    }
    if (!result) {
        return L"Check manually for the latest stable release.";
    }
    switch (result->outcome) {
    case UpdateOutcome::Available:
    case UpdateOutcome::UpToDate:
    case UpdateOutcome::NewerThanRelease: {
        if (!result->latest) {
            return L"Could not read the release version. See the log.";
        }
        const Version& version = *result->latest;
        const auto text = std::format(L"{}.{}.{}", version.major, version.minor, version.patch);
        if (result->outcome == UpdateOutcome::Available) {
            return std::format(L"OLEDbar {} is available.", text);
        }
        if (result->outcome == UpdateOutcome::NewerThanRelease) {
            return std::format(L"This version is newer than the latest release ({}).", text);
        }
        return L"You have the latest version.";
    }
    case UpdateOutcome::ConnectionError:
        return L"Could not connect to GitHub. Check your connection, proxy or firewall.";
    case UpdateOutcome::HttpError:
        if (result->httpStatus == 429) {
            return L"GitHub's rate limit was reached. Please try again later.";
        }
        if (result->httpStatus == 403) {
            return L"GitHub denied the request (HTTP 403). It may be rate-limited.";
        }
        return std::format(L"GitHub returned HTTP {}. Please try again later.", result->httpStatus);
    case UpdateOutcome::InvalidRelease:
        return L"GitHub returned an unrecognized release version. See Releases.";
    case UpdateOutcome::ResponseTooLarge:
        return L"GitHub's response was too large. See Releases.";
    case UpdateOutcome::InternalError:
        return L"The update check could not be completed. See the log.";
    }
    return L"The update check could not be completed. See the log.";
}

}  // namespace

bool AboutDialog::Show(HINSTANCE instance, HWND owner, bool checking, const std::optional<UpdateCheckResult>& result,
                       std::function<void()> checkForUpdates) {
    checking_ = checking;
    result_ = result;
    checkForUpdates_ = std::move(checkForUpdates);
    if (hwnd_) {
        UpdateControls();
        ShowWindow(hwnd_, SW_RESTORE);
        SetForegroundWindow(hwnd_);
        return true;
    }
    CreateDialogParamW(instance, MAKEINTRESOURCEW(IDD_ABOUT), owner, &AboutDialog::DialogProc,
                       reinterpret_cast<LPARAM>(this));
    if (!hwnd_) {
        return false;
    }
    ShowWindow(hwnd_, SW_SHOW);
    SetForegroundWindow(hwnd_);
    return true;
}

void AboutDialog::Close() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
    }
}

void AboutDialog::SetChecking() {
    checking_ = true;
    result_.reset();
    if (hwnd_) {
        UpdateControls();
    }
}

void AboutDialog::SetResult(const UpdateCheckResult& result) {
    checking_ = false;
    result_ = result;
    if (hwnd_) {
        UpdateControls();
    }
}

INT_PTR CALLBACK AboutDialog::DialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_INITDIALOG) {
        reinterpret_cast<AboutDialog*>(lParam)->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, DWLP_USER, lParam);
    }
    auto* self = reinterpret_cast<AboutDialog*>(GetWindowLongPtrW(hwnd, DWLP_USER));
    return self ? self->HandleMessage(message, wParam, lParam) : FALSE;
}

INT_PTR AboutDialog::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG:
        SetAppDialogIcons(hwnd_, smallIcon_, largeIcon_);
        SetDlgItemTextW(hwnd_, IDC_ABOUT_VERSION, L"OLEDbar " OLEDBAR_VERSION_WSTRING);
        UpdateControls();
        return TRUE;

    case WM_NOTIFY: {
        const auto* header = reinterpret_cast<const NMHDR*>(lParam);
        if (header && header->idFrom == IDC_ABOUT_LINKS && (header->code == NM_CLICK || header->code == NM_RETURN)) {
            const auto* link = reinterpret_cast<const NMLINK*>(lParam);
            if (link->item.iLink >= 0 && static_cast<size_t>(link->item.iLink) < kLinkUrls.size()) {
                OpenUrl(hwnd_, kLinkUrls[static_cast<size_t>(link->item.iLink)]);
            }
            return TRUE;
        }
        return FALSE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_CHECK_UPDATES:
            if (HIWORD(wParam) == BN_CLICKED && !checking_ && checkForUpdates_) {
                checkForUpdates_();
            }
            return TRUE;
        case IDC_OPEN_RELEASE:
            if (HIWORD(wParam) == BN_CLICKED && result_ && result_->outcome == UpdateOutcome::Available &&
                result_->latest) {
                OpenUrl(hwnd_, kReleasesUrl);
            }
            return TRUE;
        case IDCANCEL:
            Close();
            return TRUE;
        default:
            return FALSE;
        }

    case WM_CLOSE:
        Close();
        return TRUE;

    case WM_DESTROY:
        hwnd_ = nullptr;
        smallIcon_.reset();
        largeIcon_.reset();
        return TRUE;

    default:
        return FALSE;
    }
}

void AboutDialog::UpdateControls() {
    SetDlgItemTextW(hwnd_, IDC_UPDATE_STATUS, StatusText(checking_, result_).c_str());
    EnableWindow(GetDlgItem(hwnd_, IDC_CHECK_UPDATES), !checking_);
    const bool available = !checking_ && result_ && result_->outcome == UpdateOutcome::Available && result_->latest;
    ShowWindow(GetDlgItem(hwnd_, IDC_OPEN_RELEASE), available ? SW_SHOW : SW_HIDE);
}

}  // namespace dimmer
