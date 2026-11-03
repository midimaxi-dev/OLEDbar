#include "core/hotkey.h"

#include <algorithm>
#include <charconv>
#include <format>
#include <iterator>

namespace dimmer {
namespace {

struct NamedKey {
    std::uint32_t key;
    const char* name;
};

// Keys whose names aren't a single letter/digit or F1-F24. Punctuation uses the US layout symbol.
constexpr NamedKey kNamedKeys[] = {
    {0x08, "Backspace"}, {0x09, "Tab"},        {0x0D, "Enter"},      {0x13, "Pause"},     {0x1B, "Esc"},
    {0x20, "Space"},     {0x21, "PageUp"},     {0x22, "PageDown"},   {0x23, "End"},       {0x24, "Home"},
    {0x25, "Left"},      {0x26, "Up"},         {0x27, "Right"},      {0x28, "Down"},      {0x2D, "Insert"},
    {0x2E, "Delete"},    {0x6A, "NumMultiply"}, {0x6B, "NumPlus"},   {0x6D, "NumMinus"},  {0x6E, "NumDecimal"},
    {0x6F, "NumDivide"}, {0xBA, ";"},          {0xBB, "="},          {0xBC, ","},         {0xBD, "-"},
    {0xBE, "."},         {0xBF, "/"},          {0xC0, "`"},          {0xDB, "["},         {0xDC, "\\"},
    {0xDD, "]"},         {0xDE, "'"},
};

constexpr std::uint32_t kKeyF1 = 0x70;
constexpr std::uint32_t kKeyNum0 = 0x60;
constexpr std::uint32_t kAllModifiers = Hotkey::kAlt | Hotkey::kCtrl | Hotkey::kShift;

bool EqualsIgnoreCase(std::string_view a, std::string_view b) noexcept {
    return std::ranges::equal(a, b, [](char x, char y) {
        const auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; };
        return lower(x) == lower(y);
    });
}

std::string_view Trim(std::string_view text) noexcept {
    while (!text.empty() && text.front() == ' ') {
        text.remove_prefix(1);
    }
    while (!text.empty() && text.back() == ' ') {
        text.remove_suffix(1);
    }
    return text;
}

std::optional<std::uint32_t> ParseNumber(std::string_view text, int base) {
    std::uint32_t value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value, base);
    if (error != std::errc{} || end != text.data() + text.size() || text.empty()) {
        return std::nullopt;
    }
    return value;
}

std::optional<std::uint32_t> ParseKey(std::string_view name) {
    if (name.size() == 1) {
        const char c = name[0];
        if (c >= 'a' && c <= 'z') {
            return static_cast<std::uint32_t>(c - 'a' + 'A');
        }
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
            return static_cast<std::uint32_t>(c);
        }
    }
    for (const NamedKey& named : kNamedKeys) {
        if (EqualsIgnoreCase(name, named.name)) {
            return named.key;
        }
    }
    if (name.size() >= 2 && (name[0] == 'F' || name[0] == 'f')) {
        const auto number = ParseNumber(name.substr(1), 10);
        if (number && *number >= 1 && *number <= 24) {
            return kKeyF1 + *number - 1;
        }
    }
    if (name.size() == 4 && EqualsIgnoreCase(name.substr(0, 3), "Num") && name[3] >= '0' && name[3] <= '9') {
        return kKeyNum0 + static_cast<std::uint32_t>(name[3] - '0');
    }
    // Fallback for keys without a name, e.g. "0xE2".
    if (name.size() > 2 && name[0] == '0' && (name[1] == 'x' || name[1] == 'X')) {
        const auto code = ParseNumber(name.substr(2), 16);
        if (code && *code >= 1 && *code <= 0xFE) {
            return *code;
        }
    }
    return std::nullopt;
}

std::string KeyName(std::uint32_t key) {
    if ((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9')) {
        return std::string(1, static_cast<char>(key));
    }
    if (key >= kKeyF1 && key < kKeyF1 + 24) {
        return std::format("F{}", key - kKeyF1 + 1);
    }
    if (key >= kKeyNum0 && key <= kKeyNum0 + 9) {
        return std::format("Num{}", key - kKeyNum0);
    }
    const auto* named = std::ranges::find(kNamedKeys, key, &NamedKey::key);
    if (named != std::end(kNamedKeys)) {
        return named->name;
    }
    return std::format("0x{:02X}", key);
}

}  // namespace

bool Hotkey::IsValid() const noexcept {
    if (!IsSet()) {
        return modifiers == 0;
    }
    return key <= 0xFE && (modifiers & ~kAllModifiers) == 0 && (modifiers & (kCtrl | kAlt)) != 0;
}

std::optional<Hotkey> ParseHotkey(std::string_view text) {
    text = Trim(text);
    if (text.empty()) {
        return Hotkey{};
    }
    Hotkey hotkey;
    for (;;) {
        const size_t plus = text.find('+');
        if (plus == std::string_view::npos) {
            break;
        }
        const std::string_view part = Trim(text.substr(0, plus));
        if (EqualsIgnoreCase(part, "Ctrl") || EqualsIgnoreCase(part, "Control")) {
            hotkey.modifiers |= Hotkey::kCtrl;
        } else if (EqualsIgnoreCase(part, "Alt")) {
            hotkey.modifiers |= Hotkey::kAlt;
        } else if (EqualsIgnoreCase(part, "Shift")) {
            hotkey.modifiers |= Hotkey::kShift;
        } else {
            return std::nullopt;
        }
        text = text.substr(plus + 1);
    }
    const auto key = ParseKey(Trim(text));
    if (!key) {
        return std::nullopt;
    }
    hotkey.key = *key;
    if (!hotkey.IsValid()) {
        return std::nullopt;
    }
    return hotkey;
}

std::string FormatHotkey(const Hotkey& hotkey) {
    if (!hotkey.IsSet()) {
        return {};
    }
    std::string text;
    if (hotkey.modifiers & Hotkey::kCtrl) {
        text += "Ctrl+";
    }
    if (hotkey.modifiers & Hotkey::kAlt) {
        text += "Alt+";
    }
    if (hotkey.modifiers & Hotkey::kShift) {
        text += "Shift+";
    }
    return text + KeyName(hotkey.key);
}

}  // namespace dimmer
