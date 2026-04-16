#include "Core/Resource/Mount/RootDiscovery.h"

#include "Core/Diagnostics/Logging/LogCategories.h"
#include "Core/Diagnostics/Logging/LogMacros.h"
#include "Core/Util/CommandLine.h"

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
constexpr std::string_view k_ProjectMarkerFileName = ".rtrproject";

std::optional<std::string_view> GetRootOverride()
{
#ifdef RTRLAB_CONFIG_RELEASE
    if (!Util::ProcessHasOption("dev-mode"))
        return std::nullopt;

    if (const auto cliOverride = Util::GetProcessOptionValue("root"); cliOverride.has_value() && !cliOverride->empty())
        return cliOverride;

    return std::nullopt;
#else
    if (const auto cliOverride = Util::GetProcessOptionValue("root"); cliOverride.has_value() && !cliOverride->empty())
        return cliOverride;

    if (const char* envRoot = std::getenv("RTRL_ROOT"))
    {
        const std::string_view value = envRoot;
        if (!value.empty())
            return value;
    }

    return std::nullopt;
#endif
}

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

bool HasProjectMarker(const std::filesystem::path& directory)
{
    return std::filesystem::exists(directory / k_ProjectMarkerFileName);
}

std::filesystem::path FindRootFromExecutable()
{
    std::filesystem::path dir = GetExecutableDirectory();
    if (dir.empty())
        return {};

    constexpr int k_MaxDepth = 5;
    for (int i = 0; i < k_MaxDepth; ++i)
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
#ifdef RTRLAB_CONFIG_RELEASE
    if (const auto rootOverride = GetRootOverride(); rootOverride.has_value())
    {
        std::filesystem::path overridePath{std::string(*rootOverride)};
        if (HasProjectMarker(overridePath))
            return std::filesystem::canonical(overridePath);
        LOG_WARN_CAT(LogCategory::FileSystem,
                     "Root override is set to '{}' but no '{}' marker file found there",
                     *rootOverride,
                     k_ProjectMarkerFileName);
    }

    if (const auto exeDir = GetExecutableDirectory(); !exeDir.empty())
        return std::filesystem::canonical(exeDir);

    LOG_ERROR_CAT(LogCategory::FileSystem, "FileSystem: shipping build could not resolve executable directory");
    return std::filesystem::current_path();
#else
    if (const auto rootOverride = GetRootOverride(); rootOverride.has_value())
    {
        std::filesystem::path overridePath{std::string(*rootOverride)};
        if (HasProjectMarker(overridePath))
            return std::filesystem::canonical(overridePath);
        LOG_WARN_CAT(LogCategory::FileSystem,
                     "Root override is set to '{}' but no '{}' marker file found there",
                     *rootOverride,
                     k_ProjectMarkerFileName);
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
        LOG_WARN_CAT(LogCategory::FileSystem,
                     "GLAB_ROOT_DIR='{}' does not contain '{}'",
                     GLAB_ROOT_DIR,
                     k_ProjectMarkerFileName);
    }
#endif

    {
        const auto cwd = std::filesystem::current_path();
        if (HasProjectMarker(cwd))
            return cwd;
    }

    LOG_ERROR_CAT(
        LogCategory::FileSystem, "FileSystem: could not locate '{}' from any known root", k_ProjectMarkerFileName);
    return std::filesystem::current_path();
#endif
}
} // namespace Resource
