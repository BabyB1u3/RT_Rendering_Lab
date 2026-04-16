#pragma once

/// @file LogCategories.h
/// @brief Predefined diagnostics categories used by engine subsystems.

#include <array>
#include <string_view>

namespace LogCategory
{
constexpr const char* k_Core = "Core";
constexpr const char* k_Graphics = "Graphics";
constexpr const char* k_Renderer = "Renderer";
constexpr const char* k_Shader = "Shader";
constexpr const char* k_Input = "Input";
constexpr const char* k_FileSystem = "FileSystem";
constexpr const char* k_Window = "Window";
constexpr const char* k_ImGui = "ImGui";
constexpr const char* k_Demo = "Demo";
constexpr const char* k_Serialization = "Serialization";
constexpr const char* k_Assert = "Assert";
constexpr const char* k_Ensure = "Ensure";
constexpr const char* k_Error = "Error";
constexpr const char* k_Crash = "Crash";

inline constexpr std::array<const char*, 14> k_KnownCategories = {
    k_Core,
    k_Graphics,
    k_Renderer,
    k_Shader,
    k_Input,
    k_FileSystem,
    k_Window,
    k_ImGui,
    k_Demo,
    k_Serialization,
    k_Assert,
    k_Ensure,
    k_Error,
    k_Crash,
};

constexpr bool IsKnownCategory(std::string_view category)
{
    for (const char* knownCategory : k_KnownCategories)
    {
        if (category == knownCategory)
            return true;
    }

    return false;
}
} // namespace LogCategory
