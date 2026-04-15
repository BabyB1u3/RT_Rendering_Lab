#pragma once

/// @file LogCategories.h
/// @brief Predefined diagnostics categories used by engine subsystems.

#include <array>
#include <string_view>

namespace LogCategory
{
constexpr const char* Core = "Core";
constexpr const char* Graphics = "Graphics";
constexpr const char* Renderer = "Renderer";
constexpr const char* Shader = "Shader";
constexpr const char* Input = "Input";
constexpr const char* FileSystem = "FileSystem";
constexpr const char* Window = "Window";
constexpr const char* ImGui = "ImGui";
constexpr const char* Demo = "Demo";
constexpr const char* Serialization = "Serialization";
constexpr const char* Assert = "Assert";
constexpr const char* Ensure = "Ensure";
constexpr const char* Error = "Error";
constexpr const char* Crash = "Crash";

inline constexpr std::array<const char*, 14> KnownCategories = {
    Core,
    Graphics,
    Renderer,
    Shader,
    Input,
    FileSystem,
    Window,
    ImGui,
    Demo,
    Serialization,
    Assert,
    Ensure,
    Error,
    Crash,
};

constexpr bool IsKnownCategory(std::string_view category)
{
    for (const char* knownCategory : KnownCategories)
    {
        if (category == knownCategory)
            return true;
    }

    return false;
}
} // namespace LogCategory
