#pragma once

#include <windows.h>

#include <optional>
#include <string>
#include <string_view>

#include <wil/resource.h>

namespace dimmer {

using unique_wineventhook = wil::unique_any<HWINEVENTHOOK, decltype(&::UnhookWinEvent), ::UnhookWinEvent>;
using unique_menu = wil::unique_any<HMENU, decltype(&::DestroyMenu), ::DestroyMenu>;

// %LOCALAPPDATA%\OLEDbar (not created). Empty if LOCALAPPDATA is unavailable.
std::wstring AppDataDirectory();
std::optional<std::string> ReadTextFile(const std::wstring& path);
// Writes to a temporary file first, then atomically replaces `path`.
bool WriteTextFileAtomic(const std::wstring& path, std::string_view content);
std::wstring Widen(std::string_view utf8);

std::wstring ToString(const RECT& rect);
// Executable file name of the window's process, e.g. L"explorer.exe". Empty on failure.
std::wstring ProcessImageName(HWND hwnd);
std::wstring WindowClassName(HWND hwnd);
bool EqualsIgnoreCase(std::wstring_view a, std::wstring_view b) noexcept;

}  // namespace dimmer
