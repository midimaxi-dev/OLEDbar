#include <gtest/gtest.h>

#include <limits>
#include <string>

#include "core/version.h"

using dimmer::FormatVersion;
using dimmer::ParseVersion;
using dimmer::Version;

namespace {

TEST(VersionTest, ParsesStableVersionsWithOptionalPrefix) {
    EXPECT_EQ(ParseVersion("v0.3.0"), (Version{0, 3, 0}));
    EXPECT_EQ(ParseVersion("12.34.56"), (Version{12, 34, 56}));
    EXPECT_EQ(ParseVersion("0.0.0"), (Version{0, 0, 0}));
}

TEST(VersionTest, RejectsMalformedAndUnstableVersions) {
    for (const char* text : {"", "v", "V1.2.3", "1", "1.2", "1.2.3.4", ".1.2", "1..2", "1.2.",
                             "v1.2.3-rc1", "1.2.3+build", "-1.2.3", "+1.2.3", "1.2.a", " 1.2.3",
                             "1.2.3 ", "01.2.3", "1.02.3", "1.2.03"}) {
        EXPECT_FALSE(ParseVersion(text)) << text;
    }
}

TEST(VersionTest, RejectsOverflowInEveryComponent) {
    const std::string max = std::to_string((std::numeric_limits<int>::max)());
    const std::string over = std::to_string(static_cast<long long>((std::numeric_limits<int>::max)()) + 1);
    EXPECT_TRUE(ParseVersion(max + "." + max + "." + max));
    EXPECT_FALSE(ParseVersion(over + ".0.0"));
    EXPECT_FALSE(ParseVersion("0." + over + ".0"));
    EXPECT_FALSE(ParseVersion("0.0." + over));
    EXPECT_FALSE(ParseVersion("99999999999999999999.0.0"));
    EXPECT_FALSE(ParseVersion("0.99999999999999999999.0"));
    EXPECT_FALSE(ParseVersion("0.0.99999999999999999999"));
}

TEST(VersionTest, OrdersComponentsNumerically) {
    EXPECT_LT((Version{0, 9, 9}), (Version{1, 0, 0}));
    EXPECT_LT((Version{1, 2, 99}), (Version{1, 10, 0}));
    EXPECT_LT((Version{1, 2, 9}), (Version{1, 2, 10}));
    EXPECT_EQ((Version{1, 2, 3}), (Version{1, 2, 3}));
}

TEST(VersionTest, FormatsAndRoundTrips) {
    const Version version{12, 34, 56};
    EXPECT_EQ(FormatVersion(version), "12.34.56");
    EXPECT_EQ(ParseVersion(FormatVersion(version)), version);
}

}  // namespace
