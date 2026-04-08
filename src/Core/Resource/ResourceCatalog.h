#pragma once

#include "Core/Resource/PathTypes.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Resource
{
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
        std::string sourceRelativePath;
        std::vector<ArtifactRecord> artifacts;
    };

    class CatalogRegistry
    {
    public:
        void Reset();

        std::optional<std::filesystem::path> ResolvePath(const std::filesystem::path &rootPath,
                                                         const std::filesystem::path &engineDir,
                                                         const VirtualPath &virtualPath,
                                                         std::string_view logicalPath,
                                                         std::string_view projectContentDirName);

    private:
        struct MountCatalogCache
        {
            bool attemptedLoad = false;
            std::unordered_map<std::string, ResourceCatalogEntry> entries;
        };

        std::unordered_map<std::string, MountCatalogCache> m_MountCatalogs;
    };
} // namespace Resource
