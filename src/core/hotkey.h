#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace dimmer {

// A global keyboard shortcut. Modifier bits and key codes match Win32 MOD_* flags and virtual-key codes.
struct Hotkey {
    static constexpr std::uint32_t kAlt = 0x1;
    static constexpr std::uint32_t kCtrl = 0x2;
    static constexpr std::uint32_t kShift = 0x4;

    std::uint32_t modifiers = 0;
    std::uint32_t key = 0;  // Virtual-key code; 0 means no shortcut.

    bool IsSet() const noexcept { return key != 0; }
    // True if unset, or if it has a key plus Ctrl or Alt (so it can't swallow normal typing).
    bool IsValid() const noexcept;

    bool operator==(const Hotkey&) const = default;
};

// Parses text like "Ctrl+Alt+D" (case-insensitive). An empty string is a valid "no shortcut".
// Returns nullopt for unknown names or shortcuts that fail IsValid().
std::optional<Hotkey> ParseHotkey(std::string_view text);
// Inverse of ParseHotkey; "" for no shortcut.
std::string FormatHotkey(const Hotkey& hotkey);

}  // namespace dimmer
