#include "platform/log.h"

#include <windows.h>
#include <share.h>

#include <cstdio>
#include <filesystem>
#include <system_error>

#include "platform/win_util.h"

namespace dimmer::log {
namespace {

FILE* g_file = nullptr;
std::wstring g_path;
std::wstring g_previousPath;
size_t g_bytesWritten = 0;
bool g_debug = false;

constexpr size_t kMaxLogBytes = 5 * 1024 * 1024;

void Open() {
    // Deny only writers so the log can be read while OLEDbar is running.
    g_file = _wfsopen(g_path.c_str(), L"w, ccs=UTF-8", _SH_DENYWR);
    g_bytesWritten = 0;
}

void Rotate() {
    fclose(g_file);
    MoveFileExW(g_path.c_str(), g_previousPath.c_str(), MOVEFILE_REPLACE_EXISTING);
    Open();
}

}  // namespace

void Init() {
    const std::wstring base = AppDataDirectory();
    if (base.empty()) {
        return;
    }
    const std::filesystem::path directory = std::filesystem::path(base) / L"logs";
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    g_path = (directory / L"oledbar.log").wstring();
    g_previousPath = (directory / L"oledbar.1.log").wstring();
    Open();
}

void Shutdown() {
    if (g_file) {
        fclose(g_file);
        g_file = nullptr;
    }
}

const std::wstring& Path() {
    return g_path;
}

bool IsDebugEnabled() noexcept {
    return g_debug;
}

void SetDebugEnabled(bool enabled) noexcept {
    g_debug = enabled;
}

void Write(std::wstring_view message) {
    SYSTEMTIME time{};
    GetLocalTime(&time);
    const std::wstring line = std::format(L"{:02}:{:02}:{:02}.{:03} {}\n", time.wHour, time.wMinute,
                                          time.wSecond, time.wMilliseconds, message);
    OutputDebugStringW(line.c_str());
    if (g_file) {
        fputws(line.c_str(), g_file);
        fflush(g_file);
        g_bytesWritten += line.size();
        if (g_bytesWritten > kMaxLogBytes) {
            Rotate();
        }
    }
}

}  // namespace dimmer::log
