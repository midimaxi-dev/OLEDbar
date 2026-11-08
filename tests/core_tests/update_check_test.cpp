#include <gtest/gtest.h>

#include <cstddef>
#include <string>

#include "core/update_check.h"

using dimmer::CompareVersions;
using dimmer::ParseLatestReleaseTag;
using dimmer::Version;
using dimmer::VersionRelation;

namespace {

TEST(UpdateCheckTest, ParsesRootReleaseTagAmongOtherFields) {
    EXPECT_EQ(ParseLatestReleaseTag(R"({"name":"OLEDbar","tag_name":"v1.2.3","assets":[{"id":1}]})"),
              (Version{1, 2, 3}));
    EXPECT_EQ(ParseLatestReleaseTag(R"({"tag_name":"1.2.3"})"), (Version{1, 2, 3}));
    EXPECT_EQ(ParseLatestReleaseTag(R"({"nested":{"tag_name":"v9.0.0"},"tag_name":"v0.3.0"})"),
              (Version{0, 3, 0}));
}

TEST(UpdateCheckTest, RejectsMalformedOrIncompleteResponses) {
    for (const char* json : {"", "null", "[]", "123", R"({"tag_name":)", R"({"message":"Not Found"})",
                             R"({"nested":{"tag_name":"v1.0.0"}})", R"({"tag_name":null})",
                             R"({"tag_name":42})", R"({"tag_name":[]})", R"({"tag_name":{}})",
                             R"({"tag_name":"v1.0.0-rc1"})", R"({"tag_name":"v1.0.0"} trailing)",
                             R"({"tag_name":"v1.0.0","tag_name":"v2.0.0"})"}) {
        EXPECT_FALSE(ParseLatestReleaseTag(json)) << json;
    }
}

TEST(UpdateCheckTest, EnforcesNestingLimit) {
    const auto nestedResponse = [](std::size_t arrayDepth) {
        std::string json = R"({"tag_name":"v1.2.3","nested":)";
        json.append(arrayDepth, '[');
        json += "0";
        json.append(arrayDepth, ']');
        json += '}';
        return json;
    };
    EXPECT_EQ(ParseLatestReleaseTag(nestedResponse(31)), (Version{1, 2, 3}));
    EXPECT_FALSE(ParseLatestReleaseTag(nestedResponse(32)));
}

TEST(UpdateCheckTest, ComparesOlderEqualAndNewerVersions) {
    const Version release{1, 2, 3};
    EXPECT_EQ(CompareVersions(Version{1, 2, 2}, release), VersionRelation::UpdateAvailable);
    EXPECT_EQ(CompareVersions(release, release), VersionRelation::UpToDate);
    EXPECT_EQ(CompareVersions(Version{1, 2, 4}, release), VersionRelation::NewerThanRelease);
}

}  // namespace
