#pragma once

/// @file Serialization.h
/// @brief One-call SaveToFile / LoadFromFile convenience layer.
///
/// Ties together FileSystem, format backends, and serialization traits
/// into simple template functions for common use cases.

#include "Core/Serialization/SerializationTraits.h"
#include "Core/Serialization/JsonBackend.h"
#include "Core/Resource/FileSystem.h"
#include "Core/Diagnostics/Logging/LogCategories.h"
#include "Core/Diagnostics/Logging/LogMacros.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace Serialization
{
namespace Detail
{
inline std::string MakeConfigVirtualPath(std::string_view mountRoot, std::string_view relativePath)
{
    if (relativePath.empty())
        return std::string(mountRoot);

    return std::string(mountRoot) + "/" + std::string(relativePath);
}

template <Serializable T>
bool LoadValueFromString(T& value, const std::string& data, const FormatBackend& backend, std::string_view sourceLabel)
{
    PropertyTree tree;
    if (!backend.ReadFromString(data, tree))
    {
        LOG_ERROR_CAT(LogCategory::k_Serialization, "Serialization: failed to parse '{}'", sourceLabel);
        return false;
    }

    T temp{};
    if (!Deserialize(tree, temp))
    {
        LOG_ERROR_CAT(LogCategory::k_Serialization, "Serialization: failed to deserialize '{}'", sourceLabel);
        return false;
    }

    value = std::move(temp);
    return true;
}

inline std::optional<std::string> ResolveConfigReadText(std::string_view relativePath)
{
    const std::string savedVirtualPath = MakeConfigVirtualPath("/Saved/Config", relativePath);
    const std::string projectVirtualPath = MakeConfigVirtualPath("/Project/Config", relativePath);
    const std::string engineVirtualPath = MakeConfigVirtualPath("/Engine/Config", relativePath);

    if (const auto savedText = FileSystem::ReadText(savedVirtualPath); savedText.has_value())
        return savedText;

    const auto seedToSaved = [&](std::string_view sourceVirtualPath,
                                 std::string_view sourceLabel) -> std::optional<std::string>
    {
        const auto sourceText = FileSystem::ReadText(sourceVirtualPath);
        if (!sourceText.has_value())
            return std::nullopt;

        if (!FileSystem::WriteText(savedVirtualPath, *sourceText))
        {
            LOG_WARN_CAT(LogCategory::k_Serialization,
                         "Serialization: failed to seed {} config '{}' into saved path '{}'",
                         sourceLabel,
                         relativePath,
                         savedVirtualPath);
        }

        return sourceText;
    };

    if (const auto seededProjectText = seedToSaved(projectVirtualPath, "project"); seededProjectText.has_value())
        return seededProjectText;

    if (const auto seededEngineText = seedToSaved(engineVirtualPath, "engine"); seededEngineText.has_value())
        return seededEngineText;

    return std::nullopt;
}
} // namespace Detail

/// Get the default backend for a file extension.
inline const FormatBackend& GetBackendForExtension(std::string_view /*ext*/)
{
    // Phase 1: JSON only. Future backends register here.
    static const JsonBackend k_JsonBackend;
    return k_JsonBackend;
}

/// Save: Serialize T -> PropertyTree -> format string -> file.
/// Creates parent directories. Returns false on failure.
template <Serializable T>
bool SaveToFile(const T& value, const std::filesystem::path& path, const FormatBackend& backend)
{
    PropertyTree tree;
    Serialize(tree, value);

    std::string data = backend.WriteToString(tree);

    // Ensure parent directory exists
    if (path.has_parent_path())
    {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec)
        {
            LOG_ERROR_CAT(LogCategory::k_Serialization,
                          "Serialization: failed to create directories for '{}': {}",
                          path.string(),
                          ec.message());
            return false;
        }
    }

    std::ofstream file(path);
    if (!file.is_open())
    {
        LOG_ERROR_CAT(LogCategory::k_Serialization, "Serialization: failed to open '{}' for writing", path.string());
        return false;
    }

    file << data;
    return file.good();
}

/// Save with auto-detected backend (based on file extension).
template <Serializable T> bool SaveToFile(const T& value, const std::filesystem::path& path)
{
    auto ext = path.extension().string();
    return SaveToFile(value, path, GetBackendForExtension(ext));
}

/// Save directly through a logical resource path.
template <Serializable T>
bool SaveToVirtualPath(const T& value, std::string_view virtualPath, const FormatBackend& backend)
{
    const auto path = FileSystem::ResolveWritePath(virtualPath);
    if (!path.has_value())
    {
        LOG_ERROR_CAT(
            LogCategory::k_Serialization, "Serialization: failed to resolve writable virtual path '{}'", virtualPath);
        return false;
    }

    return SaveToFile(value, *path, backend);
}

/// Save with auto-detected backend through a logical resource path.
template <Serializable T> bool SaveToVirtualPath(const T& value, std::string_view virtualPath)
{
    const auto path = FileSystem::ResolveWritePath(virtualPath);
    if (!path.has_value())
    {
        LOG_ERROR_CAT(
            LogCategory::k_Serialization, "Serialization: failed to resolve writable virtual path '{}'", virtualPath);
        return false;
    }

    return SaveToFile(value, *path);
}

/// Load: file -> format string -> PropertyTree -> Deserialize into T.
/// On failure, `value` is unchanged. Returns false on parse or validation error.
template <Serializable T> bool LoadFromFile(T& value, const std::filesystem::path& path, const FormatBackend& backend)
{
    std::ifstream file(path);
    if (!file.is_open())
        return false;

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string data = ss.str();

    return Detail::LoadValueFromString(value, data, backend, path.string());
}

/// Load with auto-detected backend (based on file extension).
template <Serializable T> bool LoadFromFile(T& value, const std::filesystem::path& path)
{
    auto ext = path.extension().string();
    return LoadFromFile(value, path, GetBackendForExtension(ext));
}

/// Load directly through a logical resource path.
template <Serializable T> bool LoadFromVirtualPath(T& value, std::string_view virtualPath, const FormatBackend& backend)
{
    const auto data = FileSystem::ReadText(virtualPath);
    if (!data.has_value())
    {
        LOG_ERROR_CAT(
            LogCategory::k_Serialization, "Serialization: failed to resolve readable virtual path '{}'", virtualPath);
        return false;
    }

    return Detail::LoadValueFromString(value, *data, backend, virtualPath);
}

/// Load with auto-detected backend through a logical resource path.
template <Serializable T> bool LoadFromVirtualPath(T& value, std::string_view virtualPath)
{
    const auto data = FileSystem::ReadText(virtualPath);
    if (!data.has_value())
    {
        LOG_ERROR_CAT(
            LogCategory::k_Serialization, "Serialization: failed to resolve readable virtual path '{}'", virtualPath);
        return false;
    }

    const auto ext = std::filesystem::path(virtualPath).extension().string();
    return Detail::LoadValueFromString(value, *data, GetBackendForExtension(ext), virtualPath);
}

/// Save through the logical config namespace. Writes always target /Saved/Config.
template <Serializable T>
bool SaveToConfigPath(const T& value, std::string_view relativePath, const FormatBackend& backend)
{
    const std::string virtualPath =
        relativePath.empty() ? std::string{"/Saved/Config"} : "/Saved/Config/" + std::string(relativePath);
    return SaveToVirtualPath(value, virtualPath, backend);
}

/// Save through the logical config namespace with auto-detected backend.
template <Serializable T> bool SaveToConfigPath(const T& value, std::string_view relativePath)
{
    const std::string virtualPath =
        relativePath.empty() ? std::string{"/Saved/Config"} : "/Saved/Config/" + std::string(relativePath);
    return SaveToVirtualPath(value, virtualPath);
}

/// Load through the config namespace. Reads apply saved -> project -> engine fallback.
template <Serializable T> bool LoadFromConfigPath(T& value, std::string_view relativePath, const FormatBackend& backend)
{
    const auto data = Detail::ResolveConfigReadText(relativePath);
    if (!data.has_value())
    {
        LOG_ERROR_CAT(LogCategory::k_Serialization, "Serialization: failed to resolve config path '{}'", relativePath);
        return false;
    }

    const std::string sourceLabel = Detail::MakeConfigVirtualPath("/Config", relativePath);
    return Detail::LoadValueFromString(value, *data, backend, sourceLabel);
}

/// Load through the config namespace with auto-detected backend.
template <Serializable T> bool LoadFromConfigPath(T& value, std::string_view relativePath)
{
    const auto data = Detail::ResolveConfigReadText(relativePath);
    if (!data.has_value())
    {
        LOG_ERROR_CAT(LogCategory::k_Serialization, "Serialization: failed to resolve config path '{}'", relativePath);
        return false;
    }

    const std::string sourceLabel = Detail::MakeConfigVirtualPath("/Config", relativePath);
    const auto ext = std::filesystem::path(relativePath).extension().string();
    return Detail::LoadValueFromString(value, *data, GetBackendForExtension(ext), sourceLabel);
}

} // namespace Serialization
