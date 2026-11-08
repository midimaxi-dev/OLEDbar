#pragma once

#include <windows.h>
#include <winhttp.h>

#include <cstddef>
#include <string>
#include <string_view>

namespace dimmer {

enum class HttpError { None, InvalidInput, WinHttp, HttpStatus, ResponseTooLarge };

struct HttpGetResult {
    HttpError error = HttpError::None;
    DWORD systemError = ERROR_SUCCESS;
    DWORD statusCode = 0;
    std::string body;
};

// HTTPS only; redirects are not followed. Call on a worker thread.
HttpGetResult HttpsGet(std::wstring_view host, std::wstring_view path, std::wstring_view userAgent);

namespace detail {

inline constexpr std::size_t kMaxHttpResponseBytes = 64 * 1024;
HINTERNET OpenWinHttpSession(const wchar_t* userAgent) noexcept;
bool AppendHttpResponse(std::string& body, std::string_view chunk);

}  // namespace detail
}  // namespace dimmer
