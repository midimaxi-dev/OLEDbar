#pragma once

#include <format>
#include <string>
#include <string_view>
#include <utility>

namespace dimmer::log {

// Writes to %LOCALAPPDATA%\OLEDbar\logs\oledbar.log (truncated on start, rotated to oledbar.1.log at 5 MB)
// and OutputDebugString.
void Init();
void Shutdown();
void Write(std::wstring_view message);
const std::wstring& Path();
bool IsDebugEnabled() noexcept;
void SetDebugEnabled(bool enabled) noexcept;

template <class... Args>
void Info(std::wformat_string<Args...> format, Args&&... args) {
    Write(std::format(format, std::forward<Args>(args)...));
}

// High-volume diagnostics (foreground changes, hover); written only when enabled.
template <class... Args>
void Debug(std::wformat_string<Args...> format, Args&&... args) {
    if (IsDebugEnabled()) {
        Write(std::format(format, std::forward<Args>(args)...));
    }
}

}  // namespace dimmer::log
