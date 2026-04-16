#include "Core/Resource/Catalog/ResourceCatalog.h"

#include "Core/Diagnostics/Logging/LogCategories.h"
#include "Core/Diagnostics/Logging/LogMacros.h"
#include "Core/Resource/Catalog/SourceCatalog.h"
#include "Core/Resource/Mount/MountBackend.h"
#include "Core/Resource/Path/PathParser.h"

#include <algorithm>
#include <json.hpp>
#include <unordered_set>

namespace
{
using Json = nlohmann::json;

struct ArtifactSelectionContext
{
    std::string_view m_PlatformTag;
    std::string_view m_BackendTag;
    std::string_view m_ProfileTag;
};

std::filesystem::path GetMountRoot(const std::filesystem::path& rootPath,
                                   const std::filesystem::path& engineDir,
                                   const Resource::VirtualPath& virtualPath,
                                   std::string_view projectContentDirName)
{
    switch (virtualPath.m_Domain)
    {
        case Resource::PathDomain::Project:
            return rootPath / projectContentDirName;
        case Resource::PathDomain::Engine:
            return engineDir;
        case Resource::PathDomain::DLC:
        case Resource::PathDomain::Mod:
            return rootPath;
        case Resource::PathDomain::Saved:
        case Resource::PathDomain::Cache:
            return {};
    }

    return {};
}

bool DomainMatchesMount(const Resource::VirtualPath& catalogPath, const Resource::VirtualPath& requestedPath)
{
    return catalogPath.m_Domain == requestedPath.m_Domain && catalogPath.m_MountName == requestedPath.m_MountName;
}

bool IsCatalogLogicalPathForMount(const Resource::VirtualPath& catalogPath, const Resource::VirtualPath& mountPath)
{
    if (!DomainMatchesMount(catalogPath, mountPath))
        return false;

    if (catalogPath.m_RelativePath.empty())
        return false;

    switch (catalogPath.m_Domain)
    {
        case Resource::PathDomain::Project:
        case Resource::PathDomain::Engine:
            return !catalogPath.m_MountName.has_value();
        case Resource::PathDomain::DLC:
        case Resource::PathDomain::Mod:
            return catalogPath.m_MountName.has_value();
        case Resource::PathDomain::Saved:
        case Resource::PathDomain::Cache:
            return false;
    }

    return false;
}

std::optional<Resource::ArtifactRecord> ParseArtifactRecord(const Json& artifactJson)
{
    if (!artifactJson.is_object())
        return std::nullopt;

    const auto relativePathIt = artifactJson.find("relativePath");
    if (relativePathIt == artifactJson.end() || !relativePathIt->is_string())
        return std::nullopt;

    Resource::ArtifactRecord artifact;
    artifact.m_RelativePath = relativePathIt->get<std::string>();

    if (const auto it = artifactJson.find("format"); it != artifactJson.end() && it->is_string())
        artifact.m_Format = it->get<std::string>();
    if (const auto it = artifactJson.find("platformTag"); it != artifactJson.end() && it->is_string())
        artifact.m_PlatformTag = it->get<std::string>();
    if (const auto it = artifactJson.find("backendTag"); it != artifactJson.end() && it->is_string())
        artifact.m_BackendTag = it->get<std::string>();
    if (const auto it = artifactJson.find("profileTag"); it != artifactJson.end() && it->is_string())
        artifact.m_ProfileTag = it->get<std::string>();
    if (const auto it = artifactJson.find("contentHash"); it != artifactJson.end() && it->is_number_unsigned())
        artifact.m_ContentHash = it->get<uint64_t>();

    return artifact;
}

bool ParseCatalogFromJson(const Json& rootJson,
                          const std::string& catalogLabel,
                          const Resource::VirtualPath& mountPath,
                          Resource::CatalogKind& catalogKind,
                          int& catalogVersion,
                          std::unordered_map<std::string, Resource::ResourceCatalogEntry>& entries)
{
    const auto versionIt = rootJson.find("version");
    if (versionIt == rootJson.end() || !versionIt->is_number_integer())
    {
        LOG_ERROR_CAT(LogCategory::k_FileSystem, "Unsupported catalog version in '{}'", catalogLabel);
        return false;
    }

    catalogVersion = versionIt->get<int>();
    const bool isCookedCatalog = catalogVersion == 2;
    if (catalogVersion != 1 && !isCookedCatalog)
    {
        LOG_ERROR_CAT(LogCategory::k_FileSystem, "Unsupported catalog version in '{}'", catalogLabel);
        return false;
    }

    catalogKind = isCookedCatalog ? Resource::CatalogKind::Cooked : Resource::CatalogKind::Source;

    if (isCookedCatalog)
    {
        const auto kindIt = rootJson.find("kind");
        if (kindIt == rootJson.end() || !kindIt->is_string() || kindIt->get<std::string>() != "cooked")
        {
            LOG_ERROR_CAT(LogCategory::k_FileSystem, "Cooked catalog '{}' is missing kind='cooked'", catalogLabel);
            return false;
        }
    }

    const auto entriesIt = rootJson.find("entries");
    if (entriesIt == rootJson.end() || !entriesIt->is_array())
    {
        LOG_ERROR_CAT(LogCategory::k_FileSystem, "Catalog '{}' is missing an entries array", catalogLabel);
        return false;
    }

    for (const auto& entryJson : *entriesIt)
    {
        if (!entryJson.is_object())
        {
            LOG_ERROR_CAT(LogCategory::k_FileSystem, "Catalog '{}' contains a non-object entry", catalogLabel);
            return false;
        }

        const auto logicalPathIt = entryJson.find("logicalPath");
        if (logicalPathIt == entryJson.end() || !logicalPathIt->is_string())
        {
            LOG_ERROR_CAT(
                LogCategory::k_FileSystem, "Catalog '{}' contains an entry without logicalPath", catalogLabel);
            return false;
        }

        const std::string logicalPath = logicalPathIt->get<std::string>();
        const auto parsedLogicalPath = Resource::ParseVirtualPath(logicalPath);
        if (!parsedLogicalPath.has_value())
        {
            LOG_ERROR_CAT(LogCategory::k_FileSystem,
                          "Catalog '{}' contains invalid logical path '{}'",
                          catalogLabel,
                          logicalPath);
            return false;
        }

        if (!DomainMatchesMount(*parsedLogicalPath, mountPath))
            continue;

        if (!IsCatalogLogicalPathForMount(*parsedLogicalPath, mountPath))
        {
            LOG_ERROR_CAT(LogCategory::k_FileSystem,
                          "Catalog '{}' contains invalid mount-scoped logical path '{}'",
                          catalogLabel,
                          logicalPath);
            return false;
        }

        Resource::ResourceCatalogEntry entry;
        entry.m_LogicalPath = logicalPath;
        if (!isCookedCatalog)
        {
            const auto sourceRelativePathIt = entryJson.find("sourceRelativePath");
            if (sourceRelativePathIt == entryJson.end() || !sourceRelativePathIt->is_string())
            {
                LOG_ERROR_CAT(LogCategory::k_FileSystem,
                              "Source catalog '{}' entry '{}' is missing sourceRelativePath",
                              catalogLabel,
                              logicalPath);
                return false;
            }

            entry.m_SourceRelativePath = sourceRelativePathIt->get<std::string>();
        }

        const auto artifactsIt = entryJson.find("artifacts");
        if (artifactsIt == entryJson.end() || !artifactsIt->is_array() || artifactsIt->empty())
        {
            LOG_ERROR_CAT(
                LogCategory::k_FileSystem, "Catalog '{}' entry '{}' has no artifacts", catalogLabel, logicalPath);
            return false;
        }

        for (const auto& artifactJson : *artifactsIt)
        {
            const auto artifact = ParseArtifactRecord(artifactJson);
            if (!artifact.has_value())
            {
                LOG_ERROR_CAT(LogCategory::k_FileSystem,
                              "Catalog '{}' entry '{}' contains an invalid artifact",
                              catalogLabel,
                              logicalPath);
                return false;
            }

            entry.m_Artifacts.push_back(*artifact);
        }

        const auto [it, inserted] = entries.emplace(entry.m_LogicalPath, std::move(entry));
        if (!inserted)
        {
            LOG_ERROR_CAT(LogCategory::k_FileSystem,
                          "Catalog '{}' contains duplicate logical path '{}'",
                          catalogLabel,
                          it->first);
            return false;
        }
    }

    return true;
}

bool LoadCatalogFromMount(const Resource::ReadableMount& mount,
                          Resource::CatalogKind& catalogKind,
                          int& catalogVersion,
                          std::unordered_map<std::string, Resource::ResourceCatalogEntry>& entries)
{
    std::vector<uint8_t> bytes;
    std::string errorMessage;
    if (!Resource::ReadMountCatalogBytes(mount, bytes, &errorMessage))
    {
        if (!errorMessage.empty())
            LOG_ERROR_CAT(LogCategory::k_FileSystem,
                          "Failed to read mount catalog '{}': {}",
                          mount.m_MountRoot.string(),
                          errorMessage);
        return false;
    }

    Json rootJson;
    try
    {
        rootJson = Json::parse(bytes.begin(), bytes.end());
    }
    catch (const std::exception& e)
    {
        LOG_ERROR_CAT(
            LogCategory::k_FileSystem, "Failed to parse catalog '{}': {}", mount.m_MountRoot.string(), e.what());
        return false;
    }

    const std::string catalogLabel = mount.m_Backend == Resource::MountBackendKind::PakArchive
                                         ? mount.m_MountRoot.string() + "::/.rtr/catalog.json"
                                         : (mount.m_MountRoot / ".rtr" / "catalog.json").string();
    return ParseCatalogFromJson(rootJson, catalogLabel, mount.m_MountPath, catalogKind, catalogVersion, entries);
}

bool ShouldBuildSourceCatalogInMemory(const Resource::ReadableMount& mount, std::string_view currentProfileTag)
{
    if (currentProfileTag != "dev")
        return false;

    return mount.m_Priority == Resource::MountPriority::Source &&
           mount.m_Backend == Resource::MountBackendKind::Directory &&
           (mount.m_MountPath.m_Domain == Resource::PathDomain::Project ||
            mount.m_MountPath.m_Domain == Resource::PathDomain::Engine);
}

std::string_view GetCurrentPlatformTag()
{
#if defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#elif defined(__linux__)
    return "linux";
#else
    return "unknown";
#endif
}

std::string_view GetCurrentBackendTag()
{
#if defined(GLAB_BACKEND_OPENGL)
    return "opengl";
#elif defined(GLAB_BACKEND_METAL)
    return "metal";
#elif defined(GLAB_BACKEND_VULKAN)
    return "vulkan";
#else
    return "unknown";
#endif
}

std::string_view GetCurrentProfileTag()
{
#ifdef RTRLAB_CONFIG_RELEASE
    return "shipping";
#else
#if defined(GLAB_ROOT_DIR)
    return "dev";
#else
    return "shipping";
#endif
#endif
}

std::string_view GetCurrentArtifactProfileTag()
{
    const auto profile = GetCurrentProfileTag();
    if (profile == "shipping")
        return "cooked";
    return profile;
}

ArtifactSelectionContext GetCurrentArtifactSelectionContext()
{
    return ArtifactSelectionContext{
        GetCurrentPlatformTag(),
        GetCurrentBackendTag(),
        GetCurrentArtifactProfileTag(),
    };
}

int ScoreTag(std::string_view candidateTag, std::string_view runtimeTag)
{
    if (candidateTag.empty() || candidateTag == "any")
        return 1;
    if (candidateTag == runtimeTag)
        return 2;
    return -1;
}

std::optional<Resource::ArtifactRecord> ChooseArtifact(const Resource::ResourceCatalogEntry& entry)
{
    const auto context = GetCurrentArtifactSelectionContext();

    const Resource::ArtifactRecord* bestArtifact = nullptr;
    int bestScore = -1;

    for (const auto& artifact : entry.m_Artifacts)
    {
        const int profileScore = ScoreTag(artifact.m_ProfileTag, context.m_ProfileTag);
        const int backendScore = ScoreTag(artifact.m_BackendTag, context.m_BackendTag);
        const int platformScore = ScoreTag(artifact.m_PlatformTag, context.m_PlatformTag);
        if (profileScore < 0 || backendScore < 0 || platformScore < 0)
            continue;

        const int totalScore = profileScore * 100 + backendScore * 10 + platformScore;
        if (totalScore > bestScore)
        {
            bestArtifact = &artifact;
            bestScore = totalScore;
        }
    }

    if (bestArtifact != nullptr)
        return *bestArtifact;

    return std::nullopt;
}

void MergeMountEntriesIntoGlobalTable(
    const Resource::ReadableMount& mount,
    const Resource::CatalogRegistry::MountCatalogCache& cache,
    std::unordered_map<std::string, Resource::CatalogRegistry::GlobalCatalogEntry>& globalEntries,
    std::unordered_set<std::string>& conflictedLogicalPaths)
{
    for (const auto& [logicalPath, entry] : cache.m_Entries)
    {
        if (conflictedLogicalPaths.contains(logicalPath))
            continue;

        const auto existingIt = globalEntries.find(logicalPath);
        if (existingIt != globalEntries.end())
        {
            if (static_cast<int>(mount.m_Priority) > static_cast<int>(existingIt->second.m_Priority))
            {
                existingIt->second = Resource::CatalogRegistry::GlobalCatalogEntry{
                    entry,
                    cache.m_Kind,
                    cache.m_Version,
                    mount.m_Priority,
                    mount.m_Backend,
                    mount.m_MountRoot,
                    mount.m_SourceKey,
                };
                continue;
            }

            if (static_cast<int>(mount.m_Priority) < static_cast<int>(existingIt->second.m_Priority))
                continue;

            LOG_ERROR_CAT(LogCategory::k_FileSystem,
                          "Logical path '{}' is provided by multiple equal-precedence mounts ('{}' and '{}')",
                          logicalPath,
                          existingIt->second.m_SourceMountKey,
                          mount.m_SourceKey);
            conflictedLogicalPaths.insert(logicalPath);
            globalEntries.erase(existingIt);
            continue;
        }

        globalEntries.emplace(logicalPath,
                              Resource::CatalogRegistry::GlobalCatalogEntry{
                                  entry,
                                  cache.m_Kind,
                                  cache.m_Version,
                                  mount.m_Priority,
                                  mount.m_Backend,
                                  mount.m_MountRoot,
                                  mount.m_SourceKey,
                              });
    }
}
} // namespace

namespace Resource
{
void CatalogRegistry::Reset()
{
    m_GlobalTableBuilt = false;
    m_MountCatalogs.clear();
    m_GlobalEntries.clear();
    m_ConflictedLogicalPaths.clear();
}

std::optional<ResolvedReadableArtifact> CatalogRegistry::ResolveArtifact(const std::filesystem::path& rootPath,
                                                                         const std::filesystem::path& engineDir,
                                                                         const std::filesystem::path& cacheDir,
                                                                         const VirtualPath& virtualPath,
                                                                         std::string_view logicalPath,
                                                                         std::string_view projectContentDirName)
{
    const auto mountRoot = GetMountRoot(rootPath, engineDir, virtualPath, projectContentDirName);
    if (mountRoot.empty())
        return std::nullopt;

    if (!m_GlobalTableBuilt)
    {
        m_GlobalTableBuilt = true;
        m_GlobalEntries.clear();
        m_ConflictedLogicalPaths.clear();
        const auto currentProfileTag = GetCurrentProfileTag();

        for (const auto& mount : Resource::DiscoverReadableMountBackends(
                 rootPath, engineDir, cacheDir, projectContentDirName, currentProfileTag))
        {
            auto& cache = m_MountCatalogs[mount.m_CacheKey];
            if (!cache.m_HasAttemptedLoad)
            {
                cache.m_HasAttemptedLoad = true;
                if (ShouldBuildSourceCatalogInMemory(mount, currentProfileTag))
                {
                    cache.m_Kind = CatalogKind::Source;
                    cache.m_Version = 1;
                    std::vector<ResourceCatalogEntry> loadedEntries;
                    if (!Resource::BuildSourceCatalogEntries(mount.m_MountRoot, mount.m_MountPath, loadedEntries))
                    {
                        cache.m_Entries.clear();
                        LOG_ERROR_CAT(LogCategory::k_FileSystem,
                                      "Failed to build in-memory source catalog '{}'",
                                      mount.m_MountRoot.string());
                    }
                    else
                    {
                        cache.m_Entries.clear();
                        cache.m_Entries.reserve(loadedEntries.size());
                        for (auto& entry : loadedEntries)
                            cache.m_Entries.emplace(entry.m_LogicalPath, std::move(entry));
                        LOG_INFO_CAT(LogCategory::k_FileSystem,
                                     "Built in-memory source catalog '{}'",
                                     mount.m_MountRoot.string());
                    }
                }
                else if (Resource::MountHasCatalog(mount))
                {
                    cache.m_Kind = CatalogKind::Source;
                    cache.m_Version = 0;
                    std::unordered_map<std::string, ResourceCatalogEntry> loadedEntries;
                    const bool loaded = LoadCatalogFromMount(mount, cache.m_Kind, cache.m_Version, loadedEntries);
                    if (!loaded)
                    {
                        cache.m_Entries.clear();
                        LOG_ERROR_CAT(
                            LogCategory::k_FileSystem, "Failed to load catalog '{}'", mount.m_MountRoot.string());
                    }
                    else
                    {
                        cache.m_Entries = std::move(loadedEntries);
                        LOG_INFO_CAT(LogCategory::k_FileSystem, "Loaded catalog '{}'", mount.m_MountRoot.string());
                    }
                }
            }

            MergeMountEntriesIntoGlobalTable(mount, cache, m_GlobalEntries, m_ConflictedLogicalPaths);
        }
    }

    const auto logicalPathString = std::string(logicalPath);
    if (m_ConflictedLogicalPaths.contains(logicalPathString))
        return std::nullopt;

    const auto entryIt = m_GlobalEntries.find(logicalPathString);
    if (entryIt == m_GlobalEntries.end())
        return std::nullopt;

    const auto artifact = ChooseArtifact(entryIt->second.m_Entry);
    if (!artifact.has_value())
        return std::nullopt;

    const ReadableMount mount{
        .m_CacheKey = {},
        .m_SourceKey = entryIt->second.m_SourceMountKey,
        .m_MountPath = virtualPath,
        .m_Priority = entryIt->second.m_Priority,
        .m_Backend = entryIt->second.m_Backend,
        .m_MountRoot = entryIt->second.m_MountRoot,
    };

    std::string errorMessage;
    const auto resolvedArtifact = ResolveReadableMountArtifact(mount, *artifact, &errorMessage);
    if (!resolvedArtifact.has_value() && !errorMessage.empty())
    {
        LOG_ERROR_CAT(LogCategory::k_FileSystem,
                      "Failed to resolve artifact '{}' from mount '{}': {}",
                      artifact->m_RelativePath,
                      entryIt->second.m_MountRoot.string(),
                      errorMessage);
    }

    return resolvedArtifact;
}
} // namespace Resource
