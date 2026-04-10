#include "Core/Resource/Catalog/ResourceCatalog.h"

#include "Core/Diagnostics/Logging/LogCategories.h"
#include "Core/Diagnostics/Logging/LogMacros.h"
#include "Core/Resource/Mount/MountBackend.h"
#include "Core/Resource/Path/PathParser.h"

#include <algorithm>
#include <cstdlib>
#include <json.hpp>
#include <unordered_set>

namespace
{
    using Json = nlohmann::json;

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
        case Resource::PathDomain::Saved:
        case Resource::PathDomain::Cache:
            return {};
        }

        return {};
    }

    bool DomainMatchesMount(const Resource::VirtualPath &catalogPath, const Resource::VirtualPath &requestedPath)
    {
        return catalogPath.domain == requestedPath.domain;
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

    bool ParseCatalogFromJson(const Json &rootJson,
                             const std::string &catalogLabel,
                             const Resource::VirtualPath &mountPath,
                             Resource::CatalogKind &catalogKind,
                             int &catalogVersion,
                             std::unordered_map<std::string, Resource::ResourceCatalogEntry> &entries)
    {
        const auto versionIt = rootJson.find("version");
        if (versionIt == rootJson.end() || !versionIt->is_number_integer())
        {
            LOG_ERROR_CAT(LogCategory::FileSystem, "Unsupported catalog version in '{}'", catalogLabel);
            return false;
        }

        catalogVersion = versionIt->get<int>();
        const bool isCookedCatalog = catalogVersion == 2;
        if (catalogVersion != 1 && !isCookedCatalog)
        {
            LOG_ERROR_CAT(LogCategory::FileSystem, "Unsupported catalog version in '{}'", catalogLabel);
            return false;
        }

        catalogKind = isCookedCatalog ? Resource::CatalogKind::Cooked : Resource::CatalogKind::Source;

        if (isCookedCatalog)
        {
            const auto kindIt = rootJson.find("kind");
            if (kindIt == rootJson.end() || !kindIt->is_string() || kindIt->get<std::string>() != "cooked")
            {
                LOG_ERROR_CAT(LogCategory::FileSystem,
                              "Cooked catalog '{}' is missing kind='cooked'",
                              catalogLabel);
                return false;
            }
        }

        const auto entriesIt = rootJson.find("entries");
        if (entriesIt == rootJson.end() || !entriesIt->is_array())
        {
            LOG_ERROR_CAT(LogCategory::FileSystem, "Catalog '{}' is missing an entries array", catalogLabel);
            return false;
        }

        for (const auto &entryJson : *entriesIt)
        {
            if (!entryJson.is_object())
            {
                LOG_ERROR_CAT(LogCategory::FileSystem, "Catalog '{}' contains a non-object entry", catalogLabel);
                return false;
            }

            const auto logicalPathIt = entryJson.find("logicalPath");
            if (logicalPathIt == entryJson.end() || !logicalPathIt->is_string())
            {
                LOG_ERROR_CAT(LogCategory::FileSystem, "Catalog '{}' contains an entry without logicalPath", catalogLabel);
                return false;
            }

            const std::string logicalPath = logicalPathIt->get<std::string>();
            const auto parsedLogicalPath = Resource::ParseVirtualPath(logicalPath);
            if (!parsedLogicalPath.has_value() || !Resource::IsCatalogBackedPath(logicalPath) ||
                !DomainMatchesMount(*parsedLogicalPath, mountPath))
            {
                LOG_ERROR_CAT(LogCategory::FileSystem,
                              "Catalog '{}' contains invalid mount-scoped logical path '{}'",
                              catalogLabel,
                              logicalPath);
                return false;
            }

            Resource::ResourceCatalogEntry entry;
            entry.logicalPath = logicalPath;
            if (!isCookedCatalog)
            {
                const auto sourceRelativePathIt = entryJson.find("sourceRelativePath");
                if (sourceRelativePathIt == entryJson.end() || !sourceRelativePathIt->is_string())
                {
                    LOG_ERROR_CAT(LogCategory::FileSystem,
                                  "Source catalog '{}' entry '{}' is missing sourceRelativePath",
                                  catalogLabel,
                                  logicalPath);
                    return false;
                }

                entry.sourceRelativePath = sourceRelativePathIt->get<std::string>();
            }

            const auto artifactsIt = entryJson.find("artifacts");
            if (artifactsIt == entryJson.end() || !artifactsIt->is_array() || artifactsIt->empty())
            {
                LOG_ERROR_CAT(LogCategory::FileSystem,
                              "Catalog '{}' entry '{}' has no artifacts",
                              catalogLabel,
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
                                  catalogLabel,
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
                              catalogLabel,
                              it->first);
                return false;
            }
        }

        return true;
    }

    bool LoadCatalogFromMount(const Resource::ReadableMount &mount,
                             Resource::CatalogKind &catalogKind,
                             int &catalogVersion,
                             std::unordered_map<std::string, Resource::ResourceCatalogEntry> &entries)
    {
        std::vector<uint8_t> bytes;
        std::string errorMessage;
        if (!Resource::ReadMountCatalogBytes(mount, bytes, &errorMessage))
        {
            if (!errorMessage.empty())
                LOG_ERROR_CAT(LogCategory::FileSystem, "Failed to read mount catalog '{}': {}", mount.mountRoot.string(), errorMessage);
            return false;
        }

        Json rootJson;
        try
        {
            rootJson = Json::parse(bytes.begin(), bytes.end());
        }
        catch (const std::exception &e)
        {
            LOG_ERROR_CAT(LogCategory::FileSystem, "Failed to parse catalog '{}': {}", mount.mountRoot.string(), e.what());
            return false;
        }

        const std::string catalogLabel = mount.backend == Resource::MountBackendKind::PakArchive
                                             ? mount.mountRoot.string() + "::/.rtr/catalog.json"
                                             : (mount.mountRoot / ".rtr" / "catalog.json").string();
        return ParseCatalogFromJson(rootJson, catalogLabel, mount.mountPath, catalogKind, catalogVersion, entries);
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

    std::string_view GetCurrentArtifactProfileTag()
    {
        const auto profile = GetCurrentProfileTag();
        if (profile == "packaged" || profile == "shipping")
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

    void MergeMountEntriesIntoGlobalTable(
        const Resource::ReadableMount &mount,
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
                if (static_cast<int>(mount.priority) > static_cast<int>(existingIt->second.priority))
                {
                    existingIt->second = Resource::CatalogRegistry::GlobalCatalogEntry{
                        entry,
                        cache.kind,
                        cache.version,
                        mount.priority,
                        mount.backend,
                        mount.mountRoot,
                        mount.materializedRoot,
                        mount.sourceKey,
                    };
                    continue;
                }

                if (static_cast<int>(mount.priority) < static_cast<int>(existingIt->second.priority))
                    continue;

                LOG_ERROR_CAT(LogCategory::FileSystem,
                              "Logical path '{}' is provided by multiple equal-precedence mounts ('{}' and '{}')",
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
                                      cache.kind,
                                      cache.version,
                                      mount.priority,
                                      mount.backend,
                                      mount.mountRoot,
                                      mount.materializedRoot,
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

            for (const auto &mount : Resource::DiscoverReadableMountBackends(
                     rootPath, engineDir, cacheDir, projectContentDirName, GetCurrentProfileTag()))
            {
                auto &cache = m_MountCatalogs[mount.cacheKey];
                if (!cache.attemptedLoad)
                {
                    cache.attemptedLoad = true;
                    if (Resource::MountHasCatalog(mount))
                    {
                        cache.kind = CatalogKind::Source;
                        cache.version = 0;
                        std::unordered_map<std::string, ResourceCatalogEntry> loadedEntries;
                        const bool loaded = LoadCatalogFromMount(mount, cache.kind, cache.version, loadedEntries);
                        if (!loaded)
                        {
                            cache.entries.clear();
                            LOG_ERROR_CAT(LogCategory::FileSystem, "Failed to load catalog '{}'", mount.mountRoot.string());
                        }
                        else
                        {
                            cache.entries = std::move(loadedEntries);
                            LOG_INFO_CAT(LogCategory::FileSystem, "Loaded catalog '{}'", mount.mountRoot.string());
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

        const ReadableMount mount{
            .cacheKey = {},
            .sourceKey = entryIt->second.sourceMountKey,
            .mountPath = virtualPath,
            .priority = entryIt->second.priority,
            .backend = entryIt->second.backend,
            .mountRoot = entryIt->second.mountRoot,
            .materializedRoot = entryIt->second.materializedRoot,
        };

        std::string errorMessage;
        const auto resolvedArtifact = ResolveReadableMountArtifact(mount, *artifact, &errorMessage);
        if (!resolvedArtifact.has_value() && !errorMessage.empty())
        {
            LOG_ERROR_CAT(LogCategory::FileSystem,
                          "Failed to resolve artifact '{}' from mount '{}': {}",
                          artifact->relativePath,
                          entryIt->second.mountRoot.string(),
                          errorMessage);
        }

        return resolvedArtifact;
    }
} // namespace Resource
