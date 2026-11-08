#include "core/update_check.h"

#include <cstddef>
#include <string>

#include <nlohmann/json.hpp>

namespace dimmer {
namespace {

using Json = nlohmann::json;

// Stream the response and bound nesting instead of building a DOM from network data.
class ReleaseTagParser final : public nlohmann::json_sax<Json> {
public:
    bool null() override { return Scalar(); }
    bool boolean(bool) override { return Scalar(); }
    bool number_integer(number_integer_t) override { return Scalar(); }
    bool number_unsigned(number_unsigned_t) override { return Scalar(); }
    bool number_float(number_float_t, const string_t&) override { return Scalar(); }
    bool binary(binary_t&) override { return Scalar(); }

    bool string(string_t& value) override {
        if (depth_ == 1 && awaitingTag_) {
            awaitingTag_ = false;
            version_ = ParseVersion(value);
            return version_.has_value();
        }
        return Scalar();
    }

    bool start_object(std::size_t) override {
        if (depth_ == 0) {
            if (started_) {
                return false;
            }
            started_ = true;
        } else if (awaitingTag_) {
            return false;
        }
        return EnterContainer();
    }

    bool start_array(std::size_t) override {
        if (!started_ || awaitingTag_) {
            return false;
        }
        return EnterContainer();
    }

    bool key(string_t& value) override {
        if (depth_ == 1 && value == "tag_name") {
            if (seenTag_) {
                return false;
            }
            seenTag_ = true;
            awaitingTag_ = true;
        }
        return true;
    }

    bool end_object() override { return LeaveContainer(); }
    bool end_array() override { return LeaveContainer(); }
    bool parse_error(std::size_t, const std::string&, const Json::exception&) override { return false; }

    std::optional<Version> VersionValue() const noexcept { return version_; }

private:
    bool Scalar() const noexcept { return started_ && !awaitingTag_; }

    bool EnterContainer() noexcept {
        constexpr unsigned kMaxDepth = 32;
        if (depth_ >= kMaxDepth) {
            return false;
        }
        ++depth_;
        return true;
    }

    bool LeaveContainer() noexcept {
        if (depth_ == 0) {
            return false;
        }
        --depth_;
        return true;
    }

    unsigned depth_ = 0;
    bool started_ = false;
    bool seenTag_ = false;
    bool awaitingTag_ = false;
    std::optional<Version> version_;
};

}  // namespace

std::optional<Version> ParseLatestReleaseTag(std::string_view json) {
    ReleaseTagParser parser;
    if (!Json::sax_parse(json.begin(), json.end(), &parser)) {
        return std::nullopt;
    }
    return parser.VersionValue();
}

VersionRelation CompareVersions(const Version& current, const Version& latest) noexcept {
    if (current < latest) {
        return VersionRelation::UpdateAvailable;
    }
    if (current > latest) {
        return VersionRelation::NewerThanRelease;
    }
    return VersionRelation::UpToDate;
}

}  // namespace dimmer
