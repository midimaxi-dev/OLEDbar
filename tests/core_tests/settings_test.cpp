#include <gtest/gtest.h>

#include <chrono>

#include "core/settings.h"

using namespace std::chrono_literals;
using dimmer::ParseSettings;
using dimmer::SerializeSettings;
using dimmer::Settings;

namespace {

TEST(SettingsTest, DefaultsRoundTrip) {
    const auto result = ParseSettings(SerializeSettings(Settings{}));
    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(result.warnings.empty());
    EXPECT_EQ(result.settings, Settings{});
}

TEST(SettingsTest, EmptyObjectGivesDefaults) {
    const auto result = ParseSettings("{}");
    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(result.warnings.empty());
    EXPECT_EQ(result.settings, Settings{});
}

TEST(SettingsTest, ParsesValues) {
    const auto result = ParseSettings(R"({
        "enabled": false, "idleTimeoutSeconds": 20, "redimDelaySeconds": 1.5, "dimLevelPercent": 85,
        "fadeToDimMs": 800, "fadeToWakeMs": 0, "wakeOnTaskbarFocus": false, "wakeOnShellUi": false,
        "hideInFullscreen": false, "excludeFromCapture": false })");
    ASSERT_TRUE(result.valid);
    EXPECT_TRUE(result.warnings.empty());
    const Settings& s = result.settings;
    EXPECT_FALSE(s.enabled);
    EXPECT_DOUBLE_EQ(s.idleTimeoutSeconds, 20.0);
    EXPECT_DOUBLE_EQ(s.redimDelaySeconds, 1.5);
    EXPECT_DOUBLE_EQ(s.dimLevelPercent, 85.0);
    EXPECT_EQ(s.fadeToDimMs, 800);
    EXPECT_EQ(s.fadeToWakeMs, 0);
    EXPECT_FALSE(s.wakeOnTaskbarFocus);
    EXPECT_FALSE(s.wakeOnShellUi);
    EXPECT_FALSE(s.hideInFullscreen);
    EXPECT_FALSE(s.excludeFromCapture);
}

TEST(SettingsTest, OutOfRangeIsClampedWithWarning) {
    const auto result = ParseSettings(R"({ "dimLevelPercent": 150, "idleTimeoutSeconds": 0 })");
    ASSERT_TRUE(result.valid);
    EXPECT_EQ(result.warnings.size(), 2u);
    EXPECT_DOUBLE_EQ(result.settings.dimLevelPercent, 100.0);
    EXPECT_DOUBLE_EQ(result.settings.idleTimeoutSeconds, 1.0);
}

TEST(SettingsTest, WrongTypeKeepsDefaultWithWarning) {
    const auto result = ParseSettings(R"({ "enabled": "yes", "dimLevelPercent": "high" })");
    ASSERT_TRUE(result.valid);
    EXPECT_EQ(result.warnings.size(), 2u);
    EXPECT_EQ(result.settings, Settings{});
}

TEST(SettingsTest, UnknownKeyWarns) {
    const auto result = ParseSettings(R"({ "dimLevelPercnt": 50 })");
    ASSERT_TRUE(result.valid);
    ASSERT_EQ(result.warnings.size(), 1u);
    EXPECT_NE(result.warnings[0].find("dimLevelPercnt"), std::string::npos);
}

TEST(SettingsTest, LogLevelParses) {
    EXPECT_TRUE(ParseSettings(R"({ "logLevel": "debug" })").settings.debugLogging);
    EXPECT_FALSE(ParseSettings(R"({ "logLevel": "info" })").settings.debugLogging);

    const auto invalid = ParseSettings(R"({ "logLevel": "verbose" })");
    EXPECT_FALSE(invalid.settings.debugLogging);
    EXPECT_EQ(invalid.warnings.size(), 1u);
}

TEST(SettingsTest, CommentsAreAllowed) {
    const auto result = ParseSettings("{\n  // dim harder\n  \"dimLevelPercent\": 90\n}");
    ASSERT_TRUE(result.valid);
    EXPECT_DOUBLE_EQ(result.settings.dimLevelPercent, 90.0);
}

TEST(SettingsTest, InvalidJsonIsReported) {
    const auto result = ParseSettings(R"({ "dimLevelPercent": 90, })");
    EXPECT_FALSE(result.valid);
    EXPECT_FALSE(result.warnings.empty());
    EXPECT_EQ(result.settings, Settings{});
}

TEST(SettingsTest, NonObjectIsInvalid) {
    EXPECT_FALSE(ParseSettings("[]").valid);
    EXPECT_FALSE(ParseSettings("").valid);
}

TEST(SettingsTest, ToPolicyConvertsUnits) {
    Settings s;
    s.idleTimeoutSeconds = 12.5;
    s.redimDelaySeconds = 2;
    s.dimLevelPercent = 40;
    s.fadeToDimMs = 900;
    s.fadeToWakeMs = 50;
    const auto policy = dimmer::ToPolicy(s);
    EXPECT_EQ(policy.idleTimeout, 12500ms);
    EXPECT_EQ(policy.redimDelay, 2000ms);
    EXPECT_DOUBLE_EQ(policy.dimLevel, 0.4);
    EXPECT_EQ(policy.fadeToDim, 900ms);
    EXPECT_EQ(policy.fadeToWake, 50ms);
}

}  // namespace
