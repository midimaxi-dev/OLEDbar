#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "core/dim_state_machine.h"

namespace dimmer {

// User settings persisted as JSON. Unknown keys are reported and ignored.
struct Settings {
    bool enabled = true;
    double idleTimeoutSeconds = 10.0;
    double redimDelaySeconds = 3.0;
    double dimLevelPercent = 70.0;
    int fadeToDimMs = 1500;
    int fadeToWakeMs = 120;
    bool wakeOnTaskbarFocus = true;
    bool wakeOnShellUi = true;
    bool hideInFullscreen = true;
    bool excludeFromCapture = true;
    bool debugLogging = false;  // JSON: "logLevel": "info" | "debug"

    bool operator==(const Settings&) const = default;
};

struct SettingsParseResult {
    Settings settings;
    // False if the text is not a JSON object; `settings` then holds defaults.
    bool valid = false;
    std::vector<std::string> warnings;
};

SettingsParseResult ParseSettings(std::string_view json);
std::string SerializeSettings(const Settings& settings);
DimPolicy ToPolicy(const Settings& settings);

}  // namespace dimmer
