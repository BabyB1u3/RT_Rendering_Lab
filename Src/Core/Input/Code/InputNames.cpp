#include "Core/Input/Code/InputNames.h"

#include <unordered_map>

// ---------------------------------------------------------------------------
// Key name tables
// ---------------------------------------------------------------------------

namespace
{
struct KeyNameEntry
{
    Key::Code code;
    const char* name;
};

// clang-format off
    constexpr KeyNameEntry g_KeyTable[] = {
        { Key::Space,        "Space" },
        { Key::Apostrophe,   "Apostrophe" },
        { Key::Comma,        "Comma" },
        { Key::Minus,        "Minus" },
        { Key::Period,       "Period" },
        { Key::Slash,        "Slash" },

        { Key::D0, "D0" }, { Key::D1, "D1" }, { Key::D2, "D2" }, { Key::D3, "D3" },
        { Key::D4, "D4" }, { Key::D5, "D5" }, { Key::D6, "D6" }, { Key::D7, "D7" },
        { Key::D8, "D8" }, { Key::D9, "D9" },

        { Key::Semicolon, "Semicolon" },
        { Key::Equal,     "Equal" },

        { Key::A, "A" }, { Key::B, "B" }, { Key::C, "C" }, { Key::D, "D" },
        { Key::E, "E" }, { Key::F, "F" }, { Key::G, "G" }, { Key::H, "H" },
        { Key::I, "I" }, { Key::J, "J" }, { Key::K, "K" }, { Key::L, "L" },
        { Key::M, "M" }, { Key::N, "N" }, { Key::O, "O" }, { Key::P, "P" },
        { Key::Q, "Q" }, { Key::R, "R" }, { Key::S, "S" }, { Key::T, "T" },
        { Key::U, "U" }, { Key::V, "V" }, { Key::W, "W" }, { Key::X, "X" },
        { Key::Y, "Y" }, { Key::Z, "Z" },

        { Key::LeftBracket,  "LeftBracket" },
        { Key::Backslash,    "Backslash" },
        { Key::RightBracket, "RightBracket" },
        { Key::GraveAccent,  "GraveAccent" },
        { Key::World1,       "World1" },
        { Key::World2,       "World2" },

        { Key::Escape,    "Escape" },
        { Key::Enter,     "Enter" },
        { Key::Tab,       "Tab" },
        { Key::Backspace, "Backspace" },
        { Key::Insert,    "Insert" },
        { Key::Delete,    "Delete" },
        { Key::Right,     "Right" },
        { Key::Left,      "Left" },
        { Key::Down,      "Down" },
        { Key::Up,        "Up" },
        { Key::PageUp,    "PageUp" },
        { Key::PageDown,  "PageDown" },
        { Key::Home,      "Home" },
        { Key::End,       "End" },
        { Key::CapsLock,   "CapsLock" },
        { Key::ScrollLock, "ScrollLock" },
        { Key::NumLock,    "NumLock" },
        { Key::PrintScreen,"PrintScreen" },
        { Key::Pause,      "Pause" },

        { Key::F1,  "F1" },  { Key::F2,  "F2" },  { Key::F3,  "F3" },
        { Key::F4,  "F4" },  { Key::F5,  "F5" },  { Key::F6,  "F6" },
        { Key::F7,  "F7" },  { Key::F8,  "F8" },  { Key::F9,  "F9" },
        { Key::F10, "F10" }, { Key::F11, "F11" }, { Key::F12, "F12" },
        { Key::F13, "F13" }, { Key::F14, "F14" }, { Key::F15, "F15" },
        { Key::F16, "F16" }, { Key::F17, "F17" }, { Key::F18, "F18" },
        { Key::F19, "F19" }, { Key::F20, "F20" }, { Key::F21, "F21" },
        { Key::F22, "F22" }, { Key::F23, "F23" }, { Key::F24, "F24" },
        { Key::F25, "F25" },

        { Key::KP0, "KP0" }, { Key::KP1, "KP1" }, { Key::KP2, "KP2" },
        { Key::KP3, "KP3" }, { Key::KP4, "KP4" }, { Key::KP5, "KP5" },
        { Key::KP6, "KP6" }, { Key::KP7, "KP7" }, { Key::KP8, "KP8" },
        { Key::KP9, "KP9" },
        { Key::KPDecimal,  "KPDecimal" },
        { Key::KPDivide,   "KPDivide" },
        { Key::KPMultiply, "KPMultiply" },
        { Key::KPSubtract, "KPSubtract" },
        { Key::KPAdd,      "KPAdd" },
        { Key::KPEnter,    "KPEnter" },
        { Key::KPEqual,    "KPEqual" },

        { Key::LeftShift,    "LeftShift" },
        { Key::LeftControl,  "LeftControl" },
        { Key::LeftAlt,      "LeftAlt" },
        { Key::LeftSuper,    "LeftSuper" },
        { Key::RightShift,   "RightShift" },
        { Key::RightControl, "RightControl" },
        { Key::RightAlt,     "RightAlt" },
        { Key::RightSuper,   "RightSuper" },
        { Key::Menu,         "Menu" },
    };
// clang-format on

const std::unordered_map<Key::Code, std::string>& GetCodeToKeyName()
{
    static std::unordered_map<Key::Code, std::string> map;
    if (map.empty())
    {
        for (const auto& e : g_KeyTable)
            map[e.code] = e.name;
    }
    return map;
}

const std::unordered_map<std::string, Key::Code>& GetKeyNameToCode()
{
    static std::unordered_map<std::string, Key::Code> map;
    if (map.empty())
    {
        for (const auto& e : g_KeyTable)
            map[e.name] = e.code;
    }
    return map;
}

const std::string g_EmptyString;
} // namespace

const std::string& Key::ToName(Code code)
{
    const auto& map = GetCodeToKeyName();
    auto it = map.find(code);
    return it != map.end() ? it->second : g_EmptyString;
}

Key::Code Key::FromName(const std::string& name)
{
    const auto& map = GetKeyNameToCode();
    auto it = map.find(name);
    return it != map.end() ? it->second : InvalidCode;
}

// ---------------------------------------------------------------------------
// Mouse name tables
// ---------------------------------------------------------------------------

namespace
{
struct MouseNameEntry
{
    Mouse::Code code;
    const char* name;
};

// clang-format off
    constexpr MouseNameEntry g_MouseTable[] = {
        { Mouse::Left,    "Left" },
        { Mouse::Right,   "Right" },
        { Mouse::Middle,  "Middle" },
        { Mouse::Button3, "Button3" },
        { Mouse::Button4, "Button4" },
        { Mouse::Button5, "Button5" },
        { Mouse::Button6, "Button6" },
        { Mouse::Button7, "Button7" },
    };
// clang-format on

const std::unordered_map<Mouse::Code, std::string>& GetCodeToMouseName()
{
    static std::unordered_map<Mouse::Code, std::string> map;
    if (map.empty())
    {
        for (const auto& e : g_MouseTable)
            map[e.code] = e.name;
    }
    return map;
}

const std::unordered_map<std::string, Mouse::Code>& GetMouseNameToCode()
{
    static std::unordered_map<std::string, Mouse::Code> map;
    if (map.empty())
    {
        for (const auto& e : g_MouseTable)
            map[e.name] = e.code;
    }
    return map;
}
} // namespace

const std::string& Mouse::ToName(Code code)
{
    const auto& map = GetCodeToMouseName();
    auto it = map.find(code);
    return it != map.end() ? it->second : g_EmptyString;
}

Mouse::Code Mouse::FromName(const std::string& name)
{
    const auto& map = GetMouseNameToCode();
    auto it = map.find(name);
    return it != map.end() ? it->second : InvalidCode;
}

// ---------------------------------------------------------------------------
// Gamepad button name tables
// ---------------------------------------------------------------------------

namespace
{
struct GamepadButtonNameEntry
{
    GamepadButton::Code code;
    const char* name;
};

constexpr GamepadButtonNameEntry g_GamepadButtonTable[] = {
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

const std::unordered_map<GamepadButton::Code, std::string>& GetCodeToGamepadButtonName()
{
    static std::unordered_map<GamepadButton::Code, std::string> map;
    if (map.empty())
    {
        for (const auto& entry : g_GamepadButtonTable)
            map[entry.code] = entry.name;
    }
    return map;
}

const std::unordered_map<std::string, GamepadButton::Code>& GetGamepadButtonNameToCode()
{
    static std::unordered_map<std::string, GamepadButton::Code> map;
    if (map.empty())
    {
        for (const auto& entry : g_GamepadButtonTable)
            map[entry.name] = entry.code;
    }
    return map;
}
} // namespace

const std::string& GamepadButton::ToName(Code code)
{
    const auto& map = GetCodeToGamepadButtonName();
    auto it = map.find(code);
    return it != map.end() ? it->second : g_EmptyString;
}

GamepadButton::Code GamepadButton::FromName(const std::string& name)
{
    const auto& map = GetGamepadButtonNameToCode();
    auto it = map.find(name);
    return it != map.end() ? it->second : InvalidCode;
}

// ---------------------------------------------------------------------------
// Gamepad axis name tables
// ---------------------------------------------------------------------------

namespace
{
struct GamepadAxisNameEntry
{
    GamepadAxis::Code code;
    const char* name;
};

constexpr GamepadAxisNameEntry g_GamepadAxisTable[] = {
    {GamepadAxis::LeftX, "LeftX"},
    {GamepadAxis::LeftY, "LeftY"},
    {GamepadAxis::RightX, "RightX"},
    {GamepadAxis::RightY, "RightY"},
    {GamepadAxis::LeftTrigger, "LeftTrigger"},
    {GamepadAxis::RightTrigger, "RightTrigger"},
};

const std::unordered_map<GamepadAxis::Code, std::string>& GetCodeToGamepadAxisName()
{
    static std::unordered_map<GamepadAxis::Code, std::string> map;
    if (map.empty())
    {
        for (const auto& entry : g_GamepadAxisTable)
            map[entry.code] = entry.name;
    }
    return map;
}

const std::unordered_map<std::string, GamepadAxis::Code>& GetGamepadAxisNameToCode()
{
    static std::unordered_map<std::string, GamepadAxis::Code> map;
    if (map.empty())
    {
        for (const auto& entry : g_GamepadAxisTable)
            map[entry.name] = entry.code;
    }
    return map;
}
} // namespace

const std::string& GamepadAxis::ToName(Code code)
{
    const auto& map = GetCodeToGamepadAxisName();
    auto it = map.find(code);
    return it != map.end() ? it->second : g_EmptyString;
}

GamepadAxis::Code GamepadAxis::FromName(const std::string& name)
{
    const auto& map = GetGamepadAxisNameToCode();
    auto it = map.find(name);
    return it != map.end() ? it->second : InvalidCode;
}
