#include "Core/Resource/Mount/RootDiscovery.h"

#include "Core/Diagnostics/Logging/LogCategories.h"
#include "Core/Diagnostics/Logging/LogMacros.h"

#include <cstdlib>

#ifdef _WIN32
#include <Windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <unistd.h>
#include <climits>
#endif

namespace
{
    std::filesystem::path FindRootFromExecutable(std::string_view assetDirName)
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
            if (std::filesystem::exists(dir / assetDirName))
                return dir;
            const auto parent = dir.parent_path();
            if (parent == dir)
                break;
            dir = parent;
        }

        return {};
    }
} // namespace

namespace Resource
{
    std::filesystem::path DiscoverRootPath(std::string_view assetDirName)
    {
        if (const char *envRoot = std::getenv("RTRL_ROOT"))
        {
            std::filesystem::path path(envRoot);
            if (std::filesystem::exists(path / assetDirName))
                return std::filesystem::canonical(path);
            LOG_WARN_CAT(LogCategory::FileSystem, "RTRL_ROOT is set to '{}' but no '{}' directory found there",
                         envRoot, assetDirName);
        }

        {
            const auto root = FindRootFromExecutable(assetDirName);
            if (!root.empty())
                return std::filesystem::canonical(root);
        }

#ifdef GLAB_ROOT_DIR
        {
            std::filesystem::path path(GLAB_ROOT_DIR);
            if (std::filesystem::exists(path / assetDirName))
                return std::filesystem::canonical(path);
            LOG_WARN_CAT(LogCategory::FileSystem, "GLAB_ROOT_DIR='{}' does not contain '{}'",
                         GLAB_ROOT_DIR, assetDirName);
        }
#endif

        {
            const auto cwd = std::filesystem::current_path();
            if (std::filesystem::exists(cwd / assetDirName))
                return cwd;
        }

        LOG_ERROR_CAT(LogCategory::FileSystem, "FileSystem: could not locate '{}' directory from any known root",
                      assetDirName);
        return std::filesystem::current_path();
    }
} // namespace Resource
