#pragma once

#include <cstdint>
#include <string>

namespace GamepadButton
{
    using Code = uint16_t;

    enum : Code
    {
        A = 0,
        B,
        X,
        Y,
        LeftBumper,
        RightBumper,
        Back,
        Start,
        Guide,
        LeftThumb,
        RightThumb,
        DPadUp,
        DPadRight,
        DPadDown,
        DPadLeft,

        Last = DPadLeft,
        Count = Last + 1,
        InvalidCode = 0xFFFF
    };

    const std::string &ToName(Code code);
    Code FromName(const std::string &name);
}

namespace GamepadAxis
{
    using Code = uint16_t;

    enum : Code
    {
        LeftX = 0,
        LeftY,
        RightX,
        RightY,
        LeftTrigger,
        RightTrigger,

        Last = RightTrigger,
        Count = Last + 1,
        InvalidCode = 0xFFFF
    };

    const std::string &ToName(Code code);
    Code FromName(const std::string &name);
}
