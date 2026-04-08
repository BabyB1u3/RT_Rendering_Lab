#include "Core/Resource/ResourceCatalog.h"

#include "Core/Diagnostics/LogCategories.h"
#include "Core/Diagnostics/LogMacros.h"
#include "Core/Resource/PathParser.h"

#include <algorithm>
#include <fstream>
#include <json.hpp>

namespace
{
    using Json = nlohmann::json;

    struct MountDescriptor
    {
        std::string cacheKey;
        Resource::VirtualPath mountPath;
        std::filesystem::path mountRoot;
    };

    std::string MakeMountCacheKey(const Resource::VirtualPath &virtualPath)
    {
        switch (virtualPath.domain)
        {
        case Resource::PathDomain::Project:
            return "Project";
        case Resource::PathDomain::Engine:
            return "Engine";
        case Resource::PathDomain::Plugin:
            return "Plugin:" + virtualPath.mountName.value_or(std::string{});
        case Resource::PathDomain::Saved:
            return "Saved";
        case Resource::PathDomain::Cache:
            return "Cache";
        }

        return {};
    }

    std::filesystem::path GetMountRoot(const std::filesystem::path &rootPath,
                                       const std::filesystem::path &engineDir,
                                       const Resource::VirtualPath &virtualPath,
                                       std::string_view projectContentDirName)
    {
        switch (virtualPath.domain)
        {
        case Resource::PathDomain::Project:
            return rootPath / projectContentDirName;
        case Resource::PathDomain::Engine:
            return engineDir;
        case Resource::PathDomain::Plugin:
            return rootPath / "Plugins" / virtualPath.mountName.value_or(std::string{}) / "Content";
        case Resource::PathDomain::Saved:
        case Resource::PathDomain::Cache:
            return {};
        }

        return {};
    }

    bool DomainMatchesMount(const Resource::VirtualPath &catalogPath, const Resource::VirtualPath &requestedPath)
    {
        if (catalogPath.domain != requestedPath.domain)
            return false;

        if (catalogPath.domain == Resource::PathDomain::Plugin)
            return catalogPath.mountName == requestedPath.mountName;

        return true;
    }

    std::optional<Resource::ArtifactRecord> ParseArtifactRecord(const Json &artifactJson)
    {
        if (!artifactJson.is_object())
            return std::nullopt;

        const auto relativePathIt = artifactJson.find("relativePath");
        if (relativePathIt == artifactJson.end() || !relativePathIt->is_string())
            return std::nullopt;

        Resource::ArtifactRecord artifact;
        artifact.relativePath = relativePathIt->get<std::string>();

        if (const auto it = artifactJson.find("format"); it != artifactJson.end() && it->is_string())
            artifact.format = it->get<std::string>();
        if (const auto it = artifactJson.find("platformTag"); it != artifactJson.end() && it->is_string())
            artifact.platformTag = it->get<std::string>();
        if (const auto it = artifactJson.find("backendTag"); it != artifactJson.end() && it->is_string())
            artifact.backendTag = it->get<std::string>();
        if (const auto it = artifactJson.find("profileTag"); it != artifactJson.end() && it->is_string())
            artifact.profileTag = it->get<std::string>();
        if (const auto it = artifactJson.find("contentHash"); it != artifactJson.end() && it->is_number_unsigned())
            artifact.contentHash = it->get<uint64_t>();

        return artifact;
    }

    bool LoadCatalogFromJson(const std::filesystem::path &catalogPath,
                             const Resource::VirtualPath &mountPath,
                             std::unordered_map<std::string, Resource::ResourceCatalogEntry> &entries)
    {
        std::ifstream in(catalogPath, std::ios::in | std::ios::binary);
        if (!in.is_open())
            return false;

        Json rootJson;
        try
        {
            in >> rootJson;
        }
        catch (const std::exception &e)
        {
            LOG_ERROR_CAT(LogCategory::FileSystem, "Failed to parse catalog '{}': {}", catalogPath.string(), e.what());
            return false;
        }

        const auto versionIt = rootJson.find("version");
        if (versionIt == rootJson.end() || !versionIt->is_number_integer() || versionIt->get<int>() != 1)
        {
            LOG_ERROR_CAT(LogCategory::FileSystem, "Unsupported catalog version in '{}'", catalogPath.string());
            return false;
        }

        const auto entriesIt = rootJson.find("entries");
        if (entriesIt == rootJson.end() || !entriesIt->is_array())
        {
            LOG_ERROR_CAT(LogCategory::FileSystem, "Catalog '{}' is missing an entries array", catalogPath.string());
            return false;
        }

        for (const auto &entryJson : *entriesIt)
        {
            if (!entryJson.is_object())
            {
                LOG_ERROR_CAT(LogCategory::FileSystem, "Catalog '{}' contains a non-object entry", catalogPath.string());
                return false;
            }

            const auto logicalPathIt = entryJson.find("logicalPath");
            if (logicalPathIt == entryJson.end() || !logicalPathIt->is_string())
            {
                LOG_ERROR_CAT(LogCategory::FileSystem, "Catalog '{}' contains an entry without logicalPath", catalogPath.string());
                return false;
            }

            const std::string logicalPath = logicalPathIt->get<std::string>();
            const auto parsedLogicalPath = Resource::ParseVirtualPath(logicalPath);
            if (!parsedLogicalPath.has_value() || !Resource::IsCatalogBackedPath(logicalPath) ||
                !DomainMatchesMount(*parsedLogicalPath, mountPath))
            {
                LOG_ERROR_CAT(LogCategory::FileSystem,
                              "Catalog '{}' contains invalid mount-scoped logical path '{}'",
                              catalogPath.string(),
                              logicalPath);
                return false;
            }

            Resource::ResourceCatalogEntry entry;
            entry.logicalPath = logicalPath;
            if (const auto it = entryJson.find("sourceRelativePath"); it != entryJson.end() && it->is_string())
                entry.sourceRelativePath = it->get<std::string>();

            const auto artifactsIt = entryJson.find("artifacts");
            if (artifactsIt == entryJson.end() || !artifactsIt->is_array() || artifactsIt->empty())
            {
                LOG_ERROR_CAT(LogCategory::FileSystem,
                              "Catalog '{}' entry '{}' has no artifacts",
                              catalogPath.string(),
                              logicalPath);
                return false;
            }

            for (const auto &artifactJson : *artifactsIt)
            {
                const auto artifact = ParseArtifactRecord(artifactJson);
                if (!artifact.has_value())
                {
                    LOG_ERROR_CAT(LogCategory::FileSystem,
                                  "Catalog '{}' entry '{}' contains an invalid artifact",
                                  catalogPath.string(),
                                  logicalPath);
                    return false;
                }

                entry.artifacts.push_back(*artifact);
            }

            const auto [it, inserted] = entries.emplace(entry.logicalPath, std::move(entry));
            if (!inserted)
            {
                LOG_ERROR_CAT(LogCategory::FileSystem,
                              "Catalog '{}' contains duplicate logical path '{}'",
                              catalogPath.string(),
                              it->first);
                return false;
            }
        }

        return true;
    }

    std::optional<Resource::ArtifactRecord> ChooseArtifact(const Resource::ResourceCatalogEntry &entry)
    {
        for (const auto &artifact : entry.artifacts)
        {
            if (artifact.profileTag == "dev")
                return artifact;
        }

        for (const auto &artifact : entry.artifacts)
        {
            if (artifact.profileTag == "any" || artifact.profileTag.empty())
                return artifact;
        }

        if (!entry.artifacts.empty())
            return entry.artifacts.front();

        return std::nullopt;
    }

    std::vector<MountDescriptor> DiscoverReadableMounts(const std::filesystem::path &rootPath,
                                                        const std::filesystem::path &engineDir,
                                                        std::string_view projectContentDirName)
    {
        std::vector<MountDescriptor> mounts;
        mounts.push_back(MountDescriptor{
            "Project",
            Resource::VirtualPath{Resource::PathDomain::Project, std::nullopt, {}},
            rootPath / projectContentDirName});

        if (std::filesystem::exists(engineDir))
        {
            mounts.push_back(MountDescriptor{
                "Engine",
                Resource::VirtualPath{Resource::PathDomain::Engine, std::nullopt, {}},
                engineDir});
        }

        const auto pluginsRoot = rootPath / "Plugins";
        if (std::filesystem::exists(pluginsRoot))
        {
            std::vector<std::filesystem::directory_entry> pluginDirs;
            for (const auto &entry : std::filesystem::directory_iterator(pluginsRoot))
            {
                if (entry.is_directory())
                    pluginDirs.push_back(entry);
            }

            std::sort(pluginDirs.begin(), pluginDirs.end(), [](const auto &lhs, const auto &rhs) {
                return lhs.path().filename().string() < rhs.path().filename().string();
            });

            for (const auto &pluginEntry : pluginDirs)
            {
                const auto pluginName = pluginEntry.path().filename().string();
                const auto contentRoot = pluginEntry.path() / "Content";
                if (!std::filesystem::exists(contentRoot))
                    continue;

                mounts.push_back(MountDescriptor{
                    "Plugin:" + pluginName,
                    Resource::VirtualPath{Resource::PathDomain::Plugin, pluginName, {}},
                    contentRoot});
            }
        }

        return mounts;
    }

    void MergeMountEntriesIntoGlobalTable(
        const MountDescriptor &mount,
        const Resource::CatalogRegistry::MountCatalogCache &cache,
        std::unordered_map<std::string, Resource::CatalogRegistry::GlobalCatalogEntry> &globalEntries,
        std::unordered_set<std::string> &conflictedLogicalPaths)
    {
        for (const auto &[logicalPath, entry] : cache.entries)
        {
            if (conflictedLogicalPaths.contains(logicalPath))
                continue;

            const auto existingIt = globalEntries.find(logicalPath);
            if (existingIt != globalEntries.end())
            {
                LOG_ERROR_CAT(LogCategory::FileSystem,
                              "Logical path '{}' is provided by multiple readable mounts ('{}' and '{}')",
                              logicalPath,
                              existingIt->second.sourceMountKey,
                              mount.cacheKey);
                conflictedLogicalPaths.insert(logicalPath);
                globalEntries.erase(existingIt);
                continue;
            }

            globalEntries.emplace(logicalPath,
                                  Resource::CatalogRegistry::GlobalCatalogEntry{
                                      entry,
                                      mount.mountRoot,
                                      mount.cacheKey,
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

    std::optional<std::filesystem::path> CatalogRegistry::ResolvePath(const std::filesystem::path &rootPath,
                                                                      const std::filesystem::path &engineDir,
                                                                      const VirtualPath &virtualPath,
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

            for (const auto &mount : DiscoverReadableMounts(rootPath, engineDir, projectContentDirName))
            {
                auto &cache = m_MountCatalogs[mount.cacheKey];
                if (!cache.attemptedLoad)
                {
                    cache.attemptedLoad = true;
                    const auto catalogPath = mount.mountRoot / ".rtr" / "catalog.json";
                    if (std::filesystem::exists(catalogPath))
                    {
                        std::unordered_map<std::string, ResourceCatalogEntry> loadedEntries;
                        if (!LoadCatalogFromJson(catalogPath, mount.mountPath, loadedEntries))
                        {
                            cache.entries.clear();
                            LOG_ERROR_CAT(LogCategory::FileSystem, "Failed to load catalog '{}'", catalogPath.string());
                        }
                        else
                        {
                            cache.entries = std::move(loadedEntries);
                            LOG_INFO_CAT(LogCategory::FileSystem, "Loaded catalog '{}'", catalogPath.string());
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

        const auto artifact = ChooseArtifact(entryIt->second.entry);
        if (!artifact.has_value())
            return std::nullopt;

        return entryIt->second.mountRoot / artifact->relativePath;
    }
} // namespace Resource
