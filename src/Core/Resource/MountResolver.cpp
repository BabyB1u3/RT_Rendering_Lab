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
        return rootPath / "saved";
#elif defined(__APPLE__)
        if (const char *home = std::getenv("HOME"))
            return std::filesystem::path(home) / "Library" / "Application Support" / appName / "Saved";
        return rootPath / "saved";
#elif defined(__linux__)
        if (const char *xdg = std::getenv("XDG_DATA_HOME"))
            return std::filesystem::path(xdg) / appName / "Saved";
        if (const char *home = std::getenv("HOME"))
            return std::filesystem::path(home) / ".local" / "share" / appName / "Saved";
        return rootPath / "saved";
#else
        return rootPath / "saved";
#endif
    }

    std::filesystem::path GetPlatformCacheDir(const std::filesystem::path &rootPath, std::string_view appName)
    {
#ifdef _WIN32
        if (const char *dir = std::getenv("LOCALAPPDATA"))
            return std::filesystem::path(dir) / appName / "Cache";
        return rootPath / "saved" / "cache";
#elif defined(__APPLE__)
        if (const char *home = std::getenv("HOME"))
            return std::filesystem::path(home) / "Library" / "Caches" / appName;
        return rootPath / "saved" / "cache";
#elif defined(__linux__)
        if (const char *xdg = std::getenv("XDG_CACHE_HOME"))
            return std::filesystem::path(xdg) / appName;
        if (const char *home = std::getenv("HOME"))
            return std::filesystem::path(home) / ".cache" / appName;
        return rootPath / "saved" / "cache";
#else
        return rootPath / "saved" / "cache";
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
        writableRoots.savedDir = std::filesystem::path(GLAB_ROOT_DIR) / "saved";
        writableRoots.cacheDir = writableRoots.savedDir / "cache";
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
        std::filesystem::path relativePath(virtualPath.relativePath);

        if (!virtualPath.relativePath.empty())
        {
            static constexpr std::string_view kConfigPrefix = "Config/";
            if (virtualPath.relativePath == "Config")
                return std::filesystem::path("configs");

            if (virtualPath.relativePath.rfind(kConfigPrefix, 0) == 0)
                return std::filesystem::path("configs") / virtualPath.relativePath.substr(kConfigPrefix.size());
        }

        return relativePath;
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
