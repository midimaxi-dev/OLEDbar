#include <gtest/gtest.h>

#include <windows.h>
#include <winhttp.h>

#include <array>
#include <string>

#include <wil/resource.h>

#include "core/update_check.h"
#include "platform/http_client.h"

using dimmer::HttpError;
using dimmer::HttpsGet;

namespace {

TEST(HttpClientTest, RejectsInvalidInputsBeforeSending) {
    EXPECT_EQ(HttpsGet(L"", L"/", L"OLEDbar/test").error, HttpError::InvalidInput);
    EXPECT_EQ(HttpsGet(L"api.github.com:443", L"/", L"OLEDbar/test").error, HttpError::InvalidInput);
    EXPECT_EQ(HttpsGet(L"api.github.com", L"https://example.org/", L"OLEDbar/test").error, HttpError::InvalidInput);
    EXPECT_EQ(HttpsGet(L"api.github.com", L"/\r\nInjected: value", L"OLEDbar/test").error, HttpError::InvalidInput);
    EXPECT_EQ(HttpsGet(L"api.github.com", L"/", L"OLEDbar\r\nInjected: value").error, HttpError::InvalidInput);
}

TEST(HttpClientTest, BoundsResponseAcrossChunks) {
    using dimmer::detail::AppendHttpResponse;
    using dimmer::detail::kMaxHttpResponseBytes;
    std::string body;
    EXPECT_TRUE(AppendHttpResponse(body, std::string(kMaxHttpResponseBytes - 1, 'a')));
    EXPECT_TRUE(AppendHttpResponse(body, "b"));
    EXPECT_EQ(body.size(), kMaxHttpResponseBytes);
    EXPECT_FALSE(AppendHttpResponse(body, "c"));
    EXPECT_EQ(body.size(), kMaxHttpResponseBytes);
    EXPECT_TRUE(AppendHttpResponse(body, ""));

    std::string empty;
    EXPECT_FALSE(AppendHttpResponse(empty, std::string(kMaxHttpResponseBytes + 1, 'x')));
    EXPECT_TRUE(empty.empty());
}

TEST(HttpClientTest, LoadsSystemWinHttpDespiteAnInjectedSearchDirectory) {
    ASSERT_EQ(GetModuleHandleW(L"winhttp.dll"), nullptr);

    std::array<wchar_t, MAX_PATH> tempPath{};
    const DWORD tempLength = GetTempPathW(static_cast<DWORD>(tempPath.size()), tempPath.data());
    ASSERT_GT(tempLength, 0u);
    ASSERT_LT(tempLength, tempPath.size());
    std::array<wchar_t, MAX_PATH> tempName{};
    ASSERT_NE(GetTempFileNameW(tempPath.data(), L"obh", 0, tempName.data()), 0u);
    ASSERT_TRUE(DeleteFileW(tempName.data()));
    ASSERT_TRUE(CreateDirectoryW(tempName.data(), nullptr));

    const std::wstring directory(tempName.data());
    const std::wstring fakeDll = directory + L"\\winhttp.dll";
    bool copied = false;
    bool searchChanged = false;
    std::array<wchar_t, MAX_PATH> previousSearch{};
    const DWORD previousLength = GetDllDirectoryW(static_cast<DWORD>(previousSearch.size()), previousSearch.data());
    const auto cleanup = wil::scope_exit([&] {
        if (searchChanged) {
            EXPECT_TRUE(SetDllDirectoryW(previousLength ? previousSearch.data() : nullptr));
        }
        if (copied) {
            EXPECT_TRUE(DeleteFileW(fakeDll.c_str()));
        }
        EXPECT_TRUE(RemoveDirectoryW(directory.c_str()));
    });
    ASSERT_LT(previousLength, previousSearch.size());

    std::array<wchar_t, MAX_PATH> systemPath{};
    const UINT systemLength = GetSystemDirectoryW(systemPath.data(), static_cast<UINT>(systemPath.size()));
    ASSERT_GT(systemLength, 0u);
    ASSERT_LT(systemLength, systemPath.size());
    const std::wstring systemDirectory(systemPath.data(), systemLength);
    const std::wstring systemDll = systemDirectory + L"\\winhttp.dll";
    ASSERT_TRUE(CopyFileW((systemDirectory + L"\\version.dll").c_str(), fakeDll.c_str(), TRUE));
    copied = true;
    ASSERT_TRUE(SetDllDirectoryW(directory.c_str()));
    searchChanged = true;

    wil::unique_winhttp_hinternet session(dimmer::detail::OpenWinHttpSession(L"OLEDbar-test/0.0.0"));
    ASSERT_TRUE(session) << GetLastError();
    const HMODULE module = GetModuleHandleW(L"winhttp.dll");
    ASSERT_NE(module, nullptr);
    std::array<wchar_t, MAX_PATH> loadedPath{};
    const DWORD loadedLength = GetModuleFileNameW(module, loadedPath.data(), static_cast<DWORD>(loadedPath.size()));
    ASSERT_GT(loadedLength, 0u);
    ASSERT_LT(loadedLength, loadedPath.size());
    EXPECT_EQ(_wcsicmp(loadedPath.data(), systemDll.c_str()), 0);
}

// Run explicitly when network access is available; ordinary builds and CI stay offline.
TEST(HttpClientTest, DISABLED_FetchesGitHubLatestRelease) {
    const auto response = HttpsGet(L"api.github.com", L"/repos/midimaxi-dev/OLEDbar/releases/latest",
                                   L"OLEDbar-test/0.0.0");
    ASSERT_EQ(response.error, HttpError::None) << response.systemError << " HTTP " << response.statusCode;
    EXPECT_EQ(response.statusCode, 200u);
    EXPECT_TRUE(dimmer::ParseLatestReleaseTag(response.body));
}

TEST(HttpClientTest, DISABLED_ReportsGitHubHttpFailure) {
    const auto response = HttpsGet(L"api.github.com",
                                   L"/repos/midimaxi-dev/OLEDbar/releases/tags/not-a-real-release",
                                   L"OLEDbar-test/0.0.0");
    EXPECT_EQ(response.error, HttpError::HttpStatus);
    EXPECT_EQ(response.statusCode, 404u);
    EXPECT_TRUE(response.body.empty());
}

TEST(HttpClientTest, DISABLED_DoesNotFollowRedirects) {
    const auto response = HttpsGet(L"github.com", L"/midimaxi-dev/OLEDbar/releases/latest", L"OLEDbar-test/0.0.0");
    EXPECT_EQ(response.error, HttpError::HttpStatus);
    EXPECT_GE(response.statusCode, 300u);
    EXPECT_LT(response.statusCode, 400u);
    EXPECT_TRUE(response.body.empty());
}

}  // namespace
