#pragma once

#include <windows.h>

#include <optional>
#include <string>
#include <vector>

#include "core/settings.h"

namespace dimmer {

// Owns %LOCALAPPDATA%\OLEDbar\settings.json and a change notification on its directory.
// The file is created with defaults if missing; afterwards it is only rewritten by Save() (settings panel).
class SettingsStore {
public:
    SettingsStore() = default;
    ~SettingsStore();
    SettingsStore(const SettingsStore&) = delete;
    SettingsStore& operator=(const SettingsStore&) = delete;

    bool Initialize();
    // True if Initialize() created the settings file, i.e. this is the first run.
    bool CreatedNew() const noexcept { return createdNew_; }
    const std::wstring& Path() const noexcept { return path_; }

    struct LoadResult {
        std::optional<Settings> settings;  // Empty if the file is unreadable or not valid JSON.
        std::vector<std::wstring> messages;
    };
    LoadResult Load();
    // Writes the file atomically; the resulting change notification is not treated as an external edit.
    bool Save(const Settings& settings);
    // True if the file content differs from what the last Load() saw.
    bool HasChanged() const;

    // Signaled when the settings directory changes. Null if not watching.
    HANDLE ChangeEvent() const noexcept { return change_ == INVALID_HANDLE_VALUE ? nullptr : change_; }
    void Rearm();
    void StopWatching();

private:
    std::optional<std::string> ReadContent() const;

    std::wstring directory_;
    std::wstring path_;
    std::string lastContent_;
    bool createdNew_ = false;
    HANDLE change_ = INVALID_HANDLE_VALUE;
};

}  // namespace dimmer
