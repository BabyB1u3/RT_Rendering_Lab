#pragma once

/// @file Resource/FileSystem.h
/// @brief Cross-platform logical resource paths, mount resolution, and file I/O.

#include "Core/Resource/PathTypes.h"
#include "Core/Resource/ResourceCatalog.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

class FileSystem
{
public:
    using PathDomain = Resource::PathDomain;
    using VirtualPath = Resource::VirtualPath;

    static void Init();
    static void RefreshCatalogs();

    static bool IsVirtualPath(std::string_view path);
    static std::optional<VirtualPath> ParseVirtualPath(std::string_view path);
    static bool IsCatalogBackedPath(std::string_view path);
    static bool IsDocumentPath(std::string_view path);

    static std::optional<std::filesystem::path> ResolveReadPath(std::string_view virtualPath);
    static std::optional<std::filesystem::path> ResolveWritePath(std::string_view virtualPath);
    static bool Exists(std::string_view virtualPath);
    static std::optional<std::string> ReadText(std::string_view virtualPath);
    static std::optional<std::vector<uint8_t>> ReadBinary(std::string_view virtualPath);
    static bool WriteText(std::string_view virtualPath, std::string_view data);
    static bool WriteBinary(std::string_view virtualPath, std::span<const uint8_t> data);

    static const std::filesystem::path &GetRootPath();

    /// Legacy physical-path bridge retained for tests and low-level bootstrap code.
    /// New runtime systems should resolve `/Project/...` through logical-path APIs.
    static std::filesystem::path GetAssetPath(std::string_view relativePath);
    static std::filesystem::path GetCompiledShaderDir();

    static const std::filesystem::path &GetSavedDir();
    static const std::filesystem::path &GetCacheDir();

    /// Legacy physical-path bridge retained for tests and low-level bootstrap code.
    /// New runtime systems should resolve `/Saved/...` through logical-path APIs.
    static std::filesystem::path GetSavedPath(std::string_view relativePath);

    /// Legacy config bridge retained for tests and migration helpers only.
    static std::filesystem::path GetSavedConfigPath(std::string_view relativePath);

    /// Legacy config bridge retained for tests and narrow compatibility helpers.
    /// Higher-level systems should prefer `/Saved/Config/...` logical paths or
    /// `Serialization::LoadFromConfigPath()` / `SaveToConfigPath()`.
    static std::filesystem::path ResolveConfigPath(std::string_view relativePath);

    /// Low-level physical I/O helpers retained for bootstrap and compatibility layers.
    static std::optional<std::string> ReadTextFile(const std::filesystem::path &path);
    static std::optional<std::vector<uint8_t>> ReadBinaryFile(const std::filesystem::path &path);

private:
    static std::filesystem::path s_RootPath;
    static std::filesystem::path s_EngineDir;
    static std::filesystem::path s_SavedDir;
    static std::filesystem::path s_CacheDir;
    static Resource::CatalogRegistry s_CatalogRegistry;
    static bool s_Initialized;
    static bool s_WritableDirsResolved;

    static void ResolveWritableDirs();
};
