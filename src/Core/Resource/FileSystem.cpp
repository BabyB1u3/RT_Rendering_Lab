#include "Core/Resource/FileSystem.h"

#include "Core/Diagnostics/LogCategories.h"
#include "Core/Diagnostics/LogMacros.h"
#include "Core/Resource/ConfigResolver.h"
#include "Core/Resource/MountResolver.h"
#include "Core/Resource/PathParser.h"
#include "Core/Resource/PhysicalIO.h"
#include "Core/Resource/RootDiscovery.h"

std::filesystem::path FileSystem::s_RootPath;
std::filesystem::path FileSystem::s_EngineDir;
std::filesystem::path FileSystem::s_SavedDir;
std::filesystem::path FileSystem::s_CacheDir;
bool FileSystem::s_Initialized = false;
bool FileSystem::s_WritableDirsResolved = false;

namespace
{
    static constexpr std::string_view kProjectContentDirName = "Content";
    static constexpr const char *kAppName = "RTRLab";
} // namespace

void FileSystem::Init()
{
    s_RootPath = Resource::DiscoverRootPath(kProjectContentDirName);
    s_EngineDir = s_RootPath / "EngineContent";
    s_Initialized = true;
}

bool FileSystem::IsVirtualPath(std::string_view path)
{
    return Resource::IsVirtualPath(path);
}

std::optional<FileSystem::VirtualPath> FileSystem::ParseVirtualPath(std::string_view path)
{
    return Resource::ParseVirtualPath(path);
}

bool FileSystem::IsCatalogBackedPath(std::string_view path)
{
    return Resource::IsCatalogBackedPath(path);
}

bool FileSystem::IsDocumentPath(std::string_view path)
{
    return Resource::IsDocumentPath(path);
}

std::optional<std::filesystem::path> FileSystem::ResolveReadPath(std::string_view virtualPathString)
{
    const auto virtualPath = Resource::ParseVirtualPath(virtualPathString);
    if (!virtualPath.has_value())
        return std::nullopt;

    return Resource::ResolvePhysicalPath(
        s_RootPath, s_EngineDir, GetSavedDir(), GetCacheDir(), *virtualPath, kProjectContentDirName);
}

std::optional<std::filesystem::path> FileSystem::ResolveWritePath(std::string_view virtualPathString)
{
    const auto virtualPath = Resource::ParseVirtualPath(virtualPathString);
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

    const auto resolved = Resource::ResolvePhysicalPath(
        s_RootPath, s_EngineDir, GetSavedDir(), GetCacheDir(), *virtualPath, kProjectContentDirName);
    if (!resolved.has_value())
        return std::nullopt;

    std::error_code ec;
    std::filesystem::create_directories(resolved->parent_path(), ec);
    return resolved;
}

bool FileSystem::Exists(std::string_view virtualPath)
{
    const auto resolved = ResolveReadPath(virtualPath);
    return resolved.has_value() && std::filesystem::exists(*resolved);
}

std::optional<std::string> FileSystem::ReadText(std::string_view virtualPath)
{
    const auto resolved = ResolveReadPath(virtualPath);
    if (!resolved.has_value())
        return std::nullopt;

    return Resource::ReadTextFile(*resolved);
}

std::optional<std::vector<uint8_t>> FileSystem::ReadBinary(std::string_view virtualPath)
{
    const auto resolved = ResolveReadPath(virtualPath);
    if (!resolved.has_value())
        return std::nullopt;

    return Resource::ReadBinaryFile(*resolved);
}

bool FileSystem::WriteText(std::string_view virtualPath, std::string_view data)
{
    const auto resolved = ResolveWritePath(virtualPath);
    if (!resolved.has_value())
    {
        LOG_ERROR_CAT(LogCategory::FileSystem, "WriteText rejected non-writable virtual path '{}'", virtualPath);
        return false;
    }

    return Resource::WriteTextFile(*resolved, data);
}

bool FileSystem::WriteBinary(std::string_view virtualPath, std::span<const uint8_t> data)
{
    const auto resolved = ResolveWritePath(virtualPath);
    if (!resolved.has_value())
    {
        LOG_ERROR_CAT(LogCategory::FileSystem, "WriteBinary rejected non-writable virtual path '{}'", virtualPath);
        return false;
    }

    return Resource::WriteBinaryFile(*resolved, data);
}

const std::filesystem::path &FileSystem::GetRootPath()
{
    return s_RootPath;
}

std::filesystem::path FileSystem::GetAssetPath(std::string_view relativePath)
{
    return s_RootPath / kProjectContentDirName / relativePath;
}

std::filesystem::path FileSystem::GetCompiledShaderDir()
{
    const auto assetDir = s_RootPath / kProjectContentDirName / "shaders" / "compiled";
    if (std::filesystem::exists(assetDir))
        return assetDir;

#ifdef GLAB_SHADER_BUILD_DIR
    std::filesystem::path buildDir(GLAB_SHADER_BUILD_DIR);
    if (std::filesystem::exists(buildDir))
        return buildDir;
#endif

    return assetDir;
}

void FileSystem::ResolveWritableDirs()
{
    if (s_WritableDirsResolved)
        return;

    const auto writableRoots = Resource::ResolveWritableRoots(s_RootPath, kAppName);
    s_SavedDir = writableRoots.savedDir;
    s_CacheDir = writableRoots.cacheDir;

    std::filesystem::create_directories(s_SavedDir / "Config");
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

    return GetSavedDir() / "Config" / relativePath;
}

std::filesystem::path FileSystem::ResolveConfigPath(std::string_view relativePath)
{
    const std::string logicalConfigPath = relativePath.empty()
                                              ? std::string{}
                                              : "Config/" + std::string(relativePath);

    return Resource::ResolveConfigPath(
        relativePath,
        GetSavedConfigPath(relativePath),
        ResolveReadPath("/Project/" + logicalConfigPath),
        ResolveReadPath("/Engine/" + logicalConfigPath));
}

std::optional<std::string> FileSystem::ReadTextFile(const std::filesystem::path &path)
{
    return Resource::ReadTextFile(path);
}

std::optional<std::vector<uint8_t>> FileSystem::ReadBinaryFile(const std::filesystem::path &path)
{
    return Resource::ReadBinaryFile(path);
}
