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
bool FileSystem::s_Initialized = false;

void FileSystem::Init()
{
    s_RootPath = DiscoverRootPath();
    s_Initialized = true;
    LOG_INFO("FileSystem initialized — root: {}", s_RootPath.string());
}

const std::filesystem::path &FileSystem::GetRootPath()
{
    return s_RootPath;
}

std::filesystem::path FileSystem::GetAssetPath(std::string_view relativePath)
{
    return s_RootPath / GLAB_ASSET_DIR / relativePath;
}

std::filesystem::path FileSystem::GetShaderPath(std::string_view shaderName)
{
    std::string filename(shaderName);
    // Append .glsl if the caller didn't include an extension
    if (filename.find('.') == std::string::npos)
        filename += ".glsl";
    return GetAssetPath("shaders/" + filename);
}

std::string FileSystem::ReadTextFile(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in)
        throw std::runtime_error("Failed to open file: " + path.string());

    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
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
