#pragma once

#include <optional>
#include <string_view>

#include "core/version.h"

namespace dimmer {

enum class VersionRelation { UpdateAvailable, UpToDate, NewerThanRelease };

// Parses only a root-level tag_name from a GitHub release JSON document.
std::optional<Version> ParseLatestReleaseTag(std::string_view json);
VersionRelation CompareVersions(const Version& current, const Version& latest) noexcept;

}  // namespace dimmer
