#include "Core/Resource/ResourceCatalog.h"

#include "Core/Diagnostics/LogCategories.h"
#include "Core/Diagnostics/LogMacros.h"
#include "Core/Resource/PathParser.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <json.hpp>
#include <unordered_set>

namespace
{
    using Json = nlohmann::json;

    struct MountDescriptor
    {
        std::string cacheKey;
        std::string sourceKey;
        Resource::VirtualPath mountPath;
        std::filesystem::path mountRoot;
    };

    struct ArtifactSelectionContext
    {
        std::string_view platformTag;
        std::string_view backendTag;
        std::string_view profileTag;
    };

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
        if (versionIt == rootJson.end() || !versionIt->is_number_integer())
        {
            LOG_ERROR_CAT(LogCategory::FileSystem, "Unsupported catalog version in '{}'", catalogPath.string());
            return false;
        }

        const int version = versionIt->get<int>();
        const bool isCookedCatalog = version == 2;
        if (version != 1 && !isCookedCatalog)
        {
            LOG_ERROR_CAT(LogCategory::FileSystem, "Unsupported catalog version in '{}'", catalogPath.string());
            return false;
        }

        if (isCookedCatalog)
        {
            const auto kindIt = rootJson.find("kind");
            if (kindIt == rootJson.end() || !kindIt->is_string() || kindIt->get<std::string>() != "cooked")
            {
                LOG_ERROR_CAT(LogCategory::FileSystem,
                              "Cooked catalog '{}' is missing kind='cooked'",
                              catalogPath.string());
                return false;
            }
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
            if (!isCookedCatalog)
            {
                if (const auto it = entryJson.find("sourceRelativePath"); it != entryJson.end() && it->is_string())
                    entry.sourceRelativePath = it->get<std::string>();
            }
            else if (const auto it = entryJson.find("sourceRelativePath"); it != entryJson.end() && it->is_string())
            {
                entry.sourceRelativePath = it->get<std::string>();
            }

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
#if defined(GLAB_BACKEND_METAL)
        return "metal";
#else
        return "opengl";
#endif
    }

    std::string_view GetCurrentProfileTag()
    {
        if (const char *overrideValue = std::getenv("RTRLAB_RESOURCE_PROFILE"))
        {
            const std::string_view value = overrideValue;
            if (!value.empty())
                return value;
        }

#if defined(GLAB_ROOT_DIR)
        return "dev";
#elif defined(NDEBUG)
        return "shipping";
#else
        return "dev";
#endif
    }

    ArtifactSelectionContext GetCurrentArtifactSelectionContext()
    {
        return ArtifactSelectionContext{
            GetCurrentPlatformTag(),
            GetCurrentBackendTag(),
            GetCurrentProfileTag(),
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

    std::optional<Resource::ArtifactRecord> ChooseArtifact(const Resource::ResourceCatalogEntry &entry)
    {
        const auto context = GetCurrentArtifactSelectionContext();

        const Resource::ArtifactRecord *bestArtifact = nullptr;
        int bestScore = -1;

        for (const auto &artifact : entry.artifacts)
        {
            const int profileScore = ScoreTag(artifact.profileTag, context.profileTag);
            const int backendScore = ScoreTag(artifact.backendTag, context.backendTag);
            const int platformScore = ScoreTag(artifact.platformTag, context.platformTag);
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

    std::vector<MountDescriptor> DiscoverReadableMounts(const std::filesystem::path &rootPath,
                                                        const std::filesystem::path &engineDir,
                                                        const std::filesystem::path &cacheDir,
                                                        std::string_view projectContentDirName)
    {
        std::vector<MountDescriptor> mounts;
        const bool preferCookedArtifacts = GetCurrentProfileTag() == "cooked";

        const auto addReadableMount = [&](const std::string &sourceKey,
                                          const Resource::VirtualPath &mountPath,
                                          const std::filesystem::path &sourceRoot,
                                          const std::filesystem::path &cookedRoot) {
            const bool hasCookedCatalog = std::filesystem::exists(cookedRoot / ".rtr" / "catalog.json");
            const bool hasSourceRoot = std::filesystem::exists(sourceRoot);

            if (preferCookedArtifacts && hasCookedCatalog)
            {
                mounts.push_back(MountDescriptor{
                    "Cooked:" + sourceKey,
                    sourceKey,
                    mountPath,
                    cookedRoot,
                });
                return;
            }

            if (hasSourceRoot)
            {
                mounts.push_back(MountDescriptor{
                    sourceKey,
                    sourceKey,
                    mountPath,
                    sourceRoot,
                });
                return;
            }

            if (hasCookedCatalog)
            {
                mounts.push_back(MountDescriptor{
                    "Cooked:" + sourceKey,
                    sourceKey,
                    mountPath,
                    cookedRoot,
                });
            }
        };

        addReadableMount(
            "Project",
            Resource::VirtualPath{Resource::PathDomain::Project, std::nullopt, {}},
            rootPath / projectContentDirName,
            cacheDir / "Cooked" / "Project");

        addReadableMount(
            "Engine",
            Resource::VirtualPath{Resource::PathDomain::Engine, std::nullopt, {}},
            engineDir,
            cacheDir / "Cooked" / "Engine");

        const auto pluginsRoot = rootPath / "Plugins";
        const auto cookedPluginsRoot = cacheDir / "Cooked" / "Plugins";
        std::unordered_set<std::string> pluginNames;

        if (std::filesystem::exists(pluginsRoot))
        {
            for (const auto &entry : std::filesystem::directory_iterator(pluginsRoot))
            {
                if (!entry.is_directory())
                    continue;

                const auto pluginName = entry.path().filename().string();
                if (Resource::IsValidPluginMountName(pluginName))
                    pluginNames.insert(std::move(pluginName));
            }
        }

        if (std::filesystem::exists(cookedPluginsRoot))
        {
            for (const auto &entry : std::filesystem::directory_iterator(cookedPluginsRoot))
            {
                if (!entry.is_directory())
                    continue;

                const auto pluginName = entry.path().filename().string();
                if (Resource::IsValidPluginMountName(pluginName))
                    pluginNames.insert(std::move(pluginName));
            }
        }

        std::vector<std::string> sortedPluginNames(pluginNames.begin(), pluginNames.end());
        std::sort(sortedPluginNames.begin(), sortedPluginNames.end());

        for (const auto &pluginName : sortedPluginNames)
        {
            addReadableMount(
                "Plugin:" + pluginName,
                Resource::VirtualPath{Resource::PathDomain::Plugin, pluginName, {}},
                pluginsRoot / pluginName / "Content",
                cookedPluginsRoot / pluginName);
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
                              mount.sourceKey);
                conflictedLogicalPaths.insert(logicalPath);
                globalEntries.erase(existingIt);
                continue;
            }

            globalEntries.emplace(logicalPath,
                                  Resource::CatalogRegistry::GlobalCatalogEntry{
                                      entry,
                                      mount.mountRoot,
                                      mount.sourceKey,
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
                                                                      const std::filesystem::path &cacheDir,
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

            for (const auto &mount : DiscoverReadableMounts(rootPath, engineDir, cacheDir, projectContentDirName))
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
