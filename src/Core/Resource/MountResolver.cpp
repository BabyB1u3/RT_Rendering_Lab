#include "Core/Resource/MountResolver.h"

#include <cstdlib>
#include <string>

namespace
{
    std::filesystem::path GetPlatformUserDataDir(const std::filesystem::path &rootPath, std::string_view appName)
    {
#ifdef _WIN32
        if (const char *dir = std::getenv("LOCALAPPDATA"))
            return std::filesystem::path(dir) / appName / "Saved";
        return rootPath / "Saved";
#elif defined(__APPLE__)
        if (const char *home = std::getenv("HOME"))
            return std::filesystem::path(home) / "Library" / "Application Support" / appName / "Saved";
        return rootPath / "Saved";
#elif defined(__linux__)
        if (const char *xdg = std::getenv("XDG_DATA_HOME"))
            return std::filesystem::path(xdg) / appName / "Saved";
        if (const char *home = std::getenv("HOME"))
            return std::filesystem::path(home) / ".local" / "share" / appName / "Saved";
        return rootPath / "Saved";
#else
        return rootPath / "Saved";
#endif
    }

    std::filesystem::path GetPlatformCacheDir(const std::filesystem::path &rootPath, std::string_view appName)
    {
#ifdef _WIN32
        if (const char *dir = std::getenv("LOCALAPPDATA"))
            return std::filesystem::path(dir) / appName / "Cache";
        return rootPath / "Saved" / "Cache";
#elif defined(__APPLE__)
        if (const char *home = std::getenv("HOME"))
            return std::filesystem::path(home) / "Library" / "Caches" / appName;
        return rootPath / "Saved" / "Cache";
#elif defined(__linux__)
        if (const char *xdg = std::getenv("XDG_CACHE_HOME"))
            return std::filesystem::path(xdg) / appName;
        if (const char *home = std::getenv("HOME"))
            return std::filesystem::path(home) / ".cache" / appName;
        return rootPath / "Saved" / "Cache";
#else
        return rootPath / "Saved" / "Cache";
#endif
    }
} // namespace

namespace Resource
{
    WritableRoots ResolveWritableRoots(const std::filesystem::path &rootPath, std::string_view appName)
    {
        WritableRoots writableRoots{};

#ifdef GLAB_ROOT_DIR
        (void)rootPath;
        (void)appName;
        writableRoots.savedDir = std::filesystem::path(GLAB_ROOT_DIR) / "Saved";
        writableRoots.cacheDir = writableRoots.savedDir / "Cache";
#else
        writableRoots.savedDir = GetPlatformUserDataDir(rootPath, appName);
        writableRoots.cacheDir = GetPlatformCacheDir(rootPath, appName);
#endif

        return writableRoots;
    }

    std::filesystem::path GetDomainBasePath(const std::filesystem::path &rootPath,
                                            const std::filesystem::path &engineDir,
                                            const std::filesystem::path &savedDir,
                                            const std::filesystem::path &cacheDir,
                                            const VirtualPath &virtualPath,
                                            std::string_view assetDirName)
    {
        switch (virtualPath.domain)
        {
        case PathDomain::Project:
            return rootPath / assetDirName;
        case PathDomain::Engine:
            return engineDir;
        case PathDomain::Plugin:
            return rootPath / "Plugins" / virtualPath.mountName.value_or(std::string{}) / "Content";
        case PathDomain::Saved:
            return savedDir;
        case PathDomain::Cache:
            return cacheDir;
        }

        return {};
    }

    std::filesystem::path GetPhysicalRelativePath(const VirtualPath &virtualPath)
    {
        return std::filesystem::path(virtualPath.relativePath);
    }

    std::optional<std::filesystem::path> ResolvePhysicalPath(const std::filesystem::path &rootPath,
                                                             const std::filesystem::path &engineDir,
                                                             const std::filesystem::path &savedDir,
                                                             const std::filesystem::path &cacheDir,
                                                             const VirtualPath &virtualPath,
                                                             std::string_view assetDirName)
    {
        const auto basePath = GetDomainBasePath(rootPath, engineDir, savedDir, cacheDir, virtualPath, assetDirName);
        if (basePath.empty())
            return std::nullopt;

        const auto relativePath = GetPhysicalRelativePath(virtualPath);
        if (relativePath.empty())
            return basePath;

        return basePath / relativePath;
    }
} // namespace Resource
