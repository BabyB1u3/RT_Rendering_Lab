#include "Core/Resource/FileSystem.h"

#include "Core/Diagnostics/Logging/LogCategories.h"
#include "Core/Diagnostics/Logging/LogMacros.h"
#include "Core/Resource/Catalog/ResourceCatalog.h"
#include "Core/Resource/IO/PhysicalIO.h"
#include "Core/Resource/Mount/MountBackend.h"
#include "Core/Resource/Mount/MountResolver.h"
#include "Core/Resource/Mount/RootDiscovery.h"
#include "Core/Resource/Path/PathParser.h"

#include <fstream>

std::filesystem::path FileSystem::s_RootPath;
std::filesystem::path FileSystem::s_EngineDir;
std::filesystem::path FileSystem::s_SavedDir;
std::filesystem::path FileSystem::s_CacheDir;
Resource::CatalogRegistry FileSystem::s_CatalogRegistry;
bool FileSystem::s_Initialized = false;
bool FileSystem::s_WritableDirsResolved = false;

namespace
{
    static constexpr std::string_view kProjectContentDirName = "Project";
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

std::optional<Resource::ResolvedReadableArtifact> FileSystem::ResolveCatalogArtifact(std::string_view virtualPathString)
{
    const auto virtualPath = Resource::ParseVirtualPath(virtualPathString);
    if (!virtualPath.has_value())
        return std::nullopt;

    switch (virtualPath->domain)
    {
    case Resource::PathDomain::Project:
    case Resource::PathDomain::Engine:
        return s_CatalogRegistry.ResolveArtifact(
            s_RootPath, s_EngineDir, GetCacheDir(), *virtualPath, virtualPathString, kProjectContentDirName);
    case Resource::PathDomain::Saved:
    case Resource::PathDomain::Cache:
        return std::nullopt;
    }

    return std::nullopt;
}

std::optional<std::filesystem::path> FileSystem::ResolveWritableReadPath(std::string_view virtualPathString)
{
    const auto virtualPath = Resource::ParseVirtualPath(virtualPathString);
    if (!virtualPath.has_value())
        return std::nullopt;

    switch (virtualPath->domain)
    {
    case Resource::PathDomain::Saved:
    case Resource::PathDomain::Cache:
    {
        const auto writableMount = Resource::ResolveWritableMount(virtualPath->domain, GetSavedDir(), GetCacheDir());
        if (!writableMount.has_value())
            return std::nullopt;

        const auto relativePath = Resource::GetPhysicalRelativePath(*virtualPath);
        return relativePath.empty() ? writableMount->rootPath : writableMount->rootPath / relativePath;
    }
    case Resource::PathDomain::Project:
    case Resource::PathDomain::Engine:
        return std::nullopt;
    }

    return std::nullopt;
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
    if (const auto artifact = ResolveCatalogArtifact(virtualPath))
    {
        switch (artifact->backend)
        {
        case Resource::MountBackendKind::Directory:
            return std::filesystem::exists(artifact->mountRoot / artifact->relativePath);
        case Resource::MountBackendKind::PakArchive:
            return true;
        }
    }

    const auto resolved = ResolveWritableReadPath(virtualPath);
    return resolved.has_value() && std::filesystem::exists(*resolved);
}

std::optional<std::string> FileSystem::ReadText(std::string_view virtualPath)
{
    if (const auto artifact = ResolveCatalogArtifact(virtualPath))
        return Resource::ReadReadableArtifactText(*artifact);

    const auto resolved = ResolveWritableReadPath(virtualPath);
    if (!resolved.has_value())
        return std::nullopt;

    return Resource::ReadTextFile(*resolved);
}

std::optional<std::vector<uint8_t>> FileSystem::ReadBinary(std::string_view virtualPath)
{
    if (const auto artifact = ResolveCatalogArtifact(virtualPath))
        return Resource::ReadReadableArtifactBinary(*artifact);

    const auto resolved = ResolveWritableReadPath(virtualPath);
    if (!resolved.has_value())
        return std::nullopt;

    return Resource::ReadBinaryFile(*resolved);
}

std::unique_ptr<std::istream> FileSystem::OpenReadStream(std::string_view virtualPath)
{
    if (const auto artifact = ResolveCatalogArtifact(virtualPath))
        return Resource::OpenReadableArtifactStream(*artifact);

    const auto resolved = ResolveWritableReadPath(virtualPath);
    if (!resolved.has_value())
        return nullptr;

    auto fileStream = std::make_unique<std::ifstream>(*resolved, std::ios::binary);
    if (!fileStream->is_open())
        return nullptr;

    return std::unique_ptr<std::istream>(std::move(fileStream));
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
