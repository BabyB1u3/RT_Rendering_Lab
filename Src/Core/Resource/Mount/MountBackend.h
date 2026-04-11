#pragma once

#include "Core/Resource/Catalog/ResourceCatalog.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Resource
{
    struct ReadableMount
    {
        std::string cacheKey;
        std::string sourceKey;
        VirtualPath mountPath;
        MountPriority priority = MountPriority::Source;
        MountBackendKind backend = MountBackendKind::Directory;
        std::filesystem::path mountRoot;
        std::filesystem::path materializedRoot;
    };

    struct WritableMount
    {
        PathDomain domain = PathDomain::Saved;
        std::filesystem::path rootPath;
    };

    std::vector<ReadableMount> DiscoverReadableMountBackends(const std::filesystem::path &rootPath,
                                                             const std::filesystem::path &engineDir,
                                                             const std::filesystem::path &cacheDir,
                                                             std::string_view projectContentDirName,
                                                             std::string_view currentProfile);

    bool MountHasCatalog(const ReadableMount &mount);

    bool ReadMountCatalogBytes(const ReadableMount &mount,
                               std::vector<uint8_t> &bytes,
                               std::string *errorMessage = nullptr);

    std::optional<ResolvedReadableArtifact> ResolveReadableMountArtifact(const ReadableMount &mount,
                                                                         const ArtifactRecord &artifact,
                                                                         std::string *errorMessage = nullptr);

    std::optional<std::filesystem::path> MaterializeReadableArtifact(const ResolvedReadableArtifact &artifact,
                                                                     std::string *errorMessage = nullptr);

    std::optional<std::string> ReadReadableArtifactText(const ResolvedReadableArtifact &artifact,
                                                        std::string *errorMessage = nullptr);

    std::optional<std::vector<uint8_t>> ReadReadableArtifactBinary(const ResolvedReadableArtifact &artifact,
                                                                   std::string *errorMessage = nullptr);

    std::optional<WritableMount> ResolveWritableMount(PathDomain domain,
                                                      const std::filesystem::path &savedDir,
                                                      const std::filesystem::path &cacheDir);
} // namespace Resource
