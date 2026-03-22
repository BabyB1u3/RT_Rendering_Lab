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

#include <filesystem>
#include <string>
#include <string_view>

class FileSystem
{
public:
    /// Discover and cache the project root. Must be called once at startup.
    static void Init();

    /// Absolute path to the project root (parent of the "assets" directory).
    static const std::filesystem::path &GetRootPath();
    /// Resolve a path relative to the assets directory (e.g., "textures/wood.png").
    static std::filesystem::path GetAssetPath(std::string_view relativePath);
    /// Shorthand for GetAssetPath("shaders/<name>.glsl"). Appends .glsl if omitted.
    static std::filesystem::path GetShaderPath(std::string_view shaderName);

    /// Read an entire text file into a string. Throws on failure.
    static std::string ReadTextFile(const std::filesystem::path &path);
    /// Check whether a file or directory exists.
    static bool Exists(const std::filesystem::path &path);

private:
    static std::filesystem::path s_RootPath;
    static bool s_Initialized;

    /// Try all discovery strategies in priority order and return the first valid root.
    static std::filesystem::path DiscoverRootPath();
    /// Walk up from the executable directory looking for an "assets" folder.
    static std::filesystem::path FindRootFromExecutable();
};
