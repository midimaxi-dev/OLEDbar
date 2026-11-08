#include "core/version.h"

#include <charconv>
#include <format>
#include <system_error>

namespace dimmer {
namespace {

bool ParseComponent(std::string_view text, int& value) noexcept {
    if (text.empty() || (text.size() > 1 && text.front() == '0')) {
        return false;
    }
    for (const char digit : text) {
        if (digit < '0' || digit > '9') {
            return false;
        }
    }
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

}  // namespace

std::optional<Version> ParseVersion(std::string_view text) noexcept {
    if (!text.empty() && text.front() == 'v') {
        text.remove_prefix(1);
    }
    const auto firstDot = text.find('.');
    if (firstDot == std::string_view::npos) {
        return std::nullopt;
    }
    const auto secondDot = text.find('.', firstDot + 1);
    if (secondDot == std::string_view::npos) {
        return std::nullopt;
    }
    Version version;
    if (!ParseComponent(text.substr(0, firstDot), version.major) ||
        !ParseComponent(text.substr(firstDot + 1, secondDot - firstDot - 1), version.minor) ||
        !ParseComponent(text.substr(secondDot + 1), version.patch)) {
        return std::nullopt;
    }
    return version;
}

std::string FormatVersion(const Version& version) {
    return std::format("{}.{}.{}", version.major, version.minor, version.patch);
}

}  // namespace dimmer
