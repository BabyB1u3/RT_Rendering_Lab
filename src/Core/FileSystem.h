#pragma once

/// @file FileSystem.h
/// @brief Cross-platform logical resource paths, mount resolution, and file I/O.
///
/// Init() determines the project root once at startup by probing several
/// locations in priority order:
///   1. RTRL_ROOT environment variable    (CI / custom setups)
///   2. Walk up from the executable path  (deployed builds / POST_BUILD copy)
///   3. GLAB_ROOT_DIR compile-time define (in-source dev with VS debugger)
///   4. Current working directory         (last resort)
///
/// After Init(), all mount points are resolved relative to the discovered root,
/// so runtime code can move toward logical paths such as /Project/... and
/// /Saved/... instead of hardcoded physical filesystem joins.
///
/// ## Writable Directories
///
/// Runtime-writable data is split between saved data and disposable cache data:
///
///   - Development (GLAB_ROOT_DIR defined): {source_root}/saved/ and {source_root}/saved/cache/
///   - Release:                             platform user dir (e.g., %LOCALAPPDATA%/RTRLab/Saved/)
///
/// Config resolution follows the logical /Saved/Config -> /Project/Config ->
/// /Engine/Config chain, with a compatibility shim that still maps Config/ to
/// the current physical configs/ folders during migration.

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class FileSystem
{
public:
    enum class PathDomain
    {
        Project,
        Engine,
        Plugin,
        Saved,
        Cache,
    };

    struct VirtualPath
    {
        PathDomain domain;
        std::optional<std::string> mountName;
        std::string relativePath;
    };

    /// Discover and cache the project root. Must be called once at startup.
    static void Init();

    // --- Logical resource paths ---

    /// Returns true when the string is a syntactically valid mounted logical path.
    static bool IsVirtualPath(std::string_view path);
    /// Parses and normalizes a logical path such as /Project/Config/foo.json.
    static std::optional<VirtualPath> ParseVirtualPath(std::string_view path);
    /// Catalog-backed assets are extensionless read-domain paths.
    static bool IsCatalogBackedPath(std::string_view path);
    /// Document paths keep explicit filenames/extensions such as .json or .ini.
    static bool IsDocumentPath(std::string_view path);

    /// Resolve a logical path to a physical read path for the current mount layout.
    static std::optional<std::filesystem::path> ResolveReadPath(std::string_view virtualPath);
    /// Resolve a logical path to a physical write path for writable domains only.
    static std::optional<std::filesystem::path> ResolveWritePath(std::string_view virtualPath);

    // --- Read-only assets ---

    /// Absolute path to the project root (parent of the "assets" directory).
    static const std::filesystem::path &GetRootPath();
    /// Resolve a path relative to the assets directory (e.g., "textures/wood.png").
    static std::filesystem::path GetAssetPath(std::string_view relativePath);

    /// Returns the directory containing build-time compiled shader artifacts.
    static std::filesystem::path GetCompiledShaderDir();

    // --- Saved (writable) ---

    /// Root of the saved directory (user configs, saves, logs, caches).
    static const std::filesystem::path &GetSavedDir();
    /// Root of the cache directory (temporary derived data).
    static const std::filesystem::path &GetCacheDir();
    /// Resolve a relative path under saved/ (e.g., "logs/engine.log").
    static std::filesystem::path GetSavedPath(std::string_view relativePath);
    /// Resolve a relative path under /Saved/Config/ (currently backed by saved/configs/).
    static std::filesystem::path GetSavedConfigPath(std::string_view relativePath);

    /// Search for a config file via /Saved/Config/, /Project/Config/, then
    /// /Engine/Config/. Missing user configs are auto-copied into the saved
    /// config location so they become user-editable.
    static std::filesystem::path ResolveConfigPath(std::string_view relativePath);

    // --- File I/O utilities ---

    /// Read an entire text file into a string. Returns std::nullopt on failure.
    static std::optional<std::string> ReadTextFile(const std::filesystem::path &path);
    /// Read an entire binary file into a byte vector. Returns std::nullopt on failure.
    static std::optional<std::vector<uint8_t>> ReadBinaryFile(const std::filesystem::path &path);
    /// Check whether a file or directory exists.
    static bool Exists(const std::filesystem::path &path);

private:
    static std::filesystem::path s_RootPath;
    static std::filesystem::path s_EngineDir;
    static std::filesystem::path s_SavedDir;
    static std::filesystem::path s_CacheDir;
    static bool s_Initialized;
    static bool s_WritableDirsResolved;

    static std::filesystem::path DiscoverRootPath();
    static std::filesystem::path FindRootFromExecutable();
    static void ResolveWritableDirs();
    static std::filesystem::path GetDomainBasePath(const VirtualPath &virtualPath);
    static std::filesystem::path GetPhysicalRelativePath(const VirtualPath &virtualPath);
    static std::optional<std::filesystem::path> ResolvePhysicalPath(const VirtualPath &virtualPath);

    /// Platform-specific user data directory (e.g., %LOCALAPPDATA%/appName/).
    static std::filesystem::path GetPlatformUserDataDir(std::string_view appName);
    /// Platform-specific cache directory (e.g., %LOCALAPPDATA%/appName/Cache).
    static std::filesystem::path GetPlatformCacheDir(std::string_view appName);
};
