#include "FileSystem.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

#include "Logger.h"

#ifdef _WIN32
#include <Windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <unistd.h>
#include <climits>
#endif

std::filesystem::path FileSystem::s_RootPath;
std::filesystem::path FileSystem::s_UserConfigDir;
ConfigStorageMode FileSystem::s_ConfigMode = ConfigStorageMode::Portable;
bool FileSystem::s_Initialized = false;
bool FileSystem::s_ConfigDirResolved = false;

static constexpr const char *kAppName = "RTRLab";

void FileSystem::Init()
{
    s_RootPath = DiscoverRootPath();
    s_Initialized = true;

    // Check env var for config mode override
    if (const char *envMode = std::getenv("RTRL_CONFIG_MODE"))
    {
        std::string mode(envMode);
        if (mode == "portable")
            s_ConfigMode = ConfigStorageMode::Portable;
        else if (mode == "local")
            s_ConfigMode = ConfigStorageMode::UserLocal;
        else if (mode == "roaming")
            s_ConfigMode = ConfigStorageMode::UserRoaming;
        else
            LOG_WARN("Unknown RTRL_CONFIG_MODE='{}', using Portable", mode);
    }

    LOG_INFO("FileSystem initialized - root: {}", s_RootPath.string());
}

const std::filesystem::path &FileSystem::GetRootPath()
{
    return s_RootPath;
}

std::filesystem::path FileSystem::GetAssetPath(std::string_view relativePath)
{
    return s_RootPath / GLAB_ASSET_DIR / relativePath;
}

std::filesystem::path FileSystem::GetCompiledShaderDir()
{
    // Primary: assets/shaders/compiled/ (deployment and POST_BUILD copy)
    auto assetDir = s_RootPath / GLAB_ASSET_DIR / "shaders" / "compiled";
    if (std::filesystem::exists(assetDir))
        return assetDir;

#ifdef GLAB_SHADER_BUILD_DIR
    // Fallback: CMake build directory (development, before first POST_BUILD)
    std::filesystem::path buildDir(GLAB_SHADER_BUILD_DIR);
    if (std::filesystem::exists(buildDir))
        return buildDir;
#endif

    return assetDir; // return canonical path even if missing (error reported at load time)
}

// ── Writable configuration ────────────────────────────────────────────

void FileSystem::SetConfigStorageMode(ConfigStorageMode mode)
{
    s_ConfigMode = mode;
    s_ConfigDirResolved = false; // force re-resolve on next access
}

ConfigStorageMode FileSystem::GetConfigStorageMode()
{
    return s_ConfigMode;
}

void FileSystem::ResolveUserConfigDir()
{
    if (s_ConfigDirResolved)
        return;

    switch (s_ConfigMode)
    {
    case ConfigStorageMode::Portable:
        s_UserConfigDir = s_RootPath / "configs";
        break;
    case ConfigStorageMode::UserLocal:
        s_UserConfigDir = GetPlatformUserDataDir(kAppName, /*roaming=*/false);
        break;
    case ConfigStorageMode::UserRoaming:
        s_UserConfigDir = GetPlatformUserDataDir(kAppName, /*roaming=*/true);
        break;
    }

    s_ConfigDirResolved = true;
    LOG_INFO("Config directory ({}): {}",
             s_ConfigMode == ConfigStorageMode::Portable  ? "Portable" :
             s_ConfigMode == ConfigStorageMode::UserLocal  ? "UserLocal" :
                                                            "UserRoaming",
             s_UserConfigDir.string());
}

const std::filesystem::path &FileSystem::GetUserConfigDir()
{
    ResolveUserConfigDir();
    return s_UserConfigDir;
}

std::filesystem::path FileSystem::GetUserConfigPath(std::string_view relativePath)
{
    return GetUserConfigDir() / relativePath;
}

std::filesystem::path FileSystem::ResolveConfigPath(std::string_view relativePath)
{
    // 1. User-writable config dir (may have user overrides)
    auto userPath = GetUserConfigPath(relativePath);
    if (std::filesystem::exists(userPath))
        return userPath;

    // 2. Default configs shipped with assets
    auto defaultPath = GetAssetPath("configs") / relativePath;
    if (std::filesystem::exists(defaultPath))
        return defaultPath;

    // 3. Not found
    return {};
}

// ── Platform user data directory ──────────────────────────────────────

std::filesystem::path FileSystem::GetPlatformUserDataDir(std::string_view appName, bool roaming)
{
#ifdef _WIN32
    const char *envVar = roaming ? "APPDATA" : "LOCALAPPDATA";
    if (const char *dir = std::getenv(envVar))
        return std::filesystem::path(dir) / appName;
    // Fallback: next to exe
    return s_RootPath / "configs";

#elif defined(__APPLE__)
    if (const char *home = std::getenv("HOME"))
        return std::filesystem::path(home) / "Library" / "Application Support" / appName;
    return s_RootPath / "configs";

#elif defined(__linux__)
    if (const char *xdg = std::getenv("XDG_CONFIG_HOME"))
        return std::filesystem::path(xdg) / appName;
    if (const char *home = std::getenv("HOME"))
        return std::filesystem::path(home) / ".config" / appName;
    return s_RootPath / "configs";

#else
    return s_RootPath / "configs";
#endif
}

// ── File I/O ──────────────────────────────────────────────────────────

std::string FileSystem::ReadTextFile(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in)
        throw std::runtime_error("Failed to open file: " + path.string());

    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::vector<uint8_t> FileSystem::ReadBinaryFile(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::in | std::ios::binary | std::ios::ate);
    if (!in)
        throw std::runtime_error("Failed to open binary file: " + path.string());

    auto size = in.tellg();
    in.seekg(0, std::ios::beg);

    std::vector<uint8_t> data(static_cast<size_t>(size));
    in.read(reinterpret_cast<char *>(data.data()), size);
    return data;
}

bool FileSystem::Exists(const std::filesystem::path &path)
{
    return std::filesystem::exists(path);
}

std::filesystem::path FileSystem::FindRootFromExecutable()
{
    std::filesystem::path exePath;

#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (len > 0 && len < MAX_PATH)
        exePath = std::filesystem::path(buf);
#elif defined(__APPLE__)
    char buf[PATH_MAX];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) == 0)
        exePath = std::filesystem::canonical(buf);
#elif defined(__linux__)
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0)
    {
        buf[len] = '\0';
        exePath = std::filesystem::path(buf);
    }
#endif

    if (exePath.empty())
        return {};

    // Walk up from the executable directory, looking for an "assets" folder
    std::filesystem::path dir = exePath.parent_path();
    constexpr int kMaxDepth = 5;
    for (int i = 0; i < kMaxDepth; ++i)
    {
        if (std::filesystem::exists(dir / GLAB_ASSET_DIR))
            return dir;
        auto parent = dir.parent_path();
        if (parent == dir)
            break;
        dir = parent;
    }

    return {};
}

std::filesystem::path FileSystem::DiscoverRootPath()
{
    // 1. Environment variable (highest priority — CI, testing, custom setups)
    if (const char *envRoot = std::getenv("RTRL_ROOT"))
    {
        std::filesystem::path p(envRoot);
        if (std::filesystem::exists(p / GLAB_ASSET_DIR))
            return std::filesystem::canonical(p);
        LOG_WARN("RTRL_ROOT is set to '{}' but no '{}' directory found there", envRoot, GLAB_ASSET_DIR);
    }

    // 2. Executable directory / walk up (deployment and POST_BUILD copy)
    {
        auto root = FindRootFromExecutable();
        if (!root.empty())
            return std::filesystem::canonical(root);
    }

    // 3. Compile-time root (source tree — development with VS debugger CWD)
#ifdef GLAB_ROOT_DIR
    {
        std::filesystem::path p(GLAB_ROOT_DIR);
        if (std::filesystem::exists(p / GLAB_ASSET_DIR))
            return std::filesystem::canonical(p);
        LOG_WARN("GLAB_ROOT_DIR='{}' does not contain '{}'", GLAB_ROOT_DIR, GLAB_ASSET_DIR);
    }
#endif

    // 4. Current working directory (last resort)
    {
        auto cwd = std::filesystem::current_path();
        if (std::filesystem::exists(cwd / GLAB_ASSET_DIR))
            return cwd;
    }

    LOG_ERROR("FileSystem: could not locate '{}' directory from any known root", GLAB_ASSET_DIR);
    return std::filesystem::current_path();
}
