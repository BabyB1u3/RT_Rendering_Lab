#pragma once

/// @file Serialization.h
/// @brief One-call SaveToFile / LoadFromFile convenience layer.
///
/// Ties together FileSystem, format backends, and serialization traits
/// into simple template functions for common use cases.

#include "core/serialization/SerializationTraits.h"
#include "core/serialization/JsonBackend.h"
#include "core/diagnostics/LogCategories.h"
#include "core/diagnostics/LogMacros.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace Serialization
{

    /// Get the default backend for a file extension.
    inline const IFormatBackend &GetBackendForExtension(std::string_view /*ext*/)
    {
        // Phase 1: JSON only. Future backends register here.
        static const JsonBackend s_JsonBackend;
        return s_JsonBackend;
    }

    /// Save: Serialize T → PropertyTree → format string → file.
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

    /// Load: file → format string → PropertyTree → Deserialize into T.
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

} // namespace Serialization
