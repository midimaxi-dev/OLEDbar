#include "platform/startup.h"

#include <windows.h>

#include <cwchar>
#include <optional>
#include <string>

#include "platform/win_util.h"

namespace dimmer::startup {
namespace {

constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
// Task Manager's Startup apps toggle; first byte with the low bit set means "Disabled".
constexpr wchar_t kApprovedKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";
constexpr wchar_t kValueName[] = L"OLEDbar";

std::wstring CurrentCommand() {
    std::wstring path(MAX_PATH, L'\0');
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0) {
            return {};
        }
        if (length < path.size()) {
            path.resize(length);
            break;
        }
        path.resize(path.size() * 2);
    }
    return L"\"" + path + L"\"";
}

std::optional<std::wstring> ReadRunValue() {
    DWORD bytes = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, kRunKey, kValueName, RRF_RT_REG_SZ, nullptr, nullptr, &bytes) !=
        ERROR_SUCCESS) {
        return std::nullopt;
    }
    std::wstring value(bytes / sizeof(wchar_t), L'\0');
    if (RegGetValueW(HKEY_CURRENT_USER, kRunKey, kValueName, RRF_RT_REG_SZ, nullptr, value.data(), &bytes) !=
        ERROR_SUCCESS) {
        return std::nullopt;
    }
    value.resize(wcsnlen(value.c_str(), value.size()));
    return value;
}

bool DisabledInTaskManager() {
    BYTE data[64]{};
    DWORD bytes = sizeof(data);
    return RegGetValueW(HKEY_CURRENT_USER, kApprovedKey, kValueName, RRF_RT_REG_BINARY, nullptr, data, &bytes) ==
               ERROR_SUCCESS &&
           bytes > 0 && (data[0] & 1) != 0;
}

bool WriteRunValue(const std::wstring& command) {
    return RegSetKeyValueW(HKEY_CURRENT_USER, kRunKey, kValueName, REG_SZ, command.c_str(),
                           static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
}

}  // namespace

bool IsEnabled() {
    return ReadRunValue().has_value() && !DisabledInTaskManager();
}

bool SetEnabled(bool enable) {
    if (!enable) {
        const LSTATUS status = RegDeleteKeyValueW(HKEY_CURRENT_USER, kRunKey, kValueName);
        return status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND;
    }
    const std::wstring command = CurrentCommand();
    if (command.empty()) {
        return false;
    }
    // Clear a "Disabled" state set in Task Manager so the explicit choice here wins.
    RegDeleteKeyValueW(HKEY_CURRENT_USER, kApprovedKey, kValueName);
    return WriteRunValue(command);
}

bool RefreshPath() {
    const std::optional<std::wstring> registered = ReadRunValue();
    const std::wstring command = CurrentCommand();
    if (!registered || command.empty() || EqualsIgnoreCase(*registered, command)) {
        return false;
    }
    // Only follow a moved EXE; a different copy (e.g. a debug build) must not take over the entry.
    std::wstring registeredPath = *registered;
    if (registeredPath.size() >= 2 && registeredPath.front() == L'"' && registeredPath.back() == L'"') {
        registeredPath = registeredPath.substr(1, registeredPath.size() - 2);
    }
    if (GetFileAttributesW(registeredPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    return WriteRunValue(command);
}

}  // namespace dimmer::startup
