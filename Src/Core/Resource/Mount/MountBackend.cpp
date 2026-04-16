#include "Core/Resource/Mount/MountBackend.h"

#include "Core/Resource/IO/PhysicalIO.h"
#include "Core/Resource/Package/PakArchive.h"
#include "Core/Resource/Path/PathParser.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <sstream>

namespace Resource
{
namespace
{
std::vector<std::filesystem::path> CollectPakArchives(const std::filesystem::path& directory)
{
    std::vector<std::filesystem::path> archives;
    std::error_code ec;
    if (!std::filesystem::exists(directory, ec) || ec || !std::filesystem::is_directory(directory, ec))
        return archives;

    for (const auto& entry : std::filesystem::directory_iterator(directory, ec))
    {
        if (ec)
            break;

        if (!entry.is_regular_file())
            continue;

        if (entry.path().extension() == k_PakArchiveExtension)
            archives.push_back(entry.path());
    }

    std::sort(archives.begin(),
              archives.end(),
              [](const auto& lhs, const auto& rhs) { return lhs.generic_string() < rhs.generic_string(); });
    return archives;
}
} // namespace

std::vector<ReadableMount> DiscoverReadableMountBackends(const std::filesystem::path& rootPath,
                                                         const std::filesystem::path& engineDir,
                                                         const std::filesystem::path& cacheDir,
                                                         std::string_view projectContentDirName,
                                                         std::string_view currentProfile)
{
    (void)cacheDir;

    std::vector<ReadableMount> mounts;

    const auto appendReadableMount = [&](std::string cacheKey,
                                         const std::string& sourceKey,
                                         const VirtualPath& mountPath,
                                         const MountPriority priority,
                                         const MountBackendKind backend,
                                         const std::filesystem::path& mountRoot)
    {
        if (mountRoot.empty())
            return;

        mounts.push_back(ReadableMount{
            std::move(cacheKey),
            sourceKey,
            mountPath,
            priority,
            backend,
            mountRoot,
        });
    };

    const auto appendArchiveDomainMounts = [&](std::string_view layerName,
                                               const std::string& archiveName,
                                               const MountPriority priority,
                                               const std::filesystem::path& archivePath)
    {
        appendReadableMount(std::string(layerName) + ":" + archiveName + ":Project",
                            std::string(layerName) + ":" + archiveName + ":Project",
                            VirtualPath{PathDomain::Project, std::nullopt, {}},
                            priority,
                            MountBackendKind::PakArchive,
                            archivePath);
        appendReadableMount(std::string(layerName) + ":" + archiveName + ":Engine",
                            std::string(layerName) + ":" + archiveName + ":Engine",
                            VirtualPath{PathDomain::Engine, std::nullopt, {}},
                            priority,
                            MountBackendKind::PakArchive,
                            archivePath);
    };

    const auto appendNamespacedArchiveMount = [&](std::string_view layerName,
                                                  const std::string& archiveName,
                                                  const PathDomain domain,
                                                  const MountPriority priority,
                                                  const std::filesystem::path& archivePath)
    {
        appendReadableMount(std::string(layerName) + ":" + archiveName,
                            std::string(layerName) + ":" + archiveName,
                            VirtualPath{domain, archiveName, {}},
                            priority,
                            MountBackendKind::PakArchive,
                            archivePath);
    };

    if (currentProfile == "shipping")
    {
        const auto packagedArchive = GetGamePackagedArchivePath(rootPath);
        std::string errorMessage;
        if (std::filesystem::exists(packagedArchive) &&
            PakEntryExists(packagedArchive, ".rtr/catalog.json", &errorMessage))
        {
            appendReadableMount("Packaged:Project",
                                "Project",
                                VirtualPath{PathDomain::Project, std::nullopt, {}},
                                MountPriority::Packaged,
                                MountBackendKind::PakArchive,
                                packagedArchive);
            appendReadableMount("Packaged:Engine",
                                "Engine",
                                VirtualPath{PathDomain::Engine, std::nullopt, {}},
                                MountPriority::Packaged,
                                MountBackendKind::PakArchive,
                                packagedArchive);
        }

        const auto appendOverlayArchives = [&](const std::filesystem::path& directory,
                                               std::string_view layerName,
                                               const MountPriority priority,
                                               const std::optional<PathDomain> namespacedDomain)
        {
            for (const auto& archivePath : CollectPakArchives(directory))
            {
                appendArchiveDomainMounts(layerName, archivePath.stem().string(), priority, archivePath);
                if (namespacedDomain.has_value())
                {
                    appendNamespacedArchiveMount(
                        layerName, archivePath.stem().string(), *namespacedDomain, priority, archivePath);
                }
            }
        };

        appendOverlayArchives(rootPath / "DLC", "DLC", MountPriority::DLC, PathDomain::DLC);
        appendOverlayArchives(rootPath / "Patches", "Patch", MountPriority::Patch, std::nullopt);
        appendOverlayArchives(rootPath / "Mods", "Mod", MountPriority::Mod, PathDomain::Mod);

        return mounts;
    }

    const auto projectRoot = rootPath / projectContentDirName;
    if (std::filesystem::exists(projectRoot))
    {
        appendReadableMount("Project",
                            "Project",
                            VirtualPath{PathDomain::Project, std::nullopt, {}},
                            MountPriority::Source,
                            MountBackendKind::Directory,
                            projectRoot);
    }

    if (std::filesystem::exists(engineDir))
    {
        appendReadableMount("Engine",
                            "Engine",
                            VirtualPath{PathDomain::Engine, std::nullopt, {}},
                            MountPriority::Source,
                            MountBackendKind::Directory,
                            engineDir);
    }

    return mounts;
}

bool MountHasCatalog(const ReadableMount& mount)
{
    switch (mount.m_Backend)
    {
        case MountBackendKind::Directory:
            return std::filesystem::exists(mount.m_MountRoot / ".rtr" / "catalog.json");
        case MountBackendKind::PakArchive:
        {
            std::string errorMessage;
            return PakEntryExists(mount.m_MountRoot, ".rtr/catalog.json", &errorMessage);
        }
    }

    return false;
}

bool ReadMountCatalogBytes(const ReadableMount& mount, std::vector<uint8_t>& bytes, std::string* errorMessage)
{
    switch (mount.m_Backend)
    {
        case MountBackendKind::Directory:
        {
            const auto catalogPath = mount.m_MountRoot / ".rtr" / "catalog.json";
            std::ifstream in(catalogPath, std::ios::binary);
            if (!in.is_open())
            {
                if (errorMessage != nullptr)
                    *errorMessage = "failed to open mount catalog: " + catalogPath.string();
                return false;
            }

            bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
            return true;
        }
        case MountBackendKind::PakArchive:
        {
            const auto pakBytes = ReadPakEntry(mount.m_MountRoot, ".rtr/catalog.json", errorMessage);
            if (!pakBytes.has_value())
                return false;

            bytes = std::move(*pakBytes);
            return true;
        }
    }

    return false;
}

std::optional<ResolvedReadableArtifact>
ResolveReadableMountArtifact(const ReadableMount& mount, const ArtifactRecord& artifact, std::string* errorMessage)
{
    (void)errorMessage;

    switch (mount.m_Backend)
    {
        case MountBackendKind::Directory:
            return ResolvedReadableArtifact{
                .m_Backend = mount.m_Backend,
                .m_MountRoot = mount.m_MountRoot,
                .m_RelativePath = artifact.m_RelativePath,
            };
        case MountBackendKind::PakArchive:
            return ResolvedReadableArtifact{
                .m_Backend = mount.m_Backend,
                .m_MountRoot = mount.m_MountRoot,
                .m_RelativePath = artifact.m_RelativePath,
            };
    }

    return std::nullopt;
}

std::optional<std::string> ReadReadableArtifactText(const ResolvedReadableArtifact& artifact, std::string* errorMessage)
{
    switch (artifact.m_Backend)
    {
        case MountBackendKind::Directory:
        {
            const auto text = ReadTextFile(artifact.m_MountRoot / artifact.m_RelativePath);
            if (!text.has_value() && errorMessage != nullptr)
                *errorMessage =
                    "failed to read text file: " + (artifact.m_MountRoot / artifact.m_RelativePath).string();
            return text;
        }
        case MountBackendKind::PakArchive:
        {
            const auto bytes = ReadPakEntry(artifact.m_MountRoot, artifact.m_RelativePath, errorMessage);
            if (!bytes.has_value())
                return std::nullopt;

            return std::string(bytes->begin(), bytes->end());
        }
    }

    return std::nullopt;
}

std::optional<std::vector<uint8_t>> ReadReadableArtifactBinary(const ResolvedReadableArtifact& artifact,
                                                               std::string* errorMessage)
{
    switch (artifact.m_Backend)
    {
        case MountBackendKind::Directory:
        {
            const auto bytes = ReadBinaryFile(artifact.m_MountRoot / artifact.m_RelativePath);
            if (!bytes.has_value() && errorMessage != nullptr)
                *errorMessage =
                    "failed to read binary file: " + (artifact.m_MountRoot / artifact.m_RelativePath).string();
            return bytes;
        }
        case MountBackendKind::PakArchive:
            return ReadPakEntry(artifact.m_MountRoot, artifact.m_RelativePath, errorMessage);
    }

    return std::nullopt;
}

std::unique_ptr<std::istream> OpenReadableArtifactStream(const ResolvedReadableArtifact& artifact,
                                                         std::string* errorMessage)
{
    switch (artifact.m_Backend)
    {
        case MountBackendKind::Directory:
        {
            auto stream =
                std::make_unique<std::ifstream>(artifact.m_MountRoot / artifact.m_RelativePath, std::ios::binary);
            if (!stream->is_open())
            {
                if (errorMessage != nullptr)
                    *errorMessage = "failed to open readable artifact stream: " +
                                    (artifact.m_MountRoot / artifact.m_RelativePath).string();
                return nullptr;
            }

            return stream;
        }
        case MountBackendKind::PakArchive:
        {
            const auto bytes = ReadPakEntry(artifact.m_MountRoot, artifact.m_RelativePath, errorMessage);
            if (!bytes.has_value())
                return nullptr;

            auto stream = std::make_unique<std::istringstream>(
                std::string(reinterpret_cast<const char*>(bytes->data()), bytes->size()),
                std::ios::in | std::ios::binary);
            return stream;
        }
    }

    return nullptr;
}

std::optional<WritableMount>
ResolveWritableMount(PathDomain domain, const std::filesystem::path& savedDir, const std::filesystem::path& cacheDir)
{
    switch (domain)
    {
        case PathDomain::Saved:
            return WritableMount{domain, savedDir};
        case PathDomain::Cache:
            return WritableMount{domain, cacheDir};
        case PathDomain::Project:
        case PathDomain::Engine:
        case PathDomain::DLC:
        case PathDomain::Mod:
            return std::nullopt;
    }

    return std::nullopt;
}
} // namespace Resource
