#include "platform/settings_store.h"

#include <utility>

#include "platform/win_util.h"

namespace dimmer {
namespace {

constexpr std::string_view kUtf8Bom = "\xEF\xBB\xBF";

}  // namespace

SettingsStore::~SettingsStore() {
    StopWatching();
}

bool SettingsStore::Initialize() {
    directory_ = AppDataDirectory();
    if (directory_.empty()) {
        return false;
    }
    path_ = directory_ + L"\\settings.json";
    if (!CreateDirectoryW(directory_.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
        return false;
    }
    if (GetFileAttributesW(path_.c_str()) == INVALID_FILE_ATTRIBUTES) {
        if (!WriteTextFileAtomic(path_, SerializeSettings(Settings{}))) {
            return false;
        }
        createdNew_ = true;
    }
    change_ = FindFirstChangeNotificationW(
        directory_.c_str(), FALSE,
        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE);
    return true;
}

SettingsStore::LoadResult SettingsStore::Load() {
    LoadResult result;
    // A deleted file means "reset": recreate it with defaults.
    if (!path_.empty() && GetFileAttributesW(path_.c_str()) == INVALID_FILE_ATTRIBUTES &&
        GetLastError() == ERROR_FILE_NOT_FOUND) {
        WriteTextFileAtomic(path_, SerializeSettings(Settings{}));
    }
    const std::optional<std::string> content = ReadContent();
    if (!content) {
        result.messages.push_back(L"Cannot read " + path_);
        return result;
    }
    lastContent_ = *content;

    SettingsParseResult parsed = ParseSettings(*content);
    for (const std::string& warning : parsed.warnings) {
        result.messages.push_back(Widen(warning));
    }
    if (parsed.valid) {
        result.settings = parsed.settings;
    }
    return result;
}

bool SettingsStore::Save(const Settings& settings) {
    std::string content = SerializeSettings(settings);
    if (path_.empty() || !WriteTextFileAtomic(path_, content)) {
        return false;
    }
    lastContent_ = std::move(content);
    return true;
}

bool SettingsStore::HasChanged() const {
    const std::optional<std::string> content = ReadContent();
    return !content || *content != lastContent_;
}

void SettingsStore::Rearm() {
    if (change_ != INVALID_HANDLE_VALUE && !FindNextChangeNotification(change_)) {
        StopWatching();
    }
}

void SettingsStore::StopWatching() {
    if (change_ != INVALID_HANDLE_VALUE) {
        FindCloseChangeNotification(change_);
        change_ = INVALID_HANDLE_VALUE;
    }
}

std::optional<std::string> SettingsStore::ReadContent() const {
    std::optional<std::string> content = ReadTextFile(path_);
    if (content && content->starts_with(kUtf8Bom)) {
        content->erase(0, kUtf8Bom.size());
    }
    return content;
}

}  // namespace dimmer
