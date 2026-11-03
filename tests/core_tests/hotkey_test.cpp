#include <gtest/gtest.h>

#include "core/hotkey.h"

using dimmer::FormatHotkey;
using dimmer::Hotkey;
using dimmer::ParseHotkey;

namespace {

TEST(HotkeyTest, EmptyMeansNoShortcut) {
    const auto hotkey = ParseHotkey("  ");
    ASSERT_TRUE(hotkey);
    EXPECT_FALSE(hotkey->IsSet());
    EXPECT_EQ(FormatHotkey(*hotkey), "");
}

TEST(HotkeyTest, ParsesModifiersAndLetter) {
    const auto hotkey = ParseHotkey("Ctrl+Alt+D");
    ASSERT_TRUE(hotkey);
    EXPECT_EQ(hotkey->modifiers, Hotkey::kCtrl | Hotkey::kAlt);
    EXPECT_EQ(hotkey->key, static_cast<std::uint32_t>('D'));
}

TEST(HotkeyTest, IsCaseAndSpaceInsensitive) {
    EXPECT_EQ(ParseHotkey("control + shift + f12"), ParseHotkey("Ctrl+Shift+F12"));
    EXPECT_EQ(ParseHotkey("alt+d"), ParseHotkey("Alt+D"));
}

TEST(HotkeyTest, FormatsInCanonicalOrder) {
    EXPECT_EQ(FormatHotkey(*ParseHotkey("Shift+Alt+Ctrl+d")), "Ctrl+Alt+Shift+D");
}

TEST(HotkeyTest, RoundTripsAllKeyKinds) {
    for (const char* text : {"Ctrl+Alt+D", "Alt+7", "Ctrl+F24", "Ctrl+Alt+Num5", "Ctrl+Alt+NumPlus", "Alt+PageDown",
                             "Ctrl+Alt+;", "Ctrl+Alt+\\", "Ctrl+Alt+-", "Ctrl+Alt+0xE2", "Ctrl+Space"}) {
        const auto hotkey = ParseHotkey(text);
        ASSERT_TRUE(hotkey) << text;
        EXPECT_EQ(FormatHotkey(*hotkey), text);
    }
}

TEST(HotkeyTest, RequiresCtrlOrAlt) {
    EXPECT_FALSE(ParseHotkey("D"));
    EXPECT_FALSE(ParseHotkey("Shift+D"));
    EXPECT_FALSE(ParseHotkey("F5"));
    EXPECT_TRUE(ParseHotkey("Alt+Shift+D"));
}

TEST(HotkeyTest, RejectsUnknownOrMalformed) {
    EXPECT_FALSE(ParseHotkey("Win+D"));
    EXPECT_FALSE(ParseHotkey("Ctrl+Alt+"));
    EXPECT_FALSE(ParseHotkey("Ctrl+Alt+Foo"));
    EXPECT_FALSE(ParseHotkey("Ctrl+Alt+F25"));
    EXPECT_FALSE(ParseHotkey("Ctrl+Alt+0x1FF"));
    EXPECT_FALSE(ParseHotkey("Ctrl++D"));
}

TEST(HotkeyTest, Validity) {
    EXPECT_TRUE(Hotkey{}.IsValid());
    EXPECT_FALSE((Hotkey{Hotkey::kCtrl, 0}).IsValid());
    EXPECT_FALSE((Hotkey{Hotkey::kShift, 'D'}).IsValid());
    EXPECT_FALSE((Hotkey{0x8 /* MOD_WIN */ | Hotkey::kCtrl, 'D'}).IsValid());
    EXPECT_TRUE((Hotkey{Hotkey::kCtrl, 'D'}).IsValid());
}

}  // namespace
