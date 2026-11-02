#include "platform/win_util.h"

#include <algorithm>
#include <format>

namespace dimmer {
namespace {

constexpr LONGLONG kMaxTextFileBytes = 1024 * 1024;

}  // namespace

std::wstring AppDataDirectory() {
    wchar_t buffer[MAX_PATH]{};
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, static_cast<DWORD>(ARRAYSIZE(buffer)));
    if (length == 0 || length >= ARRAYSIZE(buffer)) {
        return {};
    }
    return std::wstring(buffer, length) + L"\\OLEDbar";
}

std::optional<std::string> ReadTextFile(const std::wstring& path) {
    wil::unique_hfile file(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                       nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file) {
        return std::nullopt;
    }
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file.get(), &size) || size.QuadPart > kMaxTextFileBytes) {
        return std::nullopt;
    }
    std::string content(static_cast<size_t>(size.QuadPart), '\0');
    DWORD read = 0;
    if (!content.empty() &&
        !::ReadFile(file.get(), content.data(), static_cast<DWORD>(content.size()), &read, nullptr)) {
        return std::nullopt;
    }
    content.resize(read);
    return content;
}

bool WriteTextFileAtomic(const std::wstring& path, std::string_view content) {
    const std::wstring temp = path + L".tmp";
    {
        wil::unique_hfile file(
            CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (!file) {
            return false;
        }
        DWORD written = 0;
        if (!WriteFile(file.get(), content.data(), static_cast<DWORD>(content.size()), &written, nullptr) ||
            written != content.size()) {
            return false;
        }
        FlushFileBuffers(file.get());
    }
    return MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
}

std::wstring Widen(std::string_view utf8) {
    if (utf8.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (length <= 0) {
        return {};
    }
    std::wstring result(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), result.data(), length);
    return result;
}

std::wstring ToString(const RECT& rect) {
    return std::format(L"({},{})-({},{}) {}x{}", rect.left, rect.top, rect.right, rect.bottom,
                       rect.right - rect.left, rect.bottom - rect.top);
}

std::wstring ProcessImageName(HWND hwnd) {
    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId == 0) {
        return {};
    }
    wil::unique_handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId));
    if (!process) {
        return {};
    }
    wchar_t path[MAX_PATH * 2]{};
    DWORD size = static_cast<DWORD>(ARRAYSIZE(path));
    if (!QueryFullProcessImageNameW(process.get(), 0, path, &size)) {
        return {};
    }
    const std::wstring_view full(path, size);
    const auto slash = full.find_last_of(L'\\');
    return std::wstring(slash == std::wstring_view::npos ? full : full.substr(slash + 1));
}

std::wstring WindowClassName(HWND hwnd) {
    wchar_t buffer[256]{};
    const int length = GetClassNameW(hwnd, buffer, static_cast<int>(ARRAYSIZE(buffer)));
    return std::wstring(buffer, length > 0 ? static_cast<size_t>(length) : 0);
}

bool EqualsIgnoreCase(std::wstring_view a, std::wstring_view b) noexcept {
    return CompareStringOrdinal(a.data(), static_cast<int>(a.size()), b.data(), static_cast<int>(b.size()),
                                TRUE) == CSTR_EQUAL;
}

}  // namespace dimmer
