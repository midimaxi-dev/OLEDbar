#include "core/settings.h"

#include <algorithm>
#include <cmath>
#include <format>

#include <nlohmann/json.hpp>

namespace dimmer {
namespace {

using Json = nlohmann::json;

constexpr int kSchemaVersion = 1;

constexpr const char* kKnownKeys[] = {
    "version",           "enabled",           "idleTimeoutSeconds", "redimDelaySeconds",
    "dimLevelPercent",   "fadeToDimMs",       "fadeToWakeMs",       "wakeOnTaskbarFocus",
    "wakeOnShellUi",     "hideInFullscreen",  "excludeFromCapture", "logLevel",
    "toggleDesktopIconsHotkey",
};

class Reader {
public:
    Reader(const Json& json, std::vector<std::string>& warnings) : json_(json), warnings_(warnings) {}

    void Bool(const char* key, bool& out) {
        const auto it = json_.find(key);
        if (it == json_.end()) {
            return;
        }
        if (!it->is_boolean()) {
            warnings_.push_back(std::format("'{}' must be true or false; using {}", key, out));
            return;
        }
        out = it->get<bool>();
    }

    void Number(const char* key, double& out, double min, double max) {
        const auto it = json_.find(key);
        if (it == json_.end()) {
            return;
        }
        if (!it->is_number() || !std::isfinite(it->get<double>())) {
            warnings_.push_back(std::format("'{}' must be a number; using {}", key, out));
            return;
        }
        const double value = it->get<double>();
        if (value < min || value > max) {
            out = std::clamp(value, min, max);
            warnings_.push_back(std::format("'{}' must be between {} and {}; using {}", key, min, max, out));
            return;
        }
        out = value;
    }

    void Integer(const char* key, int& out, int min, int max) {
        double value = out;
        Number(key, value, min, max);
        out = static_cast<int>(std::lround(value));
    }

    void LogLevel(const char* key, bool& debug) {
        const auto it = json_.find(key);
        if (it == json_.end()) {
            return;
        }
        if (it->is_string() && (*it == "info" || *it == "debug")) {
            debug = *it == "debug";
            return;
        }
        warnings_.push_back(std::format("'{}' must be \"info\" or \"debug\"; using \"{}\"", key,
                                        debug ? "debug" : "info"));
    }

    void HotkeyValue(const char* key, Hotkey& out) {
        const auto it = json_.find(key);
        if (it == json_.end()) {
            return;
        }
        if (it->is_string()) {
            if (const auto hotkey = ParseHotkey(it->get<std::string>())) {
                out = *hotkey;
                return;
            }
        }
        warnings_.push_back(std::format(
            "'{}' must be a shortcut with Ctrl or Alt, like \"Ctrl+Alt+D\", or \"\" for none; using \"{}\"", key,
            FormatHotkey(out)));
    }

private:
    const Json& json_;
    std::vector<std::string>& warnings_;
};

std::chrono::milliseconds SecondsToMs(double seconds) {
    return std::chrono::milliseconds(std::llround(seconds * 1000.0));
}

}  // namespace

SettingsParseResult ParseSettings(std::string_view text) {
    SettingsParseResult result;
    Json json;
    try {
        json = Json::parse(text.data(), text.data() + text.size(), nullptr, true, true);
    } catch (const Json::parse_error& error) {
        result.warnings.push_back(std::format("Invalid JSON: {}", error.what()));
        return result;
    }
    if (!json.is_object()) {
        result.warnings.push_back("Settings must be a JSON object ({ ... })");
        return result;
    }

    Settings& s = result.settings;
    Reader read(json, result.warnings);
    read.Bool("enabled", s.enabled);
    read.Number("idleTimeoutSeconds", s.idleTimeoutSeconds, 1.0, 3600.0);
    read.Number("redimDelaySeconds", s.redimDelaySeconds, 0.0, 3600.0);
    read.Number("dimLevelPercent", s.dimLevelPercent, 0.0, 100.0);
    read.Integer("fadeToDimMs", s.fadeToDimMs, 0, 5000);
    read.Integer("fadeToWakeMs", s.fadeToWakeMs, 0, 5000);
    read.Bool("wakeOnTaskbarFocus", s.wakeOnTaskbarFocus);
    read.Bool("wakeOnShellUi", s.wakeOnShellUi);
    read.Bool("hideInFullscreen", s.hideInFullscreen);
    read.Bool("excludeFromCapture", s.excludeFromCapture);
    read.LogLevel("logLevel", s.debugLogging);
    read.HotkeyValue("toggleDesktopIconsHotkey", s.toggleDesktopIconsHotkey);

    const auto version = json.find("version");
    if (version != json.end() && version->is_number_integer() && version->get<int>() > kSchemaVersion) {
        result.warnings.push_back(
            std::format("Settings version {} is newer than supported ({})", version->get<int>(), kSchemaVersion));
    }
    for (const auto& item : json.items()) {
        const std::string& key = item.key();
        if (std::none_of(std::begin(kKnownKeys), std::end(kKnownKeys), [&](const char* known) { return key == known; })) {
            result.warnings.push_back(std::format("Unknown setting '{}' ignored", key));
        }
    }

    result.valid = true;
    return result;
}

std::string SerializeSettings(const Settings& s) {
    nlohmann::ordered_json json;
    json["version"] = kSchemaVersion;
    json["enabled"] = s.enabled;
    json["idleTimeoutSeconds"] = s.idleTimeoutSeconds;
    json["redimDelaySeconds"] = s.redimDelaySeconds;
    json["dimLevelPercent"] = s.dimLevelPercent;
    json["fadeToDimMs"] = s.fadeToDimMs;
    json["fadeToWakeMs"] = s.fadeToWakeMs;
    json["wakeOnTaskbarFocus"] = s.wakeOnTaskbarFocus;
    json["wakeOnShellUi"] = s.wakeOnShellUi;
    json["hideInFullscreen"] = s.hideInFullscreen;
    json["excludeFromCapture"] = s.excludeFromCapture;
    json["logLevel"] = s.debugLogging ? "debug" : "info";
    json["toggleDesktopIconsHotkey"] = FormatHotkey(s.toggleDesktopIconsHotkey);
    return json.dump(2) + "\n";
}

DimPolicy ToPolicy(const Settings& s) {
    DimPolicy policy;
    policy.idleTimeout = SecondsToMs(s.idleTimeoutSeconds);
    policy.redimDelay = SecondsToMs(s.redimDelaySeconds);
    policy.dimLevel = s.dimLevelPercent / 100.0;
    policy.fadeToDim = std::chrono::milliseconds(s.fadeToDimMs);
    policy.fadeToWake = std::chrono::milliseconds(s.fadeToWakeMs);
    return policy;
}

}  // namespace dimmer
