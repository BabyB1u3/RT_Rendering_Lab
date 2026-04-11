#include "Core/Resource/Mount/MountBackend.h"

#include "Core/Resource/IO/PhysicalIO.h"
#include "Core/Resource/Package/PakArchive.h"
#include "Core/Resource/Path/PathParser.h"
#include "Core/Util/CommandLine.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>

namespace Resource
{
    namespace
    {
        std::optional<std::filesystem::path> GetDevelopmentOverridePath(std::string_view optionName, const char *envVarName)
        {
#ifdef RTRLAB_CONFIG_RELEASE
            (void)envVarName;
            if (!Util::ProcessHasOption("dev-mode"))
                return std::nullopt;

            if (const auto cliOverride = Util::GetProcessOptionValue(optionName); cliOverride.has_value() && !cliOverride->empty())
                return std::filesystem::path(std::string(*cliOverride));

            return std::nullopt;
#else
            if (const auto cliOverride = Util::GetProcessOptionValue(optionName); cliOverride.has_value() && !cliOverride->empty())
                return std::filesystem::path(std::string(*cliOverride));

            if (const char *overrideValue = std::getenv(envVarName))
            {
                const std::filesystem::path overrideRoot = overrideValue;
                if (!overrideRoot.empty())
                    return overrideRoot;
            }

            return std::nullopt;
#endif
        }

        std::filesystem::path GetPackagedArchiveSearchPath(const std::filesystem::path &packagedRoot,
                                                           std::string_view currentProfile,
                                                           const std::filesystem::path &legacyArchiveRelativePath)
        {
            if (currentProfile == "shipping" || currentProfile == "packaged")
                return GetGamePackagedArchivePath(packagedRoot);

            return packagedRoot / legacyArchiveRelativePath;
        }

        std::vector<std::filesystem::path> CollectPakArchives(const std::filesystem::path &directory)
        {
            std::vector<std::filesystem::path> archives;
            std::error_code ec;
            if (!std::filesystem::exists(directory, ec) || ec || !std::filesystem::is_directory(directory, ec))
                return archives;

            for (const auto &entry : std::filesystem::directory_iterator(directory, ec))
            {
                if (ec)
                    break;

                if (!entry.is_regular_file())
                    continue;

                if (entry.path().extension() == kPakArchiveExtension)
                    archives.push_back(entry.path());
            }

            std::sort(archives.begin(), archives.end(), [](const auto &lhs, const auto &rhs) {
                return lhs.generic_string() < rhs.generic_string();
            });
            return archives;
        }
    } // namespace

    std::vector<ReadableMount> DiscoverReadableMountBackends(const std::filesystem::path &rootPath,
                                                             const std::filesystem::path &engineDir,
                                                             const std::filesystem::path &cacheDir,
                                                             std::string_view projectContentDirName,
                                                             std::string_view currentProfile)
    {
        std::vector<ReadableMount> mounts;
        const bool preferCookedArtifacts = currentProfile == "cooked";
        const bool preferPackagedArtifacts = currentProfile == "packaged" || currentProfile == "shipping";
        std::vector<std::filesystem::path> cookedRootSearchOrder;
        std::vector<std::filesystem::path> packagedRootSearchOrder;

        if (const auto cookedOverride = GetDevelopmentOverridePath("cooked-root", "RTRLAB_COOKED_ROOT"))
            cookedRootSearchOrder.push_back(*cookedOverride);
        if (const auto packagedOverride = GetDevelopmentOverridePath("package-root", "RTRLAB_PACKAGE_ROOT"))
            packagedRootSearchOrder.push_back(*packagedOverride);

        if (currentProfile == "shipping")
            packagedRootSearchOrder.push_back(rootPath);
        cookedRootSearchOrder.push_back(cacheDir / "Cooked");
        cookedRootSearchOrder.push_back(rootPath / "build" / "Cooked");
        packagedRootSearchOrder.push_back(cacheDir / "Packaged");
        packagedRootSearchOrder.push_back(rootPath / "build" / "Packaged");

        auto findCookedMountRoot = [&](const std::filesystem::path &mountRelativePath) -> std::filesystem::path
        {
            for (const auto &cookedRoot : cookedRootSearchOrder)
            {
                const auto candidate = cookedRoot / mountRelativePath;
                if (std::filesystem::exists(candidate / ".rtr" / "catalog.json"))
                    return candidate;
            }

            return {};
        };

        auto findPackagedMountArchive = [&](const std::filesystem::path &archiveRelativePath) -> std::filesystem::path
        {
            for (const auto &packagedRoot : packagedRootSearchOrder)
            {
                const auto candidate = GetPackagedArchiveSearchPath(packagedRoot, currentProfile, archiveRelativePath);
                std::string errorMessage;
                if (std::filesystem::exists(candidate) && PakEntryExists(candidate, ".rtr/catalog.json", &errorMessage))
                    return candidate;
            }

            return {};
        };

        const auto appendReadableMount = [&](std::string cacheKey,
                                             const std::string &sourceKey,
                                             const VirtualPath &mountPath,
                                             const MountPriority priority,
                                             const MountBackendKind backend,
                                             const std::filesystem::path &mountRoot)
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
                                                   const std::string &archiveName,
                                                   const MountPriority priority,
                                                   const std::filesystem::path &archivePath)
        {
            appendReadableMount(
                std::string(layerName) + ":" + archiveName + ":Project",
                std::string(layerName) + ":" + archiveName + ":Project",
                VirtualPath{PathDomain::Project, std::nullopt, {}},
                priority,
                MountBackendKind::PakArchive,
                archivePath);
            appendReadableMount(
                std::string(layerName) + ":" + archiveName + ":Engine",
                std::string(layerName) + ":" + archiveName + ":Engine",
                VirtualPath{PathDomain::Engine, std::nullopt, {}},
                priority,
                MountBackendKind::PakArchive,
                archivePath);
        };

        const auto appendNamespacedArchiveMount = [&](std::string_view layerName,
                                                      const std::string &archiveName,
                                                      const PathDomain domain,
                                                      const MountPriority priority,
                                                      const std::filesystem::path &archivePath)
        {
            appendReadableMount(
                std::string(layerName) + ":" + archiveName,
                std::string(layerName) + ":" + archiveName,
                VirtualPath{domain, archiveName, {}},
                priority,
                MountBackendKind::PakArchive,
                archivePath);
        };

        const auto addReadableMount = [&](const std::string &sourceKey,
                                          const VirtualPath &mountPath,
                                          const std::filesystem::path &sourceRoot,
                                          const std::filesystem::path &cookedMountRelativePath,
                                          const std::filesystem::path &packagedArchiveRelativePath)
        {
            const auto cookedRoot = findCookedMountRoot(cookedMountRelativePath);
            const auto packagedArchive = findPackagedMountArchive(packagedArchiveRelativePath);
            const bool hasCookedCatalog = !cookedRoot.empty();
            const bool hasPackagedCatalog = !packagedArchive.empty();
            const bool hasSourceRoot = std::filesystem::exists(sourceRoot);

            if (preferPackagedArtifacts)
            {
                if (hasPackagedCatalog)
                {
                    appendReadableMount("Packaged:" + sourceKey,
                                        sourceKey,
                                        mountPath,
                                        MountPriority::Packaged,
                                        MountBackendKind::PakArchive,
                                        packagedArchive);
                }
                if (hasCookedCatalog)
                {
                    appendReadableMount("Cooked:" + sourceKey,
                                        sourceKey,
                                        mountPath,
                                        MountPriority::Cooked,
                                        MountBackendKind::Directory,
                                        cookedRoot);
                }
                if (hasSourceRoot)
                {
                    appendReadableMount(sourceKey,
                                        sourceKey,
                                        mountPath,
                                        MountPriority::Source,
                                        MountBackendKind::Directory,
                                        sourceRoot);
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
                                        MountPriority::Cooked,
                                        MountBackendKind::Directory,
                                        cookedRoot);
                }
                if (hasSourceRoot)
                {
                    appendReadableMount(sourceKey,
                                        sourceKey,
                                        mountPath,
                                        MountPriority::Source,
                                        MountBackendKind::Directory,
                                        sourceRoot);
                }
                return;
            }

            if (hasSourceRoot)
            {
                appendReadableMount(sourceKey,
                                    sourceKey,
                                    mountPath,
                                    MountPriority::Source,
                                    MountBackendKind::Directory,
                                    sourceRoot);
                return;
            }

            if (hasPackagedCatalog)
            {
                appendReadableMount("Packaged:" + sourceKey,
                                    sourceKey,
                                    mountPath,
                                    MountPriority::Packaged,
                                    MountBackendKind::PakArchive,
                                    packagedArchive);
            }
            if (hasCookedCatalog)
            {
                appendReadableMount("Cooked:" + sourceKey,
                                    sourceKey,
                                    mountPath,
                                    MountPriority::Cooked,
                                    MountBackendKind::Directory,
                                    cookedRoot);
            }
        };

        addReadableMount(
            "Project",
            VirtualPath{PathDomain::Project, std::nullopt, {}},
            rootPath / projectContentDirName,
            "Project",
            std::filesystem::path(std::string("Project") + std::string(kPakArchiveExtension)));

        addReadableMount(
            "Engine",
            VirtualPath{PathDomain::Engine, std::nullopt, {}},
            engineDir,
            "Engine",
            std::filesystem::path(std::string("Engine") + std::string(kPakArchiveExtension)));

        if (preferPackagedArtifacts)
        {
            const auto appendOverlayArchives = [&](const std::filesystem::path &directory,
                                                   std::string_view layerName,
                                                   const MountPriority priority,
                                                   const std::optional<PathDomain> namespacedDomain)
            {
                for (const auto &archivePath : CollectPakArchives(directory))
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
        }

        return mounts;
    }

    bool MountHasCatalog(const ReadableMount &mount)
    {
        switch (mount.backend)
        {
        case MountBackendKind::Directory:
            return std::filesystem::exists(mount.mountRoot / ".rtr" / "catalog.json");
        case MountBackendKind::PakArchive:
        {
            std::string errorMessage;
            return PakEntryExists(mount.mountRoot, ".rtr/catalog.json", &errorMessage);
        }
        }

        return false;
    }

    bool ReadMountCatalogBytes(const ReadableMount &mount,
                               std::vector<uint8_t> &bytes,
                               std::string *errorMessage)
    {
        switch (mount.backend)
        {
        case MountBackendKind::Directory:
        {
            const auto catalogPath = mount.mountRoot / ".rtr" / "catalog.json";
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
            const auto pakBytes = ReadPakEntry(mount.mountRoot, ".rtr/catalog.json", errorMessage);
            if (!pakBytes.has_value())
                return false;

            bytes = std::move(*pakBytes);
            return true;
        }
        }

        return false;
    }

    std::optional<ResolvedReadableArtifact> ResolveReadableMountArtifact(const ReadableMount &mount,
                                                                         const ArtifactRecord &artifact,
                                                                         std::string *errorMessage)
    {
        (void)errorMessage;

        switch (mount.backend)
        {
        case MountBackendKind::Directory:
            return ResolvedReadableArtifact{
                .backend = mount.backend,
                .mountRoot = mount.mountRoot,
                .relativePath = artifact.relativePath,
            };
        case MountBackendKind::PakArchive:
            return ResolvedReadableArtifact{
                .backend = mount.backend,
                .mountRoot = mount.mountRoot,
                .relativePath = artifact.relativePath,
            };
        }

        return std::nullopt;
    }

    std::optional<std::string> ReadReadableArtifactText(const ResolvedReadableArtifact &artifact,
                                                        std::string *errorMessage)
    {
        switch (artifact.backend)
        {
        case MountBackendKind::Directory:
        {
            const auto text = ReadTextFile(artifact.mountRoot / artifact.relativePath);
            if (!text.has_value() && errorMessage != nullptr)
                *errorMessage = "failed to read text file: " + (artifact.mountRoot / artifact.relativePath).string();
            return text;
        }
        case MountBackendKind::PakArchive:
        {
            const auto bytes = ReadPakEntry(artifact.mountRoot, artifact.relativePath, errorMessage);
            if (!bytes.has_value())
                return std::nullopt;

            return std::string(bytes->begin(), bytes->end());
        }
        }

        return std::nullopt;
    }

    std::optional<std::vector<uint8_t>> ReadReadableArtifactBinary(const ResolvedReadableArtifact &artifact,
                                                                   std::string *errorMessage)
    {
        switch (artifact.backend)
        {
        case MountBackendKind::Directory:
        {
            const auto bytes = ReadBinaryFile(artifact.mountRoot / artifact.relativePath);
            if (!bytes.has_value() && errorMessage != nullptr)
                *errorMessage = "failed to read binary file: " + (artifact.mountRoot / artifact.relativePath).string();
            return bytes;
        }
        case MountBackendKind::PakArchive:
            return ReadPakEntry(artifact.mountRoot, artifact.relativePath, errorMessage);
        }

        return std::nullopt;
    }

    std::unique_ptr<std::istream> OpenReadableArtifactStream(const ResolvedReadableArtifact &artifact,
                                                             std::string *errorMessage)
    {
        switch (artifact.backend)
        {
        case MountBackendKind::Directory:
        {
            auto stream = std::make_unique<std::ifstream>(artifact.mountRoot / artifact.relativePath, std::ios::binary);
            if (!stream->is_open())
            {
                if (errorMessage != nullptr)
                    *errorMessage = "failed to open readable artifact stream: " + (artifact.mountRoot / artifact.relativePath).string();
                return nullptr;
            }

            return stream;
        }
        case MountBackendKind::PakArchive:
        {
            const auto bytes = ReadPakEntry(artifact.mountRoot, artifact.relativePath, errorMessage);
            if (!bytes.has_value())
                return nullptr;

            auto stream = std::make_unique<std::istringstream>(
                std::string(reinterpret_cast<const char *>(bytes->data()), bytes->size()),
                std::ios::in | std::ios::binary);
            return stream;
        }
        }

        return nullptr;
    }

    std::optional<WritableMount> ResolveWritableMount(PathDomain domain,
                                                      const std::filesystem::path &savedDir,
                                                      const std::filesystem::path &cacheDir)
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
