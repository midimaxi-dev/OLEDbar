#pragma once

#include <compare>
#include <optional>
#include <string>
#include <string_view>

namespace dimmer {

struct Version {
    int major = 0;
    int minor = 0;
    int patch = 0;

    auto operator<=>(const Version&) const = default;
};

// Accepts stable SemVer core versions, optionally prefixed with 'v'.
std::optional<Version> ParseVersion(std::string_view text) noexcept;
std::string FormatVersion(const Version& version);

}  // namespace dimmer
