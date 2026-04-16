#pragma once

#include "Core/Resource/Path/PathTypes.h"

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
    DLC = 300,
    Patch = 400,
    Mod = 500,
};

struct ArtifactRecord
{
    std::string m_RelativePath;
    std::string m_Format;
    std::string m_PlatformTag = "any";
    std::string m_BackendTag = "any";
    std::string m_ProfileTag = "any";
    uint64_t m_ContentHash = 0;
};

struct ResourceCatalogEntry
{
    std::string m_LogicalPath;
    std::optional<std::string> m_SourceRelativePath;
    std::vector<ArtifactRecord> m_Artifacts;
};

struct ResolvedReadableArtifact
{
    MountBackendKind m_Backend = MountBackendKind::Directory;
    std::filesystem::path m_MountRoot;
    std::filesystem::path m_RelativePath;
};

class CatalogRegistry
{
public:
    struct MountCatalogCache
    {
        bool m_HasAttemptedLoad = false;
        CatalogKind m_Kind = CatalogKind::Source;
        int m_Version = 0;
        std::unordered_map<std::string, ResourceCatalogEntry> m_Entries;
    };

    struct GlobalCatalogEntry
    {
        ResourceCatalogEntry m_Entry;
        CatalogKind m_Kind = CatalogKind::Source;
        int m_Version = 0;
        MountPriority m_Priority = MountPriority::Source;
        MountBackendKind m_Backend = MountBackendKind::Directory;
        std::filesystem::path m_MountRoot;
        std::string m_SourceMountKey;
    };

    void Reset();

    std::optional<ResolvedReadableArtifact> ResolveArtifact(const std::filesystem::path& rootPath,
                                                            const std::filesystem::path& engineDir,
                                                            const std::filesystem::path& cacheDir,
                                                            const VirtualPath& virtualPath,
                                                            std::string_view logicalPath,
                                                            std::string_view projectContentDirName);

private:
    bool m_GlobalTableBuilt = false;
    std::unordered_map<std::string, MountCatalogCache> m_MountCatalogs;
    std::unordered_map<std::string, GlobalCatalogEntry> m_GlobalEntries;
    std::unordered_set<std::string> m_ConflictedLogicalPaths;
};
} // namespace Resource
