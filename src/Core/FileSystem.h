#pragma once

/// @file FileSystem.h
/// @brief Cross-platform asset path resolution, saved-data directory, and file I/O.
///
/// Init() determines the project root once at startup by probing several
/// locations in priority order:
///   1. RTRL_ROOT environment variable    (CI / custom setups)
///   2. Walk up from the executable path  (deployed builds / POST_BUILD copy)
///   3. GLAB_ROOT_DIR compile-time define (in-source dev with VS debugger)
///   4. Current working directory         (last resort)
///
/// After Init(), all asset/shader paths are resolved relative to the
/// discovered root, so neither demos nor render passes need hardcoded paths.
///
/// ## Saved Directory
///
/// All runtime-writable files (user configs, saves, logs, caches) live under
/// a single "saved" directory:
///
///   - Development (GLAB_ROOT_DIR defined): {source_root}/saved/
///   - Release:                             platform user dir (e.g., %LOCALAPPDATA%/RTRLab/)
///
/// Config resolution searches saved/configs/ first, then assets/configs/
/// (shipped defaults). On first access, missing user configs are auto-copied
/// from shipped defaults so users always have an editable file.

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class FileSystem
{
public:
    /// Discover and cache the project root. Must be called once at startup.
    static void Init();

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
    /// Resolve a relative path under saved/ (e.g., "logs/engine.log").
    static std::filesystem::path GetSavedPath(std::string_view relativePath);
    /// Resolve a relative path under saved/configs/ (e.g., "input/ShadowMapping.json").
    static std::filesystem::path GetSavedConfigPath(std::string_view relativePath);

    /// Search for a config file: saved/configs/ first, then assets/configs/.
    /// If found only in assets/configs/, auto-copies to saved/configs/ so it
    /// becomes user-editable. Returns the resolved path, or empty if not found.
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
    static std::filesystem::path s_SavedDir;
    static bool s_Initialized;
    static bool s_SavedDirResolved;

    static std::filesystem::path DiscoverRootPath();
    static std::filesystem::path FindRootFromExecutable();
    static void ResolveSavedDir();

    /// Platform-specific user data directory (e.g., %LOCALAPPDATA%/appName/).
    static std::filesystem::path GetPlatformUserDataDir(std::string_view appName);
};
