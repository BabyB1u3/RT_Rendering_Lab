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
    namespace detail
    {
        inline std::string MakeConfigVirtualPath(std::string_view mountRoot, std::string_view relativePath)
        {
            if (relativePath.empty())
                return std::string(mountRoot);

            return std::string(mountRoot) + "/" + std::string(relativePath);
        }

        inline std::optional<std::filesystem::path> ResolveConfigReadPath(std::string_view relativePath)
        {
            const std::string savedVirtualPath = MakeConfigVirtualPath("/Saved/Config", relativePath);
            const std::string projectVirtualPath = MakeConfigVirtualPath("/Project/Config", relativePath);
            const std::string engineVirtualPath = MakeConfigVirtualPath("/Engine/Config", relativePath);

            if (const auto savedPath = FileSystem::ResolveReadPath(savedVirtualPath);
                savedPath.has_value() && std::filesystem::exists(*savedPath))
            {
                return savedPath;
            }

            const auto seedToSaved = [&](std::string_view sourceVirtualPath, std::string_view sourceLabel)
                -> std::optional<std::filesystem::path>
            {
                const auto sourcePath = FileSystem::ResolveReadPath(sourceVirtualPath);
                if (!sourcePath.has_value() || !std::filesystem::exists(*sourcePath))
                    return std::nullopt;

                const auto savedPath = FileSystem::ResolveWritePath(savedVirtualPath);
                if (!savedPath.has_value())
                    return sourcePath;

                std::error_code ec;
                std::filesystem::copy_file(*sourcePath, *savedPath, ec);
                if (ec)
                {
                    LOG_WARN_CAT(LogCategory::Serialization,
                                 "Serialization: failed to seed {} config '{}' into saved path '{}': {}",
                                 sourceLabel,
                                 relativePath,
                                 savedPath->string(),
                                 ec.message());
                    return sourcePath;
                }

                return savedPath;
            };

            if (const auto seededProjectPath = seedToSaved(projectVirtualPath, "project"); seededProjectPath.has_value())
                return seededProjectPath;

            if (const auto seededEnginePath = seedToSaved(engineVirtualPath, "engine"); seededEnginePath.has_value())
                return seededEnginePath;

            return std::nullopt;
        }
    } // namespace detail

    /// Get the default backend for a file extension.
    inline const IFormatBackend &GetBackendForExtension(std::string_view /*ext*/)
    {
        // Phase 1: JSON only. Future backends register here.
        static const JsonBackend s_JsonBackend;
        return s_JsonBackend;
    }

    /// Save: Serialize T -> PropertyTree -> format string -> file.
    /// Creates parent directories. Returns false on failure.
    template <Serializable T>
    bool SaveToFile(const T &value, const std::filesystem::path &path,
                    const IFormatBackend &backend)
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
                LOG_ERROR_CAT(LogCategory::Serialization, "Serialization: failed to create directories for '{}': {}",
                              path.string(), ec.message());
                return false;
            }
        }

        std::ofstream file(path);
        if (!file.is_open())
        {
            LOG_ERROR_CAT(LogCategory::Serialization, "Serialization: failed to open '{}' for writing", path.string());
            return false;
        }

        file << data;
        return file.good();
    }

    /// Save with auto-detected backend (based on file extension).
    template <Serializable T>
    bool SaveToFile(const T &value, const std::filesystem::path &path)
    {
        auto ext = path.extension().string();
        return SaveToFile(value, path, GetBackendForExtension(ext));
    }

    /// Save directly through a logical resource path.
    template <Serializable T>
    bool SaveToVirtualPath(const T &value, std::string_view virtualPath,
                           const IFormatBackend &backend)
    {
        const auto path = FileSystem::ResolveWritePath(virtualPath);
        if (!path.has_value())
        {
            LOG_ERROR_CAT(LogCategory::Serialization,
                          "Serialization: failed to resolve writable virtual path '{}'",
                          virtualPath);
            return false;
        }

        return SaveToFile(value, *path, backend);
    }

    /// Save with auto-detected backend through a logical resource path.
    template <Serializable T>
    bool SaveToVirtualPath(const T &value, std::string_view virtualPath)
    {
        const auto path = FileSystem::ResolveWritePath(virtualPath);
        if (!path.has_value())
        {
            LOG_ERROR_CAT(LogCategory::Serialization,
                          "Serialization: failed to resolve writable virtual path '{}'",
                          virtualPath);
            return false;
        }

        return SaveToFile(value, *path);
    }

    /// Load: file -> format string -> PropertyTree -> Deserialize into T.
    /// On failure, `value` is unchanged. Returns false on parse or validation error.
    template <Serializable T>
    bool LoadFromFile(T &value, const std::filesystem::path &path,
                      const IFormatBackend &backend)
    {
        std::ifstream file(path);
        if (!file.is_open())
            return false;

        std::ostringstream ss;
        ss << file.rdbuf();
        std::string data = ss.str();

        PropertyTree tree;
        if (!backend.ReadFromString(data, tree))
        {
            LOG_ERROR_CAT(LogCategory::Serialization, "Serialization: failed to parse '{}'", path.string());
            return false;
        }

        // Deserialize into a temporary - only commit on success
        T temp{};
        if (!Deserialize(tree, temp))
        {
            LOG_ERROR_CAT(LogCategory::Serialization, "Serialization: failed to deserialize '{}'", path.string());
            return false;
        }

        value = std::move(temp);
        return true;
    }

    /// Load with auto-detected backend (based on file extension).
    template <Serializable T>
    bool LoadFromFile(T &value, const std::filesystem::path &path)
    {
        auto ext = path.extension().string();
        return LoadFromFile(value, path, GetBackendForExtension(ext));
    }

    /// Load directly through a logical resource path.
    template <Serializable T>
    bool LoadFromVirtualPath(T &value, std::string_view virtualPath,
                             const IFormatBackend &backend)
    {
        const auto path = FileSystem::ResolveReadPath(virtualPath);
        if (!path.has_value())
        {
            LOG_ERROR_CAT(LogCategory::Serialization,
                          "Serialization: failed to resolve readable virtual path '{}'",
                          virtualPath);
            return false;
        }

        return LoadFromFile(value, *path, backend);
    }

    /// Load with auto-detected backend through a logical resource path.
    template <Serializable T>
    bool LoadFromVirtualPath(T &value, std::string_view virtualPath)
    {
        const auto path = FileSystem::ResolveReadPath(virtualPath);
        if (!path.has_value())
        {
            LOG_ERROR_CAT(LogCategory::Serialization,
                          "Serialization: failed to resolve readable virtual path '{}'",
                          virtualPath);
            return false;
        }

        return LoadFromFile(value, *path);
    }

    /// Save through the logical config namespace. Writes always target /Saved/Config.
    template <Serializable T>
    bool SaveToConfigPath(const T &value, std::string_view relativePath,
                          const IFormatBackend &backend)
    {
        const std::string virtualPath = relativePath.empty()
                                            ? std::string{"/Saved/Config"}
                                            : "/Saved/Config/" + std::string(relativePath);
        return SaveToVirtualPath(value, virtualPath, backend);
    }

    /// Save through the logical config namespace with auto-detected backend.
    template <Serializable T>
    bool SaveToConfigPath(const T &value, std::string_view relativePath)
    {
        const std::string virtualPath = relativePath.empty()
                                            ? std::string{"/Saved/Config"}
                                            : "/Saved/Config/" + std::string(relativePath);
        return SaveToVirtualPath(value, virtualPath);
    }

    /// Load through the config namespace. Reads apply saved -> project -> engine fallback.
    template <Serializable T>
    bool LoadFromConfigPath(T &value, std::string_view relativePath,
                            const IFormatBackend &backend)
    {
        const auto path = detail::ResolveConfigReadPath(relativePath);
        if (!path.has_value())
        {
            LOG_ERROR_CAT(LogCategory::Serialization,
                          "Serialization: failed to resolve config path '{}'",
                          relativePath);
            return false;
        }

        return LoadFromFile(value, *path, backend);
    }

    /// Load through the config namespace with auto-detected backend.
    template <Serializable T>
    bool LoadFromConfigPath(T &value, std::string_view relativePath)
    {
        const auto path = detail::ResolveConfigReadPath(relativePath);
        if (!path.has_value())
        {
            LOG_ERROR_CAT(LogCategory::Serialization,
                          "Serialization: failed to resolve config path '{}'",
                          relativePath);
            return false;
        }

        return LoadFromFile(value, *path);
    }

} // namespace Serialization
