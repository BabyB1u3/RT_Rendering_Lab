#include "Core/Resource/ResourceCatalog.h"

#include "Core/Diagnostics/LogCategories.h"
#include "Core/Diagnostics/LogMacros.h"
#include "Core/Resource/PakArchive.h"
#include "Core/Resource/PathParser.h"

#include <algorithm>
#include <cctype>
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
        Resource::MountPriority priority = Resource::MountPriority::Source;
        Resource::MountBackendKind backend = Resource::MountBackendKind::Directory;
        std::filesystem::path mountRoot;
        std::filesystem::path materializedRoot;
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

    std::string SanitizeMountKeyForPath(std::string_view key)
    {
        std::string sanitized;
        sanitized.reserve(key.size());
        for (const char c : key)
        {
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-')
                sanitized.push_back(c);
            else
                sanitized.push_back('_');
        }
        return sanitized;
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

    bool LoadCatalogFromJson(const std::filesystem::path &catalogPath,
                             const Resource::VirtualPath &mountPath,
                             Resource::CatalogKind &catalogKind,
                             int &catalogVersion,
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

        return ParseCatalogFromJson(rootJson, catalogPath.string(), mountPath, catalogKind, catalogVersion, entries);
    }

    bool LoadCatalogFromPak(const std::filesystem::path &pakPath,
                            const Resource::VirtualPath &mountPath,
                            Resource::CatalogKind &catalogKind,
                            int &catalogVersion,
                            std::unordered_map<std::string, Resource::ResourceCatalogEntry> &entries)
    {
        std::string errorMessage;
        const auto bytes = Resource::ReadPakEntry(pakPath, ".rtr/catalog.json", &errorMessage);
        if (!bytes.has_value())
        {
            if (!errorMessage.empty())
            {
                LOG_ERROR_CAT(LogCategory::FileSystem, "Failed to read pak catalog '{}': {}", pakPath.string(), errorMessage);
            }
            return false;
        }

        Json rootJson;
        try
        {
            rootJson = Json::parse(bytes->begin(), bytes->end());
        }
        catch (const std::exception &e)
        {
            LOG_ERROR_CAT(LogCategory::FileSystem, "Failed to parse pak catalog '{}': {}", pakPath.string(), e.what());
            return false;
        }

        return ParseCatalogFromJson(rootJson, pakPath.string() + "::/.rtr/catalog.json", mountPath, catalogKind, catalogVersion, entries);
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

    std::vector<MountDescriptor> DiscoverReadableMounts(const std::filesystem::path &rootPath,
                                                        const std::filesystem::path &engineDir,
                                                        const std::filesystem::path &cacheDir,
                                                        std::string_view projectContentDirName)
    {
        std::vector<MountDescriptor> mounts;
        const bool preferCookedArtifacts = GetCurrentProfileTag() == "cooked";
        const bool preferPackagedArtifacts = GetCurrentProfileTag() == "packaged" || GetCurrentProfileTag() == "shipping";
        std::vector<std::filesystem::path> cookedRootSearchOrder;
        std::vector<std::filesystem::path> packagedRootSearchOrder;
        std::vector<std::filesystem::path> overlayRootSearchOrder;

        if (const char *overrideValue = std::getenv("RTRLAB_COOKED_ROOT"))
        {
            const std::filesystem::path overrideRoot = overrideValue;
            if (!overrideRoot.empty())
                cookedRootSearchOrder.push_back(overrideRoot);
        }
        if (const char *overrideValue = std::getenv("RTRLAB_PACKAGE_ROOT"))
        {
            const std::filesystem::path overrideRoot = overrideValue;
            if (!overrideRoot.empty())
                packagedRootSearchOrder.push_back(overrideRoot);
        }
        if (const char *overrideValue = std::getenv("RTRLAB_OVERLAY_ROOT"))
        {
            const std::filesystem::path overrideRoot = overrideValue;
            if (!overrideRoot.empty())
                overlayRootSearchOrder.push_back(overrideRoot);
        }

        cookedRootSearchOrder.push_back(cacheDir / "Cooked");
        cookedRootSearchOrder.push_back(rootPath / "build" / "Cooked");
        packagedRootSearchOrder.push_back(cacheDir / "Packaged");
        packagedRootSearchOrder.push_back(rootPath / "build" / "Packaged");
        overlayRootSearchOrder.push_back(rootPath / "Saved" / "Overrides");

        auto findCookedMountRoot = [&](const std::filesystem::path &mountRelativePath) -> std::filesystem::path {
            for (const auto &cookedRoot : cookedRootSearchOrder)
            {
                const auto candidate = cookedRoot / mountRelativePath;
                if (std::filesystem::exists(candidate / ".rtr" / "catalog.json"))
                    return candidate;
            }

            return {};
        };

        auto findPackagedMountArchive = [&](const std::filesystem::path &archiveRelativePath) -> std::filesystem::path {
            for (const auto &packagedRoot : packagedRootSearchOrder)
            {
                const auto candidate = packagedRoot / archiveRelativePath;
                std::string errorMessage;
                if (std::filesystem::exists(candidate) && Resource::PakEntryExists(candidate, ".rtr/catalog.json", &errorMessage))
                    return candidate;
            }

            return {};
        };

        auto findOverlayMountRoot = [&](const std::filesystem::path &mountRelativePath) -> std::filesystem::path {
            for (const auto &overlayRoot : overlayRootSearchOrder)
            {
                const auto candidate = overlayRoot / mountRelativePath;
                if (std::filesystem::exists(candidate / ".rtr" / "catalog.json"))
                    return candidate;
            }

            return {};
        };

        const auto appendReadableMount = [&](std::string cacheKey,
                                             const std::string &sourceKey,
                                             const Resource::VirtualPath &mountPath,
                                             const Resource::MountPriority priority,
                                             const Resource::MountBackendKind backend,
                                             const std::filesystem::path &mountRoot,
                                             const std::filesystem::path &materializedRoot) {
            if (mountRoot.empty())
                return;

            mounts.push_back(MountDescriptor{
                std::move(cacheKey),
                sourceKey,
                mountPath,
                priority,
                backend,
                mountRoot,
                materializedRoot,
            });
        };

        const auto addReadableMount = [&](const std::string &sourceKey,
                                          const Resource::VirtualPath &mountPath,
                                          const std::filesystem::path &sourceRoot,
                                          const std::filesystem::path &cookedMountRelativePath,
                                          const std::filesystem::path &packagedArchiveRelativePath) {
            const auto overlayRoot = findOverlayMountRoot(cookedMountRelativePath);
            const auto cookedRoot = findCookedMountRoot(cookedMountRelativePath);
            const auto packagedArchive = findPackagedMountArchive(packagedArchiveRelativePath);
            const bool hasOverlayCatalog = !overlayRoot.empty();
            const bool hasCookedCatalog = !cookedRoot.empty();
            const bool hasPackagedCatalog = !packagedArchive.empty();
            const bool hasSourceRoot = std::filesystem::exists(sourceRoot);
            const auto materializedRoot = cacheDir / "PackagedExtracted" / SanitizeMountKeyForPath(sourceKey);

            if (hasOverlayCatalog)
            {
                appendReadableMount("Overlay:" + sourceKey,
                                    sourceKey,
                                    mountPath,
                                    Resource::MountPriority::Overlay,
                                    Resource::MountBackendKind::Directory,
                                    overlayRoot,
                                    {});
            }

            if (preferPackagedArtifacts)
            {
                if (hasPackagedCatalog)
                {
                    appendReadableMount("Packaged:" + sourceKey,
                                        sourceKey,
                                        mountPath,
                                        Resource::MountPriority::Packaged,
                                        Resource::MountBackendKind::PakArchive,
                                        packagedArchive,
                                        materializedRoot);
                }
                if (hasCookedCatalog)
                {
                    appendReadableMount("Cooked:" + sourceKey,
                                        sourceKey,
                                        mountPath,
                                        Resource::MountPriority::Cooked,
                                        Resource::MountBackendKind::Directory,
                                        cookedRoot,
                                        {});
                }
                if (hasSourceRoot)
                {
                    appendReadableMount(sourceKey,
                                        sourceKey,
                                        mountPath,
                                        Resource::MountPriority::Source,
                                        Resource::MountBackendKind::Directory,
                                        sourceRoot,
                                        {});
                }
                return;
            }

            if (preferCookedArtifacts)
            {
                if (hasCookedCatalog)
                {
                    appendReadableMount("Cooked:" + sourceKey,
                                        sourceKey,
                                        mountPath,
                                        Resource::MountPriority::Cooked,
                                        Resource::MountBackendKind::Directory,
                                        cookedRoot,
                                        {});
                }
                if (hasSourceRoot)
                {
                    appendReadableMount(sourceKey,
                                        sourceKey,
                                        mountPath,
                                        Resource::MountPriority::Source,
                                        Resource::MountBackendKind::Directory,
                                        sourceRoot,
                                        {});
                }
                return;
            }

            if (hasSourceRoot)
            {
                appendReadableMount(sourceKey,
                                    sourceKey,
                                    mountPath,
                                    Resource::MountPriority::Source,
                                    Resource::MountBackendKind::Directory,
                                    sourceRoot,
                                    {});
                return;
            }

            if (hasPackagedCatalog)
            {
                appendReadableMount("Packaged:" + sourceKey,
                                    sourceKey,
                                    mountPath,
                                    Resource::MountPriority::Packaged,
                                    Resource::MountBackendKind::PakArchive,
                                    packagedArchive,
                                    materializedRoot);
            }
            if (hasCookedCatalog)
            {
                appendReadableMount("Cooked:" + sourceKey,
                                    sourceKey,
                                    mountPath,
                                    Resource::MountPriority::Cooked,
                                    Resource::MountBackendKind::Directory,
                                    cookedRoot,
                                    {});
            }
        };

        addReadableMount(
            "Project",
            Resource::VirtualPath{Resource::PathDomain::Project, std::nullopt, {}},
            rootPath / projectContentDirName,
            "Project",
            std::filesystem::path(std::string("Project") + std::string(Resource::kPakArchiveExtension)));

        addReadableMount(
            "Engine",
            Resource::VirtualPath{Resource::PathDomain::Engine, std::nullopt, {}},
            engineDir,
            "Engine",
            std::filesystem::path(std::string("Engine") + std::string(Resource::kPakArchiveExtension)));

        const auto pluginsRoot = rootPath / "Plugins";
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

        for (const auto &cookedRoot : cookedRootSearchOrder)
        {
            const auto cookedPluginsRoot = cookedRoot / "Plugins";
            if (!std::filesystem::exists(cookedPluginsRoot))
                continue;

            for (const auto &entry : std::filesystem::directory_iterator(cookedPluginsRoot))
            {
                if (!entry.is_directory())
                    continue;

                const auto pluginName = entry.path().filename().string();
                if (Resource::IsValidPluginMountName(pluginName))
                    pluginNames.insert(std::move(pluginName));
            }
        }

        for (const auto &packagedRoot : packagedRootSearchOrder)
        {
            const auto packagedPluginsRoot = packagedRoot / "Plugins";
            if (!std::filesystem::exists(packagedPluginsRoot))
                continue;

            for (const auto &entry : std::filesystem::directory_iterator(packagedPluginsRoot))
            {
                if (!entry.is_regular_file())
                    continue;

                const auto path = entry.path();
                if (path.extension().string() != Resource::kPakArchiveExtension)
                    continue;

                const auto pluginName = path.stem().string();
                if (Resource::IsValidPluginMountName(pluginName))
                    pluginNames.insert(std::move(pluginName));
            }
        }

        for (const auto &overlayRoot : overlayRootSearchOrder)
        {
            const auto overlayPluginsRoot = overlayRoot / "Plugins";
            if (!std::filesystem::exists(overlayPluginsRoot))
                continue;

            for (const auto &entry : std::filesystem::directory_iterator(overlayPluginsRoot))
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
                std::filesystem::path("Plugins") / pluginName,
                std::filesystem::path("Plugins") / (pluginName + std::string(Resource::kPakArchiveExtension)));
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

            for (const auto &mount : DiscoverReadableMounts(rootPath, engineDir, cacheDir, projectContentDirName))
            {
                auto &cache = m_MountCatalogs[mount.cacheKey];
                if (!cache.attemptedLoad)
                {
                    cache.attemptedLoad = true;
                    const auto catalogPath = mount.mountRoot / ".rtr" / "catalog.json";
                    const bool hasDirectoryCatalog = mount.backend == MountBackendKind::Directory && std::filesystem::exists(catalogPath);
                    const bool hasPakCatalog = mount.backend == MountBackendKind::PakArchive;
                    if (hasDirectoryCatalog || hasPakCatalog)
                    {
                        cache.kind = CatalogKind::Source;
                        cache.version = 0;
                        std::unordered_map<std::string, ResourceCatalogEntry> loadedEntries;
                        const bool loaded = mount.backend == MountBackendKind::Directory
                                                ? LoadCatalogFromJson(catalogPath, mount.mountPath, cache.kind, cache.version, loadedEntries)
                                                : LoadCatalogFromPak(mount.mountRoot, mount.mountPath, cache.kind, cache.version, loadedEntries);
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

        if (entryIt->second.backend == MountBackendKind::PakArchive)
        {
            std::string errorMessage;
            const auto materialized = MaterializePakEntry(entryIt->second.mountRoot,
                                                          artifact->relativePath,
                                                          entryIt->second.materializedRoot,
                                                          &errorMessage);
            if (!materialized.has_value())
            {
                if (!errorMessage.empty())
                {
                    LOG_ERROR_CAT(LogCategory::FileSystem,
                                  "Failed to materialize pak entry '{}' from '{}': {}",
                                  artifact->relativePath,
                                  entryIt->second.mountRoot.string(),
                                  errorMessage);
                }
                return std::nullopt;
            }

            return materialized;
        }

        return entryIt->second.mountRoot / artifact->relativePath;
    }
} // namespace Resource
