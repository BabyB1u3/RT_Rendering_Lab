#include <gtest/gtest.h>

#include "core/input/code/InputNames.h"

namespace
{
struct KeyNameCase
{
    Key::Code code;
    const char *name;
};

struct MouseNameCase
{
    Mouse::Code code;
    const char *name;
};

struct GamepadButtonNameCase
{
    GamepadButton::Code code;
    const char *name;
};

struct GamepadAxisNameCase
{
    GamepadAxis::Code code;
    const char *name;
};

constexpr KeyNameCase kKeyCases[] = {
    {Key::Space, "Space"},
    {Key::Apostrophe, "Apostrophe"},
    {Key::Comma, "Comma"},
    {Key::Minus, "Minus"},
    {Key::Period, "Period"},
    {Key::Slash, "Slash"},

    {Key::D0, "D0"},
    {Key::D1, "D1"},
    {Key::D2, "D2"},
    {Key::D3, "D3"},
    {Key::D4, "D4"},
    {Key::D5, "D5"},
    {Key::D6, "D6"},
    {Key::D7, "D7"},
    {Key::D8, "D8"},
    {Key::D9, "D9"},

    {Key::Semicolon, "Semicolon"},
    {Key::Equal, "Equal"},

    {Key::A, "A"},
    {Key::B, "B"},
    {Key::C, "C"},
    {Key::D, "D"},
    {Key::E, "E"},
    {Key::F, "F"},
    {Key::G, "G"},
    {Key::H, "H"},
    {Key::I, "I"},
    {Key::J, "J"},
    {Key::K, "K"},
    {Key::L, "L"},
    {Key::M, "M"},
    {Key::N, "N"},
    {Key::O, "O"},
    {Key::P, "P"},
    {Key::Q, "Q"},
    {Key::R, "R"},
    {Key::S, "S"},
    {Key::T, "T"},
    {Key::U, "U"},
    {Key::V, "V"},
    {Key::W, "W"},
    {Key::X, "X"},
    {Key::Y, "Y"},
    {Key::Z, "Z"},

    {Key::LeftBracket, "LeftBracket"},
    {Key::Backslash, "Backslash"},
    {Key::RightBracket, "RightBracket"},
    {Key::GraveAccent, "GraveAccent"},
    {Key::World1, "World1"},
    {Key::World2, "World2"},

    {Key::Escape, "Escape"},
    {Key::Enter, "Enter"},
    {Key::Tab, "Tab"},
    {Key::Backspace, "Backspace"},
    {Key::Insert, "Insert"},
    {Key::Delete, "Delete"},
    {Key::Right, "Right"},
    {Key::Left, "Left"},
    {Key::Down, "Down"},
    {Key::Up, "Up"},
    {Key::PageUp, "PageUp"},
    {Key::PageDown, "PageDown"},
    {Key::Home, "Home"},
    {Key::End, "End"},
    {Key::CapsLock, "CapsLock"},
    {Key::ScrollLock, "ScrollLock"},
    {Key::NumLock, "NumLock"},
    {Key::PrintScreen, "PrintScreen"},
    {Key::Pause, "Pause"},

    {Key::F1, "F1"},
    {Key::F2, "F2"},
    {Key::F3, "F3"},
    {Key::F4, "F4"},
    {Key::F5, "F5"},
    {Key::F6, "F6"},
    {Key::F7, "F7"},
    {Key::F8, "F8"},
    {Key::F9, "F9"},
    {Key::F10, "F10"},
    {Key::F11, "F11"},
    {Key::F12, "F12"},
    {Key::F13, "F13"},
    {Key::F14, "F14"},
    {Key::F15, "F15"},
    {Key::F16, "F16"},
    {Key::F17, "F17"},
    {Key::F18, "F18"},
    {Key::F19, "F19"},
    {Key::F20, "F20"},
    {Key::F21, "F21"},
    {Key::F22, "F22"},
    {Key::F23, "F23"},
    {Key::F24, "F24"},
    {Key::F25, "F25"},

    {Key::KP0, "KP0"},
    {Key::KP1, "KP1"},
    {Key::KP2, "KP2"},
    {Key::KP3, "KP3"},
    {Key::KP4, "KP4"},
    {Key::KP5, "KP5"},
    {Key::KP6, "KP6"},
    {Key::KP7, "KP7"},
    {Key::KP8, "KP8"},
    {Key::KP9, "KP9"},
    {Key::KPDecimal, "KPDecimal"},
    {Key::KPDivide, "KPDivide"},
    {Key::KPMultiply, "KPMultiply"},
    {Key::KPSubtract, "KPSubtract"},
    {Key::KPAdd, "KPAdd"},
    {Key::KPEnter, "KPEnter"},
    {Key::KPEqual, "KPEqual"},

    {Key::LeftShift, "LeftShift"},
    {Key::LeftControl, "LeftControl"},
    {Key::LeftAlt, "LeftAlt"},
    {Key::LeftSuper, "LeftSuper"},
    {Key::RightShift, "RightShift"},
    {Key::RightControl, "RightControl"},
    {Key::RightAlt, "RightAlt"},
    {Key::RightSuper, "RightSuper"},
    {Key::Menu, "Menu"},
};

constexpr MouseNameCase kMouseCases[] = {
    {Mouse::Left, "Left"},
    {Mouse::Right, "Right"},
    {Mouse::Middle, "Middle"},
    {Mouse::Button3, "Button3"},
    {Mouse::Button4, "Button4"},
    {Mouse::Button5, "Button5"},
    {Mouse::Button6, "Button6"},
    {Mouse::Button7, "Button7"},
};

constexpr GamepadButtonNameCase kGamepadButtonCases[] = {
    {GamepadButton::A, "A"},
    {GamepadButton::B, "B"},
    {GamepadButton::X, "X"},
    {GamepadButton::Y, "Y"},
    {GamepadButton::LeftBumper, "LeftBumper"},
    {GamepadButton::RightBumper, "RightBumper"},
    {GamepadButton::Back, "Back"},
    {GamepadButton::Start, "Start"},
    {GamepadButton::Guide, "Guide"},
    {GamepadButton::LeftThumb, "LeftThumb"},
    {GamepadButton::RightThumb, "RightThumb"},
    {GamepadButton::DPadUp, "DPadUp"},
    {GamepadButton::DPadRight, "DPadRight"},
    {GamepadButton::DPadDown, "DPadDown"},
    {GamepadButton::DPadLeft, "DPadLeft"},
};

constexpr GamepadAxisNameCase kGamepadAxisCases[] = {
    {GamepadAxis::LeftX, "LeftX"},
    {GamepadAxis::LeftY, "LeftY"},
    {GamepadAxis::RightX, "RightX"},
    {GamepadAxis::RightY, "RightY"},
    {GamepadAxis::LeftTrigger, "LeftTrigger"},
    {GamepadAxis::RightTrigger, "RightTrigger"},
};
} // namespace

TEST(InputNamesTests, KeyToNameReturnsEmptyForUnknownCodes)
{
    EXPECT_TRUE(Key::ToName(static_cast<Key::Code>(999)).empty());
    EXPECT_TRUE(Key::ToName(Key::InvalidCode).empty());
}

TEST(InputNamesTests, KeyFromNameReturnsInvalidForUnknownInputs)
{
    EXPECT_EQ(Key::FromName("NotARealKey"), Key::InvalidCode);
    EXPECT_EQ(Key::FromName(""), Key::InvalidCode);
}

TEST(InputNamesTests, KeyFromNameIsCaseSensitiveAndExactMatch)
{
    EXPECT_EQ(Key::FromName("w"), Key::InvalidCode);
    EXPECT_EQ(Key::FromName("SPACE"), Key::InvalidCode);
    EXPECT_EQ(Key::FromName("Left Shift"), Key::InvalidCode);
    EXPECT_EQ(Key::FromName("LeftShift"), Key::LeftShift);
}

TEST(InputNamesTests, KeyCanonicalEntriesRoundTrip)
{
    for (const auto &entry : kKeyCases)
    {
        EXPECT_EQ(Key::ToName(entry.code), entry.name);
        EXPECT_EQ(Key::FromName(entry.name), entry.code);
    }
}

TEST(InputNamesTests, MouseToNameReturnsEmptyForUnknownCodes)
{
    EXPECT_TRUE(Mouse::ToName(static_cast<Mouse::Code>(99)).empty());
    EXPECT_TRUE(Mouse::ToName(Mouse::InvalidCode).empty());
}

TEST(InputNamesTests, MouseFromNameReturnsInvalidForUnknownInputs)
{
    EXPECT_EQ(Mouse::FromName("NotARealButton"), Mouse::InvalidCode);
    EXPECT_EQ(Mouse::FromName(""), Mouse::InvalidCode);
}

TEST(InputNamesTests, MouseFromNameIsCaseSensitiveAndExactMatch)
{
    EXPECT_EQ(Mouse::FromName("left"), Mouse::InvalidCode);
    EXPECT_EQ(Mouse::FromName("Button0"), Mouse::InvalidCode);
    EXPECT_EQ(Mouse::FromName("Left"), Mouse::Left);
}

TEST(InputNamesTests, MouseCanonicalEntriesRoundTrip)
{
    for (const auto &entry : kMouseCases)
    {
        EXPECT_EQ(Mouse::ToName(entry.code), entry.name);
        EXPECT_EQ(Mouse::FromName(entry.name), entry.code);
    }
}

TEST(InputNamesTests, GamepadButtonToNameReturnsEmptyForUnknownCodes)
{
    EXPECT_TRUE(GamepadButton::ToName(static_cast<GamepadButton::Code>(999)).empty());
    EXPECT_TRUE(GamepadButton::ToName(GamepadButton::InvalidCode).empty());
}

TEST(InputNamesTests, GamepadButtonFromNameReturnsInvalidForUnknownInputs)
{
    EXPECT_EQ(GamepadButton::FromName("NotARealButton"), GamepadButton::InvalidCode);
    EXPECT_EQ(GamepadButton::FromName(""), GamepadButton::InvalidCode);
}

TEST(InputNamesTests, GamepadButtonCanonicalEntriesRoundTrip)
{
    for (const auto &entry : kGamepadButtonCases)
    {
        EXPECT_EQ(GamepadButton::ToName(entry.code), entry.name);
        EXPECT_EQ(GamepadButton::FromName(entry.name), entry.code);
    }
}

TEST(InputNamesTests, GamepadAxisToNameReturnsEmptyForUnknownCodes)
{
    EXPECT_TRUE(GamepadAxis::ToName(static_cast<GamepadAxis::Code>(999)).empty());
    EXPECT_TRUE(GamepadAxis::ToName(GamepadAxis::InvalidCode).empty());
}

TEST(InputNamesTests, GamepadAxisFromNameReturnsInvalidForUnknownInputs)
{
    EXPECT_EQ(GamepadAxis::FromName("NotARealAxis"), GamepadAxis::InvalidCode);
    EXPECT_EQ(GamepadAxis::FromName(""), GamepadAxis::InvalidCode);
}

TEST(InputNamesTests, GamepadAxisCanonicalEntriesRoundTrip)
{
    for (const auto &entry : kGamepadAxisCases)
    {
        EXPECT_EQ(GamepadAxis::ToName(entry.code), entry.name);
        EXPECT_EQ(GamepadAxis::FromName(entry.name), entry.code);
    }
}
