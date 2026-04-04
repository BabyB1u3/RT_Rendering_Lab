#pragma once

/// @file InputNames.h
/// @brief Bidirectional name <-> code mapping for keys and mouse buttons.
///        Used by InputActionMap serialization.

#include <string>
#include "core/input/code/GamepadCode.h"
#include "core/input/code/KeyCode.h"
#include "core/input/code/MouseCode.h"

namespace Key
{
    /// Returns the canonical name for a key code (e.g. 65 -> "A", 256 -> "Escape").
    /// Returns empty string for unknown codes.
    const std::string &ToName(Code code);

    /// Returns the key code for a name (e.g. "A" -> 65, "Escape" -> 256).
    /// Returns 0xFFFF for unknown names.
    Code FromName(const std::string &name);

    constexpr Code InvalidCode = 0xFFFF;
}

namespace Mouse
{
    /// Returns the canonical name for a mouse button (e.g. 0 -> "Left", 2 -> "Middle").
    const std::string &ToName(Code code);

    /// Returns the mouse button code for a name (e.g. "Left" -> 0, "Right" -> 1).
    /// Returns 0xFFFF for unknown names.
    Code FromName(const std::string &name);

    constexpr Code InvalidCode = 0xFFFF;
}

namespace GamepadButton
{
    const std::string &ToName(Code code);
    Code FromName(const std::string &name);
}

namespace GamepadAxis
{
    const std::string &ToName(Code code);
    Code FromName(const std::string &name);
}
