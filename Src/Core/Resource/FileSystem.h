#pragma once

/// @file Resource/FileSystem.h
/// @brief Cross-platform logical resource paths, mount resolution, and file I/O.

#include "Core/Resource/Catalog/ResourceCatalog.h"
#include "Core/Resource/IO/PhysicalIO.h"
#include "Core/Resource/Path/PathTypes.h"

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

    static std::optional<std::filesystem::path> ResolveReadPath(std::string_view virtualPath);
    static std::optional<std::filesystem::path> ResolveWritePath(std::string_view virtualPath);
    static bool Exists(std::string_view virtualPath);
    static std::optional<std::string> ReadText(std::string_view virtualPath);
    static std::optional<std::vector<uint8_t>> ReadBinary(std::string_view virtualPath);
    static bool WriteText(std::string_view virtualPath, std::string_view data);
    static bool WriteBinary(std::string_view virtualPath, std::span<const uint8_t> data);

    static const std::filesystem::path &GetRootPath();

    static const std::filesystem::path &GetSavedDir();
    static const std::filesystem::path &GetCacheDir();

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
