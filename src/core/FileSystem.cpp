#include "FileSystem.h"

#include <fstream>
#include <optional>
#include <sstream>

#include "core/diagnostics/LogCategories.h"
#include "core/diagnostics/LogMacros.h"

#ifdef _WIN32
#include <Windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <unistd.h>
#include <climits>
#endif

std::filesystem::path FileSystem::s_RootPath;
std::filesystem::path FileSystem::s_SavedDir;
bool FileSystem::s_Initialized = false;
bool FileSystem::s_SavedDirResolved = false;

static constexpr const char *kAppName = "RTRLab";

void FileSystem::Init()
{
    s_RootPath = DiscoverRootPath();
    s_Initialized = true;
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
    auto assetDir = s_RootPath / GLAB_ASSET_DIR / "shaders" / "compiled";
    if (std::filesystem::exists(assetDir))
        return assetDir;

#ifdef GLAB_SHADER_BUILD_DIR
    std::filesystem::path buildDir(GLAB_SHADER_BUILD_DIR);
    if (std::filesystem::exists(buildDir))
        return buildDir;
#endif

    return assetDir;
}

// ── Saved directory ──────────────────────────────────────────────────

void FileSystem::ResolveSavedDir()
{
    if (s_SavedDirResolved)
        return;

#ifdef GLAB_ROOT_DIR
    // Development: saved/ in source tree root (persists across clean builds)
    s_SavedDir = std::filesystem::path(GLAB_ROOT_DIR) / "saved";
#else
    // Release: platform user directory
    s_SavedDir = GetPlatformUserDataDir(kAppName);
#endif

    std::filesystem::create_directories(s_SavedDir / "configs");
    s_SavedDirResolved = true;
}

const std::filesystem::path &FileSystem::GetSavedDir()
{
    ResolveSavedDir();
    return s_SavedDir;
}

std::filesystem::path FileSystem::GetSavedPath(std::string_view relativePath)
{
    return GetSavedDir() / relativePath;
}

std::filesystem::path FileSystem::GetSavedConfigPath(std::string_view relativePath)
{
    return GetSavedDir() / "configs" / relativePath;
}

std::filesystem::path FileSystem::ResolveConfigPath(std::string_view relativePath)
{
    // 1. User-editable config in saved/configs/
    auto savedPath = GetSavedConfigPath(relativePath);
    if (std::filesystem::exists(savedPath))
        return savedPath;

    // 2. Shipped default in assets/configs/
    auto defaultPath = GetAssetPath("configs") / relativePath;
    if (std::filesystem::exists(defaultPath))
    {
        // Auto-copy to saved/configs/ so the user has an editable file
        std::filesystem::create_directories(savedPath.parent_path());
        std::error_code ec;
        std::filesystem::copy_file(defaultPath, savedPath, ec);
        if (ec)
        {
            LOG_WARN_CAT(LogCategory::FileSystem, "Failed to copy default config '{}' to saved: {}",
                         relativePath, ec.message());
            return defaultPath; // still usable, just not editable in-place
        }
        LOG_INFO_CAT(LogCategory::FileSystem, "Copied default config to saved: {}", savedPath.string());
        return savedPath;
    }

    // 3. Not found anywhere
    return {};
}

// ── Platform user data directory ─────────────────────────────────────

std::filesystem::path FileSystem::GetPlatformUserDataDir(std::string_view appName)
{
#ifdef _WIN32
    if (const char *dir = std::getenv("LOCALAPPDATA"))
        return std::filesystem::path(dir) / appName;
    // Fallback: next to exe
    return s_RootPath / "saved";

#elif defined(__APPLE__)
    if (const char *home = std::getenv("HOME"))
        return std::filesystem::path(home) / "Library" / "Application Support" / appName;
    return s_RootPath / "saved";

#elif defined(__linux__)
    if (const char *xdg = std::getenv("XDG_DATA_HOME"))
        return std::filesystem::path(xdg) / appName;
    if (const char *home = std::getenv("HOME"))
        return std::filesystem::path(home) / ".local" / "share" / appName;
    return s_RootPath / "saved";

#else
    return s_RootPath / "saved";
#endif
}

// ── File I/O ─────────────────────────────────────────────────────────

std::optional<std::string> FileSystem::ReadTextFile(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in)
    {
        LOG_ERROR_CAT(LogCategory::FileSystem, "Failed to open text file: {}", path.string());
        return std::nullopt;
    }

    std::ostringstream ss;
    ss << in.rdbuf();
    if (!in.good() && !in.eof())
    {
        LOG_ERROR_CAT(LogCategory::FileSystem, "Failed while reading text file: {}", path.string());
        return std::nullopt;
    }

    return ss.str();
}

std::optional<std::vector<uint8_t>> FileSystem::ReadBinaryFile(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::in | std::ios::binary | std::ios::ate);
    if (!in)
    {
        LOG_ERROR_CAT(LogCategory::FileSystem, "Failed to open binary file: {}", path.string());
        return std::nullopt;
    }

    auto size = in.tellg();
    if (size < 0)
    {
        LOG_ERROR_CAT(LogCategory::FileSystem, "Failed to query binary file size: {}", path.string());
        return std::nullopt;
    }

    in.seekg(0, std::ios::beg);

    std::vector<uint8_t> data(static_cast<size_t>(size));
    if (!data.empty())
        in.read(reinterpret_cast<char *>(data.data()), size);

    if (in.fail())
    {
        LOG_ERROR_CAT(LogCategory::FileSystem, "Failed while reading binary file: {}", path.string());
        return std::nullopt;
    }

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
        LOG_WARN_CAT(LogCategory::FileSystem, "RTRL_ROOT is set to '{}' but no '{}' directory found there", envRoot, GLAB_ASSET_DIR);
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
        LOG_WARN_CAT(LogCategory::FileSystem, "GLAB_ROOT_DIR='{}' does not contain '{}'", GLAB_ROOT_DIR, GLAB_ASSET_DIR);
    }
#endif

    // 4. Current working directory (last resort)
    {
        auto cwd = std::filesystem::current_path();
        if (std::filesystem::exists(cwd / GLAB_ASSET_DIR))
            return cwd;
    }

    LOG_ERROR_CAT(LogCategory::FileSystem, "FileSystem: could not locate '{}' directory from any known root", GLAB_ASSET_DIR);
    return std::filesystem::current_path();
}
