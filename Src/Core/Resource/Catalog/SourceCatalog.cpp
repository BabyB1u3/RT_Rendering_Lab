#include "Core/Resource/Catalog/SourceCatalog.h"

#include "Core/Resource/Path/PathParser.h"

#include <algorithm>
#include <cctype>
#include <unordered_set>
#include <unordered_map>

namespace
{
std::string CatalogPathToGenericString(const std::filesystem::path& path)
{
    return path.generic_string();
}

std::string ToLowerAscii(std::string value)
{
    std::transform(
        value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool IsExcludedRelativePath(const std::filesystem::path& relativePath)
{
    for (const auto& segment : relativePath)
    {
        const auto segmentText = segment.generic_string();
        if (segmentText == ".rtr")
            return true;
    }

    return false;
}

std::string MakeLogicalPath(const Resource::VirtualPath& mountPath, const std::filesystem::path& logicalRelativePath)
{
    static const std::unordered_map<std::string, std::string> k_CanonicalSegmentNames{
        {"textures", "Textures"},
        {"shaders", "Shaders"},
        {"materials", "Materials"},
        {"scenes", "Scenes"},
        {"defaults", "Defaults"},
        {"editor", "Editor"},
        {"config", "Config"},
    };

    std::filesystem::path canonicalRelative;
    for (const auto& segment : logicalRelativePath)
    {
        const auto segmentText = segment.generic_string();
        const auto lookupIt = k_CanonicalSegmentNames.find(ToLowerAscii(segmentText));
        canonicalRelative /= (lookupIt != k_CanonicalSegmentNames.end()) ? lookupIt->second : segmentText;
    }

    const auto relative = CatalogPathToGenericString(canonicalRelative);

    switch (mountPath.m_Domain)
    {
        case Resource::PathDomain::Project:
            return relative.empty() ? "/Project" : "/Project/" + relative;
        case Resource::PathDomain::Engine:
            return relative.empty() ? "/Engine" : "/Engine/" + relative;
        case Resource::PathDomain::DLC:
            if (!mountPath.m_MountName.has_value())
                return {};
            return relative.empty() ? "/DLC/" + *mountPath.m_MountName
                                    : "/DLC/" + *mountPath.m_MountName + "/" + relative;
        case Resource::PathDomain::Mod:
            if (!mountPath.m_MountName.has_value())
                return {};
            return relative.empty() ? "/Mod/" + *mountPath.m_MountName
                                    : "/Mod/" + *mountPath.m_MountName + "/" + relative;
        case Resource::PathDomain::Saved:
            return relative.empty() ? "/Saved" : "/Saved/" + relative;
        case Resource::PathDomain::Cache:
            return relative.empty() ? "/Cache" : "/Cache/" + relative;
    }

    return {};
}

std::string DetectFormat(const std::filesystem::path& path)
{
    auto extension = path.extension().string();
    if (!extension.empty() && extension.front() == '.')
        extension.erase(extension.begin());
    return ToLowerAscii(extension);
}

bool RelativePathStartsWith(const std::filesystem::path& relativePath, std::string_view firstSegment)
{
    const auto it = relativePath.begin();
    return it != relativePath.end() && ToLowerAscii(it->generic_string()) == ToLowerAscii(std::string(firstSegment));
}

bool RelativePathContainsAssetDirectory(const std::filesystem::path& relativePath)
{
    static const std::unordered_set<std::string> k_AssetDirectories{
        "textures",
        "shaders",
        "materials",
        "scenes",
        "defaults",
    };

    for (const auto& segment : relativePath)
    {
        if (k_AssetDirectories.contains(ToLowerAscii(segment.generic_string())))
            return true;
    }

    return false;
}

bool IsDocumentPath(const std::filesystem::path& relativePath)
{
    if (RelativePathStartsWith(relativePath, "Config"))
        return true;

    if (RelativePathContainsAssetDirectory(relativePath))
        return false;

    const auto extension = DetectFormat(relativePath);
    return extension == "ini" || extension == "txt" || extension == "xml" || extension == "toml";
}

} // namespace

namespace Resource
{
bool BuildSourceCatalogEntries(const std::filesystem::path& mountRoot,
                               const VirtualPath& mountPath,
                               std::vector<ResourceCatalogEntry>& entries,
                               std::string* errorMessage)
{
    entries.clear();

    std::unordered_map<std::string, size_t> entryIndexByLogicalPath;
    std::error_code ec;
    std::vector<std::filesystem::path> files;

    if (!std::filesystem::exists(mountRoot))
    {
        if (errorMessage != nullptr)
            *errorMessage = "mount root does not exist: " + mountRoot.string();
        return false;
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(mountRoot, ec))
    {
        if (ec)
        {
            if (errorMessage != nullptr)
                *errorMessage = "failed to scan mount root: " + ec.message();
            return false;
        }

        if (!entry.is_regular_file())
            continue;

        const auto relativePath = std::filesystem::relative(entry.path(), mountRoot, ec);
        if (ec)
        {
            if (errorMessage != nullptr)
                *errorMessage = "failed to compute relative path for " + entry.path().string();
            return false;
        }

        if (IsExcludedRelativePath(relativePath))
            continue;

        files.push_back(relativePath);
    }

    std::sort(files.begin(),
              files.end(),
              [](const auto& lhs, const auto& rhs) { return lhs.generic_string() < rhs.generic_string(); });

    for (const auto& relativePath : files)
    {
        const bool isDocument = IsDocumentPath(relativePath);
        const auto logicalRelativePath = isDocument ? relativePath : (relativePath.parent_path() / relativePath.stem());
        const auto logicalPath = MakeLogicalPath(mountPath, logicalRelativePath);

        if (logicalPath.empty())
        {
            if (errorMessage != nullptr)
                *errorMessage = "failed to derive logical path for " + relativePath.string();
            return false;
        }

        if (entryIndexByLogicalPath.contains(logicalPath))
        {
            if (errorMessage != nullptr)
            {
                *errorMessage = "duplicate logical path '" + logicalPath + "' generated from source file '" +
                                relativePath.generic_string() + "'";
            }
            return false;
        }

        ResourceCatalogEntry catalogEntry;
        catalogEntry.m_LogicalPath = logicalPath;
        catalogEntry.m_SourceRelativePath = CatalogPathToGenericString(relativePath);
        catalogEntry.m_Artifacts.push_back(ArtifactRecord{
            .m_RelativePath = CatalogPathToGenericString(relativePath),
            .m_Format = isDocument ? "document" : DetectFormat(relativePath),
            .m_PlatformTag = "any",
            .m_BackendTag = "any",
            .m_ProfileTag = "dev",
            .m_ContentHash = 0,
        });

        entryIndexByLogicalPath.emplace(logicalPath, entries.size());
        entries.push_back(std::move(catalogEntry));
    }

    return true;
}
} // namespace Resource
