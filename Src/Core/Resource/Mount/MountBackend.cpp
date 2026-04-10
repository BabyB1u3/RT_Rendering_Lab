#include "Core/Resource/Mount/MountBackend.h"

#include "Core/Resource/Package/PakArchive.h"
#include "Core/Resource/Path/PathParser.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iterator>

namespace
{
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
} // namespace

namespace Resource
{
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
                if (std::filesystem::exists(candidate) && PakEntryExists(candidate, ".rtr/catalog.json", &errorMessage))
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
                                             const VirtualPath &mountPath,
                                             const MountPriority priority,
                                             const MountBackendKind backend,
                                             const std::filesystem::path &mountRoot,
                                             const std::filesystem::path &materializedRoot) {
            if (mountRoot.empty())
                return;

            mounts.push_back(ReadableMount{
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
                                          const VirtualPath &mountPath,
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
                                    MountPriority::Overlay,
                                    MountBackendKind::Directory,
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
                                        MountPriority::Packaged,
                                        MountBackendKind::PakArchive,
                                        packagedArchive,
                                        materializedRoot);
                }
                if (hasCookedCatalog)
                {
                    appendReadableMount("Cooked:" + sourceKey,
                                        sourceKey,
                                        mountPath,
                                        MountPriority::Cooked,
                                        MountBackendKind::Directory,
                                        cookedRoot,
                                        {});
                }
                if (hasSourceRoot)
                {
                    appendReadableMount(sourceKey,
                                        sourceKey,
                                        mountPath,
                                        MountPriority::Source,
                                        MountBackendKind::Directory,
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
                                        MountPriority::Cooked,
                                        MountBackendKind::Directory,
                                        cookedRoot,
                                        {});
                }
                if (hasSourceRoot)
                {
                    appendReadableMount(sourceKey,
                                        sourceKey,
                                        mountPath,
                                        MountPriority::Source,
                                        MountBackendKind::Directory,
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
                                    MountPriority::Source,
                                    MountBackendKind::Directory,
                                    sourceRoot,
                                    {});
                return;
            }

            if (hasPackagedCatalog)
            {
                appendReadableMount("Packaged:" + sourceKey,
                                    sourceKey,
                                    mountPath,
                                    MountPriority::Packaged,
                                    MountBackendKind::PakArchive,
                                    packagedArchive,
                                    materializedRoot);
            }
            if (hasCookedCatalog)
            {
                appendReadableMount("Cooked:" + sourceKey,
                                    sourceKey,
                                    mountPath,
                                    MountPriority::Cooked,
                                    MountBackendKind::Directory,
                                    cookedRoot,
                                    {});
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

    std::optional<std::filesystem::path> ResolveReadableMountArtifact(const ReadableMount &mount,
                                                                      const ArtifactRecord &artifact,
                                                                      std::string *errorMessage)
    {
        switch (mount.backend)
        {
        case MountBackendKind::Directory:
            return mount.mountRoot / artifact.relativePath;
        case MountBackendKind::PakArchive:
            return MaterializePakEntry(mount.mountRoot, artifact.relativePath, mount.materializedRoot, errorMessage);
        }

        return std::nullopt;
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
            return std::nullopt;
        }

        return std::nullopt;
    }
} // namespace Resource
