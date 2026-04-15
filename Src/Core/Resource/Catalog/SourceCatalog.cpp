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
    static const std::unordered_map<std::string, std::string> kCanonicalSegmentNames{
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
        const auto lookupIt = kCanonicalSegmentNames.find(ToLowerAscii(segmentText));
        canonicalRelative /= (lookupIt != kCanonicalSegmentNames.end()) ? lookupIt->second : segmentText;
    }

    const auto relative = CatalogPathToGenericString(canonicalRelative);

    switch (mountPath.domain)
    {
        case Resource::PathDomain::Project:
            return relative.empty() ? "/Project" : "/Project/" + relative;
        case Resource::PathDomain::Engine:
            return relative.empty() ? "/Engine" : "/Engine/" + relative;
        case Resource::PathDomain::DLC:
            if (!mountPath.mountName.has_value())
                return {};
            return relative.empty() ? "/DLC/" + *mountPath.mountName : "/DLC/" + *mountPath.mountName + "/" + relative;
        case Resource::PathDomain::Mod:
            if (!mountPath.mountName.has_value())
                return {};
            return relative.empty() ? "/Mod/" + *mountPath.mountName : "/Mod/" + *mountPath.mountName + "/" + relative;
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
    static const std::unordered_set<std::string> kAssetDirectories{
        "textures",
        "shaders",
        "materials",
        "scenes",
        "defaults",
    };

    for (const auto& segment : relativePath)
    {
        if (kAssetDirectories.contains(ToLowerAscii(segment.generic_string())))
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
        catalogEntry.logicalPath = logicalPath;
        catalogEntry.sourceRelativePath = CatalogPathToGenericString(relativePath);
        catalogEntry.artifacts.push_back(ArtifactRecord{
            .relativePath = CatalogPathToGenericString(relativePath),
            .format = isDocument ? "document" : DetectFormat(relativePath),
            .platformTag = "any",
            .backendTag = "any",
            .profileTag = "dev",
            .contentHash = 0,
        });

        entryIndexByLogicalPath.emplace(logicalPath, entries.size());
        entries.push_back(std::move(catalogEntry));
    }

    return true;
}
} // namespace Resource
