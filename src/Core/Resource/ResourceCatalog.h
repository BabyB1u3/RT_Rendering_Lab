#pragma once

#include "Core/Resource/PathTypes.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <unordered_map>
#include <vector>

namespace Resource
{
    enum class CatalogKind
    {
        Source,
        Cooked,
    };

    enum class MountBackendKind
    {
        Directory,
        PakArchive,
    };

    enum class MountPriority
    {
        Source = 0,
        Cooked = 100,
        Packaged = 200,
        Overlay = 300,
    };

    struct ArtifactRecord
    {
        std::string relativePath;
        std::string format;
        std::string platformTag = "any";
        std::string backendTag = "any";
        std::string profileTag = "any";
        uint64_t contentHash = 0;
    };

    struct ResourceCatalogEntry
    {
        std::string logicalPath;
        std::optional<std::string> sourceRelativePath;
        std::vector<ArtifactRecord> artifacts;
    };

    class CatalogRegistry
    {
    public:
        struct MountCatalogCache
        {
            bool attemptedLoad = false;
            CatalogKind kind = CatalogKind::Source;
            int version = 0;
            std::unordered_map<std::string, ResourceCatalogEntry> entries;
        };

        struct GlobalCatalogEntry
        {
            ResourceCatalogEntry entry;
            CatalogKind kind = CatalogKind::Source;
            int version = 0;
            MountPriority priority = MountPriority::Source;
            MountBackendKind backend = MountBackendKind::Directory;
            std::filesystem::path mountRoot;
            std::filesystem::path materializedRoot;
            std::string sourceMountKey;
        };

        void Reset();

        std::optional<std::filesystem::path> ResolvePath(const std::filesystem::path &rootPath,
                                                         const std::filesystem::path &engineDir,
                                                         const std::filesystem::path &cacheDir,
                                                         const VirtualPath &virtualPath,
                                                         std::string_view logicalPath,
                                                         std::string_view projectContentDirName);

    private:
        bool m_GlobalTableBuilt = false;
        std::unordered_map<std::string, MountCatalogCache> m_MountCatalogs;
        std::unordered_map<std::string, GlobalCatalogEntry> m_GlobalEntries;
        std::unordered_set<std::string> m_ConflictedLogicalPaths;
    };
} // namespace Resource
