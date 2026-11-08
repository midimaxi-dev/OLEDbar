#include "platform/http_client.h"

#include <winhttp.h>

#include <algorithm>
#include <array>
#include <iterator>

#include <wil/resource.h>

namespace dimmer {
namespace {

bool IsHostCharacter(wchar_t c) noexcept {
    return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9') || c == L'.' ||
           c == L'-';
}

bool IsPrintableAscii(wchar_t c) noexcept {
    return c >= L' ' && c <= L'~';
}

HttpGetResult WinHttpFailure() {
    return {HttpError::WinHttp, GetLastError()};
}

}  // namespace

HINTERNET detail::OpenWinHttpSession(const wchar_t* userAgent) noexcept {
    // Pin the system DLL before resolving delay-loaded imports.
    wil::unique_hmodule winhttp(LoadLibraryExW(L"winhttp.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32));
    if (!winhttp) {
        return nullptr;
    }
    HINTERNET session = WinHttpOpen(userAgent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                                   WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        const DWORD error = GetLastError();
        winhttp.reset();
        SetLastError(error);
    }
    return session;
}

bool detail::AppendHttpResponse(std::string& body, std::string_view chunk) {
    if (body.size() > kMaxHttpResponseBytes || chunk.size() > kMaxHttpResponseBytes - body.size()) {
        return false;
    }
    body.append(chunk);
    return true;
}

HttpGetResult HttpsGet(std::wstring_view host, std::wstring_view path, std::wstring_view userAgent) {
    if (host.empty() || !std::ranges::all_of(host, IsHostCharacter) || path.empty() || path.front() != L'/' ||
        !std::ranges::all_of(path, IsPrintableAscii) || userAgent.empty() ||
        !std::ranges::all_of(userAgent, IsPrintableAscii)) {
        return {HttpError::InvalidInput, ERROR_INVALID_PARAMETER};
    }

    const std::wstring hostText(host);
    const std::wstring pathText(path);
    const std::wstring agentText(userAgent);
    wil::unique_winhttp_hinternet session(detail::OpenWinHttpSession(agentText.c_str()));
    if (!session) {
        return WinHttpFailure();
    }
    if (!WinHttpSetTimeouts(session.get(), 5000, 5000, 5000, 10000)) {
        return WinHttpFailure();
    }

    wil::unique_winhttp_hinternet connection(WinHttpConnect(session.get(), hostText.c_str(),
                                                             INTERNET_DEFAULT_HTTPS_PORT, 0));
    if (!connection) {
        return WinHttpFailure();
    }
    wil::unique_winhttp_hinternet request(
        WinHttpOpenRequest(connection.get(), L"GET", pathText.c_str(), nullptr, WINHTTP_NO_REFERER,
                           WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
    if (!request) {
        return WinHttpFailure();
    }

    DWORD disabled = WINHTTP_DISABLE_REDIRECTS | WINHTTP_DISABLE_COOKIES;
    if (!WinHttpSetOption(request.get(), WINHTTP_OPTION_DISABLE_FEATURE, &disabled, sizeof(disabled))) {
        return WinHttpFailure();
    }
    constexpr wchar_t kHeaders[] = L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n";
    if (!WinHttpSendRequest(request.get(), kHeaders, static_cast<DWORD>(std::size(kHeaders) - 1),
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request.get(), nullptr)) {
        return WinHttpFailure();
    }

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    if (!WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX)) {
        return WinHttpFailure();
    }
    if (status != 200) {
        return {HttpError::HttpStatus, ERROR_SUCCESS, status};
    }

    HttpGetResult result;
    result.statusCode = status;
    std::array<char, 4096> buffer{};
    for (;;) {
        DWORD read = 0;
        if (!WinHttpReadData(request.get(), buffer.data(), static_cast<DWORD>(buffer.size()), &read)) {
            return WinHttpFailure();
        }
        if (read == 0) {
            return result;
        }
        if (!detail::AppendHttpResponse(result.body, std::string_view(buffer.data(), read))) {
            return {HttpError::ResponseTooLarge, ERROR_SUCCESS, status};
        }
    }
}

}  // namespace dimmer
