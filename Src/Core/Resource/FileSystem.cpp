#include "Core/Resource/FileSystem.h"

#include "Core/Diagnostics/Logging/LogCategories.h"
#include "Core/Diagnostics/Logging/LogMacros.h"
#include "Core/Resource/Catalog/ResourceCatalog.h"
#include "Core/Resource/IO/PhysicalIO.h"
#include "Core/Resource/Mount/MountBackend.h"
#include "Core/Resource/Mount/MountResolver.h"
#include "Core/Resource/Mount/RootDiscovery.h"
#include "Core/Resource/Path/PathParser.h"

std::filesystem::path FileSystem::s_RootPath;
std::filesystem::path FileSystem::s_EngineDir;
std::filesystem::path FileSystem::s_SavedDir;
std::filesystem::path FileSystem::s_CacheDir;
Resource::CatalogRegistry FileSystem::s_CatalogRegistry;
bool FileSystem::s_Initialized = false;
bool FileSystem::s_WritableDirsResolved = false;

namespace
{
    static constexpr std::string_view kProjectContentDirName = "Content";
    static constexpr const char *kAppName = "RTRLab";
} // namespace

void FileSystem::Init()
{
    s_RootPath = Resource::DiscoverRootPath();
    s_EngineDir = s_RootPath / "Engine";
    s_CatalogRegistry.Reset();
    s_Initialized = true;
}

void FileSystem::RefreshCatalogs()
{
    s_CatalogRegistry.Reset();
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

    if (Resource::IsCatalogBackedPath(virtualPathString))
    {
        if (const auto resolved = s_CatalogRegistry.ResolvePath(
                s_RootPath, s_EngineDir, GetCacheDir(), *virtualPath, virtualPathString, kProjectContentDirName))
        {
            return resolved;
        }

        return std::nullopt;
    }

    return Resource::ResolvePhysicalPath(
        s_RootPath, s_EngineDir, GetSavedDir(), GetCacheDir(), *virtualPath, kProjectContentDirName);
}

std::optional<std::filesystem::path> FileSystem::ResolveWritePath(std::string_view virtualPathString)
{
    const auto virtualPath = Resource::ParseVirtualPath(virtualPathString);
    if (!virtualPath.has_value())
        return std::nullopt;

    const auto writableMount = Resource::ResolveWritableMount(virtualPath->domain, GetSavedDir(), GetCacheDir());
    if (!writableMount.has_value())
        return std::nullopt;

    const auto relativePath = Resource::GetPhysicalRelativePath(*virtualPath);
    const auto resolved = relativePath.empty() ? writableMount->rootPath : writableMount->rootPath / relativePath;

    std::error_code ec;
    std::filesystem::create_directories(resolved.parent_path(), ec);
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
