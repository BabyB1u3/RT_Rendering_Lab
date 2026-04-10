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
    constexpr std::string_view kProjectMarkerFileName = ".rtrproject";

    std::filesystem::path GetExecutableDirectory()
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

        return exePath.parent_path();
    }

    bool HasProjectMarker(const std::filesystem::path &directory)
    {
        return std::filesystem::exists(directory / kProjectMarkerFileName);
    }

    std::filesystem::path FindRootFromExecutable()
    {
        std::filesystem::path dir = GetExecutableDirectory();
        if (dir.empty())
            return {};

        constexpr int kMaxDepth = 5;
        for (int i = 0; i < kMaxDepth; ++i)
        {
            if (HasProjectMarker(dir))
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
    std::filesystem::path DiscoverRootPath()
    {
#ifdef RTRL_SHIPPING
        if (const auto exeDir = GetExecutableDirectory(); !exeDir.empty())
            return std::filesystem::canonical(exeDir);

        LOG_ERROR_CAT(LogCategory::FileSystem, "FileSystem: shipping build could not resolve executable directory");
        return std::filesystem::current_path();
#else
        if (const char *envRoot = std::getenv("RTRL_ROOT"))
        {
            std::filesystem::path path(envRoot);
            if (HasProjectMarker(path))
                return std::filesystem::canonical(path);
            LOG_WARN_CAT(LogCategory::FileSystem, "RTRL_ROOT is set to '{}' but no '{}' marker file found there",
                         envRoot, kProjectMarkerFileName);
        }

        {
            const auto root = FindRootFromExecutable();
            if (!root.empty())
                return std::filesystem::canonical(root);
        }

#ifdef GLAB_ROOT_DIR
        {
            std::filesystem::path path(GLAB_ROOT_DIR);
            if (HasProjectMarker(path))
                return std::filesystem::canonical(path);
            LOG_WARN_CAT(LogCategory::FileSystem, "GLAB_ROOT_DIR='{}' does not contain '{}'",
                         GLAB_ROOT_DIR, kProjectMarkerFileName);
        }
#endif

        {
            const auto cwd = std::filesystem::current_path();
            if (HasProjectMarker(cwd))
                return cwd;
        }

        LOG_ERROR_CAT(LogCategory::FileSystem, "FileSystem: could not locate '{}' from any known root",
                      kProjectMarkerFileName);
        return std::filesystem::current_path();
#endif
    }
} // namespace Resource
