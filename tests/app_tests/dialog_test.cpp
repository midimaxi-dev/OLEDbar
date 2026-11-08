#include <gtest/gtest.h>

#include <windows.h>
#include <commctrl.h>

#include <array>
#include <string>

#include "app/about_dialog.h"
#include "app/resource.h"
#include "app/settings_dialog.h"
#include "version.h"

using dimmer::AboutDialog;
using dimmer::Settings;
using dimmer::SettingsDialog;
using dimmer::UpdateCheckResult;
using dimmer::UpdateOutcome;
using dimmer::Version;

namespace {

std::wstring Text(HWND dialog, int id) {
    std::array<wchar_t, 512> buffer{};
    const int length = GetDlgItemTextW(dialog, id, buffer.data(), static_cast<int>(buffer.size()));
    return std::wstring(buffer.data(), length > 0 ? static_cast<size_t>(length) : 0);
}

bool HasVisibleStyle(HWND hwnd) {
    return (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_VISIBLE) != 0;
}

bool HasLink(HWND hwnd, int index, const wchar_t* expectedId) {
    LITEM item{};
    item.mask = LIF_ITEMINDEX | LIF_ITEMID;
    item.iLink = index;
    return SendMessageW(hwnd, LM_GETITEM, 0, reinterpret_cast<LPARAM>(&item)) != FALSE &&
           std::wstring(item.szID) == expectedId;
}

class DialogTest : public testing::Test {
protected:
    void SetUp() override {
        INITCOMMONCONTROLSEX init{sizeof(init),
                                  ICC_BAR_CLASSES | ICC_UPDOWN_CLASS | ICC_HOTKEY_CLASS | ICC_LINK_CLASS};
        ASSERT_TRUE(InitCommonControlsEx(&init));
    }
};

TEST_F(DialogTest, AboutShowsCompiledVersionAndFixedLinks) {
    AboutDialog dialog;
    ASSERT_TRUE(dialog.Show(GetModuleHandleW(nullptr), nullptr, false, {}, [] {}));
    EXPECT_EQ(Text(dialog.Hwnd(), IDC_ABOUT_VERSION), L"OLEDbar " OLEDBAR_VERSION_WSTRING);
    EXPECT_EQ(Text(dialog.Hwnd(), IDC_UPDATE_STATUS), L"Check manually for the latest stable release.");
    EXPECT_TRUE(IsWindowEnabled(GetDlgItem(dialog.Hwnd(), IDC_CHECK_UPDATES)));
    EXPECT_FALSE(HasVisibleStyle(GetDlgItem(dialog.Hwnd(), IDC_OPEN_RELEASE)));
    const HWND links = GetDlgItem(dialog.Hwnd(), IDC_ABOUT_LINKS);
    ASSERT_NE(links, nullptr);
    EXPECT_TRUE(HasLink(links, 0, L"website"));
    EXPECT_TRUE(HasLink(links, 1, L"github"));
    EXPECT_TRUE(HasLink(links, 2, L"releases"));
    dialog.Close();
    EXPECT_EQ(dialog.Hwnd(), nullptr);
}

TEST_F(DialogTest, AboutDisablesCheckingAndDisplaysAvailableRelease) {
    int clicks = 0;
    AboutDialog dialog;
    ASSERT_TRUE(dialog.Show(GetModuleHandleW(nullptr), nullptr, false, {}, [&] { ++clicks; }));
    SendMessageW(dialog.Hwnd(), WM_COMMAND, MAKEWPARAM(IDC_CHECK_UPDATES, BN_CLICKED), 0);
    EXPECT_EQ(clicks, 1);
    dialog.SetChecking();
    EXPECT_EQ(Text(dialog.Hwnd(), IDC_UPDATE_STATUS), L"Checking for updates...");
    EXPECT_FALSE(IsWindowEnabled(GetDlgItem(dialog.Hwnd(), IDC_CHECK_UPDATES)));
    SendMessageW(dialog.Hwnd(), WM_COMMAND, MAKEWPARAM(IDC_CHECK_UPDATES, BN_CLICKED), 0);
    EXPECT_EQ(clicks, 1);

    dialog.SetResult(UpdateCheckResult{.outcome = UpdateOutcome::Available, .latest = Version{1, 2, 3}});
    EXPECT_EQ(Text(dialog.Hwnd(), IDC_UPDATE_STATUS), L"OLEDbar 1.2.3 is available.");
    EXPECT_TRUE(IsWindowEnabled(GetDlgItem(dialog.Hwnd(), IDC_CHECK_UPDATES)));
    EXPECT_TRUE(HasVisibleStyle(GetDlgItem(dialog.Hwnd(), IDC_OPEN_RELEASE)));
    dialog.SetResult(UpdateCheckResult{.outcome = UpdateOutcome::UpToDate, .latest = Version{1, 2, 3}});
    EXPECT_EQ(Text(dialog.Hwnd(), IDC_UPDATE_STATUS), L"You have the latest version.");
    EXPECT_FALSE(HasVisibleStyle(GetDlgItem(dialog.Hwnd(), IDC_OPEN_RELEASE)));
}

TEST_F(DialogTest, AboutReportsFailuresWithoutMislabelingEvery403AsRateLimit) {
    AboutDialog dialog;
    ASSERT_TRUE(dialog.Show(GetModuleHandleW(nullptr), nullptr, false, {}, [] {}));
    dialog.SetResult(UpdateCheckResult{.outcome = UpdateOutcome::ConnectionError});
    EXPECT_NE(Text(dialog.Hwnd(), IDC_UPDATE_STATUS).find(L"connection"), std::wstring::npos);
    dialog.SetResult(UpdateCheckResult{.outcome = UpdateOutcome::HttpError, .httpStatus = 403});
    EXPECT_NE(Text(dialog.Hwnd(), IDC_UPDATE_STATUS).find(L"may be rate-limited"), std::wstring::npos);
    dialog.SetResult(UpdateCheckResult{.outcome = UpdateOutcome::HttpError, .httpStatus = 429});
    EXPECT_NE(Text(dialog.Hwnd(), IDC_UPDATE_STATUS).find(L"rate limit"), std::wstring::npos);
    dialog.SetResult(UpdateCheckResult{.outcome = UpdateOutcome::InvalidRelease});
    EXPECT_NE(Text(dialog.Hwnd(), IDC_UPDATE_STATUS).find(L"unrecognized"), std::wstring::npos);
    dialog.SetResult(UpdateCheckResult{.outcome = UpdateOutcome::ResponseTooLarge});
    EXPECT_NE(Text(dialog.Hwnd(), IDC_UPDATE_STATUS).find(L"too large"), std::wstring::npos);
    EXPECT_FALSE(HasVisibleStyle(GetDlgItem(dialog.Hwnd(), IDC_OPEN_RELEASE)));
}

TEST_F(DialogTest, ReopeningAboutUsesOneWindowAndRestoresPendingState) {
    AboutDialog dialog;
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    ASSERT_TRUE(dialog.Show(instance, nullptr, false, {}, [] {}));
    const HWND first = dialog.Hwnd();
    ASSERT_TRUE(dialog.Show(instance, nullptr, true, {}, [] {}));
    EXPECT_EQ(dialog.Hwnd(), first);
    EXPECT_EQ(Text(dialog.Hwnd(), IDC_UPDATE_STATUS), L"Checking for updates...");
    dialog.Close();
    ASSERT_TRUE(dialog.Show(instance, nullptr, false,
                            UpdateCheckResult{.outcome = UpdateOutcome::NewerThanRelease,
                                              .latest = Version{1, 0, 0}},
                            [] {}));
    EXPECT_NE(Text(dialog.Hwnd(), IDC_UPDATE_STATUS).find(L"newer"), std::wstring::npos);
}

TEST_F(DialogTest, SettingsAboutLinkHandlesMouseAndKeyboardNotifications) {
    int clicks = 0;
    SettingsDialog dialog;
    const bool shown = dialog.Show(GetModuleHandleW(nullptr), Settings{}, false,
                                   SettingsDialog::Callbacks{.showAbout = [&] { ++clicks; }});
    ASSERT_TRUE(shown);
    const HWND settings = dialog.Hwnd();
    EXPECT_NE(Text(settings, IDC_SETTINGS_ABOUT).find(L"OLEDbar " OLEDBAR_VERSION_WSTRING), std::wstring::npos);
    const HWND link = GetDlgItem(settings, IDC_SETTINGS_ABOUT);
    ASSERT_NE(link, nullptr);
    EXPECT_TRUE(HasLink(link, 0, L"about"));

    NMLINK notification{};
    notification.hdr.hwndFrom = link;
    notification.hdr.idFrom = IDC_SETTINGS_ABOUT;
    notification.hdr.code = NM_CLICK;
    SendMessageW(settings, WM_NOTIFY, IDC_SETTINGS_ABOUT, reinterpret_cast<LPARAM>(&notification));
    notification.hdr.code = NM_RETURN;
    SendMessageW(settings, WM_NOTIFY, IDC_SETTINGS_ABOUT, reinterpret_cast<LPARAM>(&notification));
    EXPECT_EQ(clicks, 2);
    dialog.Close();
}

}  // namespace
