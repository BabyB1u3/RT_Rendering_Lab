#pragma once

#include "Core/Resource/Catalog/ResourceCatalog.h"

#include <filesystem>
#include <istream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Resource
{
struct ReadableMount
{
    std::string m_CacheKey;
    std::string m_SourceKey;
    VirtualPath m_MountPath;
    MountPriority m_Priority = MountPriority::Source;
    MountBackendKind m_Backend = MountBackendKind::Directory;
    std::filesystem::path m_MountRoot;
};

struct WritableMount
{
    PathDomain m_Domain = PathDomain::Saved;
    std::filesystem::path m_RootPath;
};

std::vector<ReadableMount> DiscoverReadableMountBackends(const std::filesystem::path& rootPath,
                                                         const std::filesystem::path& engineDir,
                                                         const std::filesystem::path& cacheDir,
                                                         std::string_view projectContentDirName,
                                                         std::string_view currentProfile);

bool MountHasCatalog(const ReadableMount& mount);

bool ReadMountCatalogBytes(const ReadableMount& mount,
                           std::vector<uint8_t>& bytes,
                           std::string* errorMessage = nullptr);

std::optional<ResolvedReadableArtifact> ResolveReadableMountArtifact(const ReadableMount& mount,
                                                                     const ArtifactRecord& artifact,
                                                                     std::string* errorMessage = nullptr);

std::optional<std::string> ReadReadableArtifactText(const ResolvedReadableArtifact& artifact,
                                                    std::string* errorMessage = nullptr);

std::optional<std::vector<uint8_t>> ReadReadableArtifactBinary(const ResolvedReadableArtifact& artifact,
                                                               std::string* errorMessage = nullptr);

std::unique_ptr<std::istream> OpenReadableArtifactStream(const ResolvedReadableArtifact& artifact,
                                                         std::string* errorMessage = nullptr);

std::optional<WritableMount>
ResolveWritableMount(PathDomain domain, const std::filesystem::path& savedDir, const std::filesystem::path& cacheDir);
} // namespace Resource
