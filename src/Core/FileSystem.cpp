#include "FileSystem.h"

#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "Core/Diagnostics/LogCategories.h"
#include "Core/Diagnostics/LogMacros.h"

#ifdef _WIN32
#include <Windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <unistd.h>
#include <climits>
#endif

std::filesystem::path FileSystem::s_RootPath;
std::filesystem::path FileSystem::s_EngineDir;
std::filesystem::path FileSystem::s_SavedDir;
std::filesystem::path FileSystem::s_CacheDir;
bool FileSystem::s_Initialized = false;
bool FileSystem::s_WritableDirsResolved = false;

static constexpr const char *kAppName = "RTRLab";

namespace
{
    std::string JoinSegments(const std::vector<std::string> &segments, size_t firstSegment)
    {
        std::string joined;
        for (size_t i = firstSegment; i < segments.size(); ++i)
        {
            if (!joined.empty())
                joined += '/';
            joined += segments[i];
        }

        return joined;
    }

    bool HasDocumentExtension(std::string_view relativePath)
    {
        const size_t slashPos = relativePath.find_last_of('/');
        const std::string_view fileName = slashPos == std::string_view::npos
                                              ? relativePath
                                              : relativePath.substr(slashPos + 1);
        if (fileName.empty())
            return false;

        const size_t dotPos = fileName.find_last_of('.');
        return dotPos != std::string_view::npos && dotPos > 0 && dotPos + 1 < fileName.size();
    }
} // namespace

void FileSystem::Init()
{
    s_RootPath = DiscoverRootPath();
    s_EngineDir = s_RootPath / "EngineContent";
    s_Initialized = true;
}

bool FileSystem::IsVirtualPath(std::string_view path)
{
    return ParseVirtualPath(path).has_value();
}

std::optional<FileSystem::VirtualPath> FileSystem::ParseVirtualPath(std::string_view path)
{
    if (path.empty() || path.front() != '/')
        return std::nullopt;

    std::vector<std::string> segments;
    size_t index = 1;
    while (index < path.size())
    {
        while (index < path.size() && path[index] == '/')
            ++index;

        if (index >= path.size())
            break;

        const size_t start = index;
        while (index < path.size() && path[index] != '/')
        {
            if (path[index] == '\\')
                return std::nullopt;
            ++index;
        }

        const std::string segment(path.substr(start, index - start));
        if (segment.empty() || segment == "." || segment == "..")
            return std::nullopt;

        segments.push_back(segment);
    }

    if (segments.empty())
        return std::nullopt;

    VirtualPath virtualPath{};
    if (segments[0] == "Project")
    {
        virtualPath.domain = PathDomain::Project;
        virtualPath.relativePath = JoinSegments(segments, 1);
        return virtualPath;
    }

    if (segments[0] == "Engine")
    {
        virtualPath.domain = PathDomain::Engine;
        virtualPath.relativePath = JoinSegments(segments, 1);
        return virtualPath;
    }

    if (segments[0] == "Saved")
    {
        virtualPath.domain = PathDomain::Saved;
        virtualPath.relativePath = JoinSegments(segments, 1);
        return virtualPath;
    }

    if (segments[0] == "Cache")
    {
        virtualPath.domain = PathDomain::Cache;
        virtualPath.relativePath = JoinSegments(segments, 1);
        return virtualPath;
    }

    if (segments[0] == "Plugins" && segments.size() >= 2)
    {
        virtualPath.domain = PathDomain::Plugin;
        virtualPath.mountName = segments[1];
        virtualPath.relativePath = JoinSegments(segments, 2);
        return virtualPath;
    }

    return std::nullopt;
}

bool FileSystem::IsCatalogBackedPath(std::string_view path)
{
    const auto virtualPath = ParseVirtualPath(path);
    if (!virtualPath.has_value())
        return false;

    switch (virtualPath->domain)
    {
    case PathDomain::Project:
    case PathDomain::Engine:
    case PathDomain::Plugin:
        return !virtualPath->relativePath.empty() && !IsDocumentPath(path);
    case PathDomain::Saved:
    case PathDomain::Cache:
        return false;
    }

    return false;
}

bool FileSystem::IsDocumentPath(std::string_view path)
{
    const auto virtualPath = ParseVirtualPath(path);
    return virtualPath.has_value() && HasDocumentExtension(virtualPath->relativePath);
}

std::optional<std::filesystem::path> FileSystem::ResolveReadPath(std::string_view virtualPathString)
{
    const auto virtualPath = ParseVirtualPath(virtualPathString);
    if (!virtualPath.has_value())
        return std::nullopt;

    return ResolvePhysicalPath(*virtualPath);
}

std::optional<std::filesystem::path> FileSystem::ResolveWritePath(std::string_view virtualPathString)
{
    const auto virtualPath = ParseVirtualPath(virtualPathString);
    if (!virtualPath.has_value())
        return std::nullopt;

    switch (virtualPath->domain)
    {
    case PathDomain::Saved:
    case PathDomain::Cache:
        break;
    case PathDomain::Project:
    case PathDomain::Engine:
    case PathDomain::Plugin:
        return std::nullopt;
    }

    const auto resolved = ResolvePhysicalPath(*virtualPath);
    if (!resolved.has_value())
        return std::nullopt;

    std::error_code ec;
    std::filesystem::create_directories(resolved->parent_path(), ec);
    return resolved;
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

// --- Saved directory ---

void FileSystem::ResolveWritableDirs()
{
    if (s_WritableDirsResolved)
        return;

#ifdef GLAB_ROOT_DIR
    // Development: saved/ in source tree root (persists across clean builds)
    s_SavedDir = std::filesystem::path(GLAB_ROOT_DIR) / "saved";
    s_CacheDir = s_SavedDir / "cache";
#else
    // Release: platform user directory
    s_SavedDir = GetPlatformUserDataDir(kAppName);
    s_CacheDir = GetPlatformCacheDir(kAppName);
#endif

    std::filesystem::create_directories(s_SavedDir / "configs");
    std::filesystem::create_directories(s_CacheDir);
    s_WritableDirsResolved = true;
}

const std::filesystem::path &FileSystem::GetSavedDir()
{
    ResolveWritableDirs();
    return s_SavedDir;
}

const std::filesystem::path &FileSystem::GetCacheDir()
{
    ResolveWritableDirs();
    return s_CacheDir;
}

std::filesystem::path FileSystem::GetSavedPath(std::string_view relativePath)
{
    std::string virtualPath = "/Saved";
    if (!relativePath.empty())
        virtualPath += "/" + std::string(relativePath);

    if (const auto resolved = ResolveWritePath(virtualPath))
        return *resolved;

    return GetSavedDir() / relativePath;
}

std::filesystem::path FileSystem::GetSavedConfigPath(std::string_view relativePath)
{
    std::string virtualPath = "/Saved/Config";
    if (!relativePath.empty())
        virtualPath += "/" + std::string(relativePath);

    if (const auto resolved = ResolveWritePath(virtualPath))
        return *resolved;

    return GetSavedDir() / "configs" / relativePath;
}

std::filesystem::path FileSystem::ResolveConfigPath(std::string_view relativePath)
{
    const std::string logicalConfigPath = relativePath.empty()
                                              ? std::string{}
                                              : "Config/" + std::string(relativePath);

    // 1. User-editable config in /Saved/Config/
    auto savedPath = GetSavedConfigPath(relativePath);
    if (std::filesystem::exists(savedPath))
        return savedPath;

    // 2. Project default in /Project/Config/
    const auto projectDefaultPath = ResolveReadPath("/Project/" + logicalConfigPath);
    if (projectDefaultPath.has_value() && std::filesystem::exists(*projectDefaultPath))
    {
        std::filesystem::create_directories(savedPath.parent_path());
        std::error_code ec;
        std::filesystem::copy_file(*projectDefaultPath, savedPath, ec);
        if (ec)
        {
            LOG_WARN_CAT(LogCategory::FileSystem, "Failed to copy project config '{}' to saved: {}",
                         relativePath, ec.message());
            return *projectDefaultPath;
        }
        LOG_INFO_CAT(LogCategory::FileSystem, "Copied project config to saved: {}", savedPath.string());
        return savedPath;
    }

    // 3. Engine default in /Engine/Config/
    const auto engineDefaultPath = ResolveReadPath("/Engine/" + logicalConfigPath);
    if (engineDefaultPath.has_value() && std::filesystem::exists(*engineDefaultPath))
    {
        std::filesystem::create_directories(savedPath.parent_path());
        std::error_code ec;
        std::filesystem::copy_file(*engineDefaultPath, savedPath, ec);
        if (ec)
        {
            LOG_WARN_CAT(LogCategory::FileSystem, "Failed to copy engine config '{}' to saved: {}",
                         relativePath, ec.message());
            return *engineDefaultPath;
        }
        LOG_INFO_CAT(LogCategory::FileSystem, "Copied engine config to saved: {}", savedPath.string());
        return savedPath;
    }

    // 4. Legacy fallback for callers still thinking in assets/configs/
    auto defaultPath = GetAssetPath("configs") / relativePath;
    if (std::filesystem::exists(defaultPath))
    {
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

    // 5. Not found anywhere
    return {};
}

// --- Platform user data directory ---

std::filesystem::path FileSystem::GetPlatformUserDataDir(std::string_view appName)
{
#ifdef _WIN32
    if (const char *dir = std::getenv("LOCALAPPDATA"))
        return std::filesystem::path(dir) / appName / "Saved";
    // Fallback: next to exe
    return s_RootPath / "saved";

#elif defined(__APPLE__)
    if (const char *home = std::getenv("HOME"))
        return std::filesystem::path(home) / "Library" / "Application Support" / appName / "Saved";
    return s_RootPath / "saved";

#elif defined(__linux__)
    if (const char *xdg = std::getenv("XDG_DATA_HOME"))
        return std::filesystem::path(xdg) / appName / "Saved";
    if (const char *home = std::getenv("HOME"))
        return std::filesystem::path(home) / ".local" / "share" / appName / "Saved";
    return s_RootPath / "saved";

#else
    return s_RootPath / "saved";
#endif
}

std::filesystem::path FileSystem::GetPlatformCacheDir(std::string_view appName)
{
#ifdef _WIN32
    if (const char *dir = std::getenv("LOCALAPPDATA"))
        return std::filesystem::path(dir) / appName / "Cache";
    return s_RootPath / "saved" / "cache";

#elif defined(__APPLE__)
    if (const char *home = std::getenv("HOME"))
        return std::filesystem::path(home) / "Library" / "Caches" / appName;
    return s_RootPath / "saved" / "cache";

#elif defined(__linux__)
    if (const char *xdg = std::getenv("XDG_CACHE_HOME"))
        return std::filesystem::path(xdg) / appName;
    if (const char *home = std::getenv("HOME"))
        return std::filesystem::path(home) / ".cache" / appName;
    return s_RootPath / "saved" / "cache";

#else
    return s_RootPath / "saved" / "cache";
#endif
}

// --- File I/O ---

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

std::filesystem::path FileSystem::GetDomainBasePath(const VirtualPath &virtualPath)
{
    switch (virtualPath.domain)
    {
    case PathDomain::Project:
        return s_RootPath / GLAB_ASSET_DIR;
    case PathDomain::Engine:
        return s_EngineDir;
    case PathDomain::Plugin:
        return s_RootPath / "Plugins" / virtualPath.mountName.value_or(std::string{}) / "Content";
    case PathDomain::Saved:
        return GetSavedDir();
    case PathDomain::Cache:
        return GetCacheDir();
    }

    return {};
}

std::filesystem::path FileSystem::GetPhysicalRelativePath(const VirtualPath &virtualPath)
{
    std::filesystem::path relativePath(virtualPath.relativePath);

    if (!virtualPath.relativePath.empty())
    {
        const std::string configPrefix = "Config/";
        if (virtualPath.relativePath == "Config")
            return std::filesystem::path("configs");

        if (virtualPath.relativePath.rfind(configPrefix, 0) == 0)
            return std::filesystem::path("configs") / virtualPath.relativePath.substr(configPrefix.size());
    }

    return relativePath;
}

std::optional<std::filesystem::path> FileSystem::ResolvePhysicalPath(const VirtualPath &virtualPath)
{
    const auto basePath = GetDomainBasePath(virtualPath);
    if (basePath.empty())
        return std::nullopt;

    const auto relativePath = GetPhysicalRelativePath(virtualPath);
    if (relativePath.empty())
        return basePath;

    return basePath / relativePath;
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
    // 1. Environment variable (highest priority - CI, testing, custom setups)
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

    // 3. Compile-time root (source tree - development with VS debugger CWD)
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
