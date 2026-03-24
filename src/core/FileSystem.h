#pragma once

/// @file FileSystem.h
/// @brief Cross-platform asset path resolution and file I/O.
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
/// ## Writable Configuration
///
/// Runtime-generated config files (input bindings, imgui layout, etc.) live
/// separately from read-only assets. Three storage modes are supported:
///
///   - Portable   — {root}/configs/          (next to exe / assets)
///   - UserLocal  — platform user-local dir  (e.g., %LOCALAPPDATA%/RTRLab/)
///   - UserRoaming— platform roaming dir     (e.g., %APPDATA%/RTRLab/)
///
/// Use ResolveConfigPath() for reading (searches user configs then default
/// assets). Use GetUserConfigPath() for writing (always user-writable dir).

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

/// Where runtime-writable configuration files are stored.
enum class ConfigStorageMode : uint8_t
{
    Portable,    ///< {root}/configs/  — next to exe, good for dev & portable installs
    UserLocal,   ///< Platform user-local dir (not synced across machines)
    UserRoaming  ///< Platform roaming dir (synced in domain environments)
};

class FileSystem
{
public:
    /// Discover and cache the project root. Must be called once at startup.
    static void Init();

    // ── Read-only assets ──────────────────────────────────────────────

    /// Absolute path to the project root (parent of the "assets" directory).
    static const std::filesystem::path &GetRootPath();
    /// Resolve a path relative to the assets directory (e.g., "textures/wood.png").
    static std::filesystem::path GetAssetPath(std::string_view relativePath);

    /// Returns the directory containing build-time compiled shader artifacts.
    /// Looks in {root}/assets/shaders/compiled/ (deployment / POST_BUILD copy),
    /// falling back to the CMake build directory during development.
    static std::filesystem::path GetCompiledShaderDir();

    // ── Writable configuration ────────────────────────────────────────

    /// Set the config storage strategy. Must be called before first config
    /// access (typically right after Init). Defaults to Portable.
    /// Can be overridden by env var RTRL_CONFIG_MODE=portable|local|roaming.
    static void SetConfigStorageMode(ConfigStorageMode mode);
    static ConfigStorageMode GetConfigStorageMode();

    /// Root directory for user-writable config files.
    static const std::filesystem::path &GetUserConfigDir();

    /// Resolve a relative path under the user config directory (writable).
    /// e.g., GetUserConfigPath("input/ShadowMapping.json")
    static std::filesystem::path GetUserConfigPath(std::string_view relativePath);

    /// Search for a config file: user config dir first, then assets/configs/.
    /// Returns the first path that exists, or an empty path if neither found.
    static std::filesystem::path ResolveConfigPath(std::string_view relativePath);

    // ── File I/O utilities ────────────────────────────────────────────

    /// Read an entire text file into a string. Throws on failure.
    static std::string ReadTextFile(const std::filesystem::path &path);
    /// Read an entire binary file into a byte vector. Throws on failure.
    static std::vector<uint8_t> ReadBinaryFile(const std::filesystem::path &path);
    /// Check whether a file or directory exists.
    static bool Exists(const std::filesystem::path &path);

    // ── Platform helpers ──────────────────────────────────────────────

    /// Returns the platform-specific user data directory for an application.
    ///   Windows Local:   %LOCALAPPDATA%/{appName}/
    ///   Windows Roaming: %APPDATA%/{appName}/
    ///   Linux:           $XDG_CONFIG_HOME/{appName}/  (fallback ~/.config/)
    ///   macOS:           ~/Library/Application Support/{appName}/
    static std::filesystem::path GetPlatformUserDataDir(std::string_view appName, bool roaming = false);

private:
    static std::filesystem::path s_RootPath;
    static std::filesystem::path s_UserConfigDir;
    static ConfigStorageMode s_ConfigMode;
    static bool s_Initialized;
    static bool s_ConfigDirResolved;

    /// Try all discovery strategies in priority order and return the first valid root.
    static std::filesystem::path DiscoverRootPath();
    /// Walk up from the executable directory looking for an "assets" folder.
    static std::filesystem::path FindRootFromExecutable();
    /// Resolve and cache the user config directory based on current mode.
    static void ResolveUserConfigDir();
};
