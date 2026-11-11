#include "app/settings_dialog.h"

#include <commctrl.h>

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <format>
#include <iterator>
#include <optional>
#include <string>
#include <utility>

#include "app/dialog_util.h"
#include "app/resource.h"
#include "version.h"

namespace dimmer {
namespace {

struct NumberField {
    int edit;
    int spin;
    double min;
    double max;
    double step;
    bool integer;
};

constexpr NumberField kFields[] = {
    {IDC_IDLE_EDIT, IDC_IDLE_SPIN, 1, 3600, 1, false},
    {IDC_REDIM_EDIT, IDC_REDIM_SPIN, 0, 3600, 1, false},
    {IDC_FADE_OUT_EDIT, IDC_FADE_OUT_SPIN, 0, 5000, 100, true},
    {IDC_FADE_IN_EDIT, IDC_FADE_IN_SPIN, 0, 5000, 20, true},
};

std::optional<double> GetNumber(HWND dialog, int id) {
    wchar_t text[32]{};
    GetDlgItemTextW(dialog, id, text, static_cast<int>(std::size(text)));
    wchar_t* end = nullptr;
    const double value = std::wcstod(text, &end);
    if (end == text) {
        return std::nullopt;
    }
    while (*end == L' ') {
        ++end;
    }
    if (*end != L'\0' || !std::isfinite(value)) {
        return std::nullopt;
    }
    return value;
}

void SetNumber(HWND dialog, int id, double value) {
    SetDlgItemTextW(dialog, id, std::format(L"{}", value).c_str());
}

void SetCheck(HWND dialog, int id, bool checked) {
    CheckDlgButton(dialog, id, checked ? BST_CHECKED : BST_UNCHECKED);
}

bool GetCheck(HWND dialog, int id) {
    return IsDlgButtonChecked(dialog, id) == BST_CHECKED;
}

void ShowFieldError(HWND edit, const std::wstring& text) {
    EDITBALLOONTIP tip{};
    tip.cbStruct = sizeof(tip);
    tip.pszTitle = L"Invalid value";
    tip.pszText = text.c_str();
    tip.ttiIcon = TTI_WARNING;
    SendMessageW(edit, EM_SHOWBALLOONTIP, 0, reinterpret_cast<LPARAM>(&tip));
    SetFocus(edit);
}

// The hotkey control uses HOTKEYF_* bits; Hotkey uses MOD_* bits.
void SetHotkeyControl(HWND dialog, int id, const Hotkey& hotkey) {
    BYTE flags = 0;
    flags |= (hotkey.modifiers & Hotkey::kCtrl) ? HOTKEYF_CONTROL : 0;
    flags |= (hotkey.modifiers & Hotkey::kAlt) ? HOTKEYF_ALT : 0;
    flags |= (hotkey.modifiers & Hotkey::kShift) ? HOTKEYF_SHIFT : 0;
    SendDlgItemMessageW(dialog, id, HKM_SETHOTKEY, MAKEWORD(static_cast<BYTE>(hotkey.key), flags), 0);
}

Hotkey GetHotkeyControl(HWND dialog, int id) {
    const auto value = static_cast<WORD>(SendDlgItemMessageW(dialog, id, HKM_GETHOTKEY, 0, 0));
    const BYTE flags = HIBYTE(value);
    Hotkey hotkey;
    hotkey.key = LOBYTE(value);
    if (hotkey.key == 0) {
        return {};
    }
    hotkey.modifiers |= (flags & HOTKEYF_CONTROL) ? Hotkey::kCtrl : 0;
    hotkey.modifiers |= (flags & HOTKEYF_ALT) ? Hotkey::kAlt : 0;
    hotkey.modifiers |= (flags & HOTKEYF_SHIFT) ? Hotkey::kShift : 0;
    return hotkey;
}

}  // namespace

bool SettingsDialog::Show(HINSTANCE instance, const Settings& settings, bool runAtStartup, Callbacks callbacks) {
    if (hwnd_) {
        ShowWindow(hwnd_, SW_RESTORE);
        SetForegroundWindow(hwnd_);
        return true;
    }
    callbacks_ = std::move(callbacks);
    pending_ = settings;
    pendingRunAtStartup_ = runAtStartup;
    CreateDialogParamW(instance, MAKEINTRESOURCEW(IDD_SETTINGS), nullptr, &SettingsDialog::DialogProc,
                       reinterpret_cast<LPARAM>(this));
    if (!hwnd_) {
        return false;
    }
    ShowWindow(hwnd_, SW_SHOW);
    SetForegroundWindow(hwnd_);
    return true;
}

void SettingsDialog::Close() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
    }
}

INT_PTR CALLBACK SettingsDialog::DialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_INITDIALOG) {
        reinterpret_cast<SettingsDialog*>(lParam)->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, DWLP_USER, lParam);
    }
    auto* self = reinterpret_cast<SettingsDialog*>(GetWindowLongPtrW(hwnd, DWLP_USER));
    return self ? self->HandleMessage(message, wParam, lParam) : FALSE;
}

INT_PTR SettingsDialog::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG:
        InitializeControls();
        Load(pending_, pendingRunAtStartup_);
        return TRUE;

    case WM_HSCROLL:
        if (reinterpret_cast<HWND>(lParam) == GetDlgItem(hwnd_, IDC_DIM_SLIDER)) {
            UpdateDimLabel();
            if (callbacks_.preview) {
                callbacks_.preview(DimLevel() / 100.0);
            }
        }
        return TRUE;

    case WM_NOTIFY: {
        const auto* header = reinterpret_cast<const NMHDR*>(lParam);
        if (!header) {
            return FALSE;
        }
        if (header->idFrom == IDC_SETTINGS_ABOUT && (header->code == NM_CLICK || header->code == NM_RETURN)) {
            if (callbacks_.showAbout) {
                callbacks_.showAbout();
            }
            return TRUE;
        }
        if (header->code == UDN_DELTAPOS) {
            const auto* upDown = reinterpret_cast<const NMUPDOWN*>(lParam);
            Step(static_cast<int>(header->idFrom), upDown->iDelta);
            // Handled here; returning TRUE keeps the up-down control's own position unchanged.
            SetWindowLongPtrW(hwnd_, DWLP_MSGRESULT, TRUE);
            return TRUE;
        }
        return FALSE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
            if (Apply()) {
                Close();
            }
            return TRUE;
        case IDCANCEL:
            Close();
            return TRUE;
        case IDC_APPLY:
            Apply();
            return TRUE;
        case IDC_DEFAULTS:
            Load(Settings{}, GetCheck(hwnd_, IDC_RUN_AT_STARTUP));
            return TRUE;
        case IDC_DESKTOP_ICONS_CLEAR:
            SetHotkeyControl(hwnd_, IDC_DESKTOP_ICONS_HOTKEY, Hotkey{});
            SetFocus(GetDlgItem(hwnd_, IDC_DESKTOP_ICONS_HOTKEY));
            return TRUE;
        case IDC_OPEN_FILE:
            if (callbacks_.openFile) {
                callbacks_.openFile();
            }
            return TRUE;
        default:
            return FALSE;
        }

    case WM_DESTROY:
        hwnd_ = nullptr;
        smallIcon_.reset();
        largeIcon_.reset();
        if (callbacks_.closed) {
            callbacks_.closed();
        }
        return TRUE;

    default:
        return FALSE;
    }
}

void SettingsDialog::InitializeControls() {
    const HWND slider = GetDlgItem(hwnd_, IDC_DIM_SLIDER);
    SendMessageW(slider, TBM_SETRANGEMIN, FALSE, 0);
    SendMessageW(slider, TBM_SETRANGEMAX, FALSE, 100);
    SendMessageW(slider, TBM_SETTICFREQ, 10, 0);
    SendMessageW(slider, TBM_SETLINESIZE, 0, 5);
    SendMessageW(slider, TBM_SETPAGESIZE, 0, 10);

    for (const NumberField& field : kFields) {
        // Steps are applied in UDN_DELTAPOS; a mid-range position keeps both arrows active.
        SendDlgItemMessageW(hwnd_, field.spin, UDM_SETRANGE32, 0, 100);
        SendDlgItemMessageW(hwnd_, field.spin, UDM_SETPOS32, 0, 50);
        SendDlgItemMessageW(hwnd_, field.edit, EM_SETLIMITTEXT, 8, 0);
    }

    // Keys pressed without Ctrl or Alt become Ctrl+Alt+key, so the shortcut can't swallow normal typing.
    SendDlgItemMessageW(hwnd_, IDC_DESKTOP_ICONS_HOTKEY, HKM_SETRULES, HKCOMB_NONE | HKCOMB_S,
                        MAKELPARAM(HOTKEYF_CONTROL | HOTKEYF_ALT, 0));

    SetDlgItemTextW(hwnd_, IDC_SETTINGS_ABOUT, L"OLEDbar " OLEDBAR_VERSION_WSTRING L"  <a id=\"about\">About</a>");
    SetAppDialogIcons(hwnd_, smallIcon_, largeIcon_);
}

void SettingsDialog::Load(const Settings& settings, bool runAtStartup) {
    SendDlgItemMessageW(hwnd_, IDC_DIM_SLIDER, TBM_SETPOS, TRUE, std::lround(settings.dimLevelPercent));
    UpdateDimLabel();
    SetNumber(hwnd_, IDC_IDLE_EDIT, settings.idleTimeoutSeconds);
    SetNumber(hwnd_, IDC_REDIM_EDIT, settings.redimDelaySeconds);
    SetNumber(hwnd_, IDC_FADE_OUT_EDIT, settings.fadeToDimMs);
    SetNumber(hwnd_, IDC_FADE_IN_EDIT, settings.fadeToWakeMs);
    SetCheck(hwnd_, IDC_WAKE_FOCUS, settings.wakeOnTaskbarFocus);
    SetCheck(hwnd_, IDC_WAKE_SHELL, settings.wakeOnShellUi);
    SetCheck(hwnd_, IDC_HIDE_FULLSCREEN, settings.hideInFullscreen);
    SetCheck(hwnd_, IDC_EXCLUDE_CAPTURE, settings.excludeFromCapture);
    SetCheck(hwnd_, IDC_RUN_AT_STARTUP, runAtStartup);
    SetCheck(hwnd_, IDC_DEBUG_LOG, settings.debugLogging);
    SetHotkeyControl(hwnd_, IDC_DESKTOP_ICONS_HOTKEY, settings.toggleDesktopIconsHotkey);
}

bool SettingsDialog::Read(Settings& settings, bool& runAtStartup) {
    double values[std::size(kFields)]{};
    for (size_t i = 0; i < std::size(kFields); ++i) {
        const NumberField& field = kFields[i];
        const std::optional<double> value = GetNumber(hwnd_, field.edit);
        if (!value || *value < field.min || *value > field.max || (field.integer && std::floor(*value) != *value)) {
            ShowFieldError(GetDlgItem(hwnd_, field.edit), std::format(L"Enter a {}number from {} to {}.",
                                                                       field.integer ? L"whole " : L"", field.min,
                                                                       field.max));
            return false;
        }
        values[i] = *value;
    }
    const Hotkey hotkey = GetHotkeyControl(hwnd_, IDC_DESKTOP_ICONS_HOTKEY);
    if (!hotkey.IsValid()) {
        MessageBoxW(hwnd_, L"The desktop icons shortcut needs Ctrl or Alt.", L"OLEDbar", MB_OK | MB_ICONWARNING);
        SetFocus(GetDlgItem(hwnd_, IDC_DESKTOP_ICONS_HOTKEY));
        return false;
    }

    settings.dimLevelPercent = DimLevel();
    settings.idleTimeoutSeconds = values[0];
    settings.redimDelaySeconds = values[1];
    settings.fadeToDimMs = static_cast<int>(values[2]);
    settings.fadeToWakeMs = static_cast<int>(values[3]);
    settings.wakeOnTaskbarFocus = GetCheck(hwnd_, IDC_WAKE_FOCUS);
    settings.wakeOnShellUi = GetCheck(hwnd_, IDC_WAKE_SHELL);
    settings.hideInFullscreen = GetCheck(hwnd_, IDC_HIDE_FULLSCREEN);
    settings.excludeFromCapture = GetCheck(hwnd_, IDC_EXCLUDE_CAPTURE);
    settings.debugLogging = GetCheck(hwnd_, IDC_DEBUG_LOG);
    settings.toggleDesktopIconsHotkey = hotkey;
    runAtStartup = GetCheck(hwnd_, IDC_RUN_AT_STARTUP);
    return true;
}

bool SettingsDialog::Apply() {
    Settings settings;
    bool runAtStartup = false;
    if (!Read(settings, runAtStartup)) {
        return false;
    }
    return !callbacks_.apply || callbacks_.apply(settings, runAtStartup);
}

void SettingsDialog::Step(int spinId, int delta) {
    const auto* field = std::ranges::find(kFields, spinId, &NumberField::spin);
    if (field == std::end(kFields)) {
        return;
    }
    const double value = GetNumber(hwnd_, field->edit).value_or(field->min);
    SetNumber(hwnd_, field->edit, std::clamp(value + delta * field->step, field->min, field->max));
}

int SettingsDialog::DimLevel() const {
    return static_cast<int>(SendDlgItemMessageW(hwnd_, IDC_DIM_SLIDER, TBM_GETPOS, 0, 0));
}

void SettingsDialog::UpdateDimLabel() {
    SetDlgItemTextW(hwnd_, IDC_DIM_VALUE, std::format(L"{}% darker", DimLevel()).c_str());
}

}  // namespace dimmer
