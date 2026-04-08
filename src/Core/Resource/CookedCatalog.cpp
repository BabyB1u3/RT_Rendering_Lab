#include "Core/Resource/CookedCatalog.h"

#include "Core/Resource/PathParser.h"
#include "Core/Resource/ResourceCatalog.h"
#include "Core/Resource/SourceCatalog.h"

#include <stb_image.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <json.hpp>

namespace
{
    using Json = nlohmann::json;

    struct SourceMountDescriptor
    {
        Resource::VirtualPath mountPath;
        std::filesystem::path sourceRoot;
        std::filesystem::path cookedRoot;
    };

    struct BootstrapTextureHeader
    {
        char magic[8] = {'R', 'T', 'R', 'T', 'E', 'X', '0', '1'};
        uint32_t version = 1;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t channelCount = 4;
        uint32_t pixelFormat = 1; // RGBA8_UNORM bootstrap payload
        uint32_t dataSize = 0;
    };

    static_assert(sizeof(BootstrapTextureHeader) == 32);

    std::vector<SourceMountDescriptor> DiscoverReadableSourceMounts(const std::filesystem::path &rootPath,
                                                                    const std::filesystem::path &cookedRootPath,
                                                                    std::string_view projectContentDirName)
    {
        std::vector<SourceMountDescriptor> mounts;

        const auto projectRoot = rootPath / projectContentDirName;
        if (std::filesystem::exists(projectRoot))
        {
            mounts.push_back(SourceMountDescriptor{
                Resource::VirtualPath{Resource::PathDomain::Project, std::nullopt, {}},
                projectRoot,
                cookedRootPath / "Project",
            });
        }

        const auto engineRoot = rootPath / "EngineContent";
        if (std::filesystem::exists(engineRoot))
        {
            mounts.push_back(SourceMountDescriptor{
                Resource::VirtualPath{Resource::PathDomain::Engine, std::nullopt, {}},
                engineRoot,
                cookedRootPath / "Engine",
            });
        }

        const auto pluginsRoot = rootPath / "Plugins";
        if (std::filesystem::exists(pluginsRoot))
        {
            std::vector<std::filesystem::directory_entry> pluginDirs;
            for (const auto &entry : std::filesystem::directory_iterator(pluginsRoot))
            {
                if (entry.is_directory())
                    pluginDirs.push_back(entry);
            }

            std::sort(pluginDirs.begin(), pluginDirs.end(), [](const auto &lhs, const auto &rhs) {
                return lhs.path().filename().string() < rhs.path().filename().string();
            });

            for (const auto &pluginDir : pluginDirs)
            {
                const auto pluginName = pluginDir.path().filename().string();
                if (!Resource::IsValidPluginMountName(pluginName))
                    continue;

                const auto contentRoot = pluginDir.path() / "Content";
                if (!std::filesystem::exists(contentRoot))
                    continue;

                mounts.push_back(SourceMountDescriptor{
                    Resource::VirtualPath{Resource::PathDomain::Plugin, pluginName, {}},
                    contentRoot,
                    cookedRootPath / "Plugins" / pluginName,
                });
            }
        }

        return mounts;
    }

    bool LoadCatalogEntries(const std::filesystem::path &catalogPath,
                            std::vector<Resource::ResourceCatalogEntry> &entries,
                            std::string *errorMessage)
    {
        std::ifstream in(catalogPath, std::ios::in | std::ios::binary);
        if (!in.is_open())
        {
            if (errorMessage != nullptr)
                *errorMessage = "failed to open source catalog: " + catalogPath.string();
            return false;
        }

        Json rootJson;
        try
        {
            in >> rootJson;
        }
        catch (const std::exception &e)
        {
            if (errorMessage != nullptr)
                *errorMessage = "failed to parse source catalog '" + catalogPath.string() + "': " + e.what();
            return false;
        }

        const auto versionIt = rootJson.find("version");
        if (versionIt == rootJson.end() || !versionIt->is_number_integer() || versionIt->get<int>() != 1)
        {
            if (errorMessage != nullptr)
                *errorMessage = "unsupported source catalog version in " + catalogPath.string();
            return false;
        }

        const auto entriesIt = rootJson.find("entries");
        if (entriesIt == rootJson.end() || !entriesIt->is_array())
        {
            if (errorMessage != nullptr)
                *errorMessage = "source catalog missing entries array: " + catalogPath.string();
            return false;
        }

        entries.clear();

        for (const auto &entryJson : *entriesIt)
        {
            if (!entryJson.is_object())
            {
                if (errorMessage != nullptr)
                    *errorMessage = "source catalog contains a non-object entry: " + catalogPath.string();
                return false;
            }

            const auto logicalPathIt = entryJson.find("logicalPath");
            const auto sourceRelativePathIt = entryJson.find("sourceRelativePath");
            const auto artifactsIt = entryJson.find("artifacts");
            if (logicalPathIt == entryJson.end() || !logicalPathIt->is_string() ||
                sourceRelativePathIt == entryJson.end() || !sourceRelativePathIt->is_string() ||
                artifactsIt == entryJson.end() || !artifactsIt->is_array())
            {
                if (errorMessage != nullptr)
                    *errorMessage = "source catalog entry is missing required fields: " + catalogPath.string();
                return false;
            }

            Resource::ResourceCatalogEntry entry;
            entry.logicalPath = logicalPathIt->get<std::string>();
            entry.sourceRelativePath = sourceRelativePathIt->get<std::string>();

            for (const auto &artifactJson : *artifactsIt)
            {
                if (!artifactJson.is_object())
                {
                    if (errorMessage != nullptr)
                        *errorMessage = "source catalog entry contains a non-object artifact: " + catalogPath.string();
                    return false;
                }

                const auto relativePathIt = artifactJson.find("relativePath");
                if (relativePathIt == artifactJson.end() || !relativePathIt->is_string())
                {
                    if (errorMessage != nullptr)
                        *errorMessage = "source catalog artifact is missing relativePath: " + catalogPath.string();
                    return false;
                }

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

                entry.artifacts.push_back(std::move(artifact));
            }

            entries.push_back(std::move(entry));
        }

        return true;
    }

    bool CopySourceArtifact(const std::filesystem::path &sourceMountRoot,
                            const Resource::ResourceCatalogEntry &sourceEntry,
                            const Resource::ArtifactRecord &sourceArtifact,
                            const std::filesystem::path &cookedMountRoot,
                            Resource::ArtifactRecord &cookedArtifact,
                            std::string *errorMessage)
    {
        const auto sourcePath = sourceMountRoot / sourceArtifact.relativePath;
        const auto parsedLogicalPath = Resource::ParseVirtualPath(sourceEntry.logicalPath);
        if (!parsedLogicalPath.has_value())
        {
            if (errorMessage != nullptr)
                *errorMessage = "invalid logical path in source catalog: " + sourceEntry.logicalPath;
            return false;
        }

        std::filesystem::path cookedRelativePath = sourceArtifact.relativePath;
        std::string cookedFormat = sourceArtifact.format;

        const std::string lowerFormat = [&]() {
            std::string value = sourceArtifact.format;
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return value;
        }();

        const bool isTextureArtifact = parsedLogicalPath->relativePath.rfind("Textures/", 0) == 0 &&
                                       (lowerFormat == "jpg" || lowerFormat == "jpeg" || lowerFormat == "png");
        if (isTextureArtifact)
        {
            cookedRelativePath = std::filesystem::path(parsedLogicalPath->relativePath + ".ktx2");
            cookedFormat = "ktx2";
        }

        const auto cookedPath = cookedMountRoot / cookedRelativePath;

        std::error_code ec;
        std::filesystem::create_directories(cookedPath.parent_path(), ec);
        if (ec)
        {
            if (errorMessage != nullptr)
                *errorMessage = "failed to create cooked artifact directory: " + ec.message();
            return false;
        }

        if (isTextureArtifact)
        {
            int width = 0;
            int height = 0;
            int sourceChannels = 0;
            stbi_uc *pixels = stbi_load(sourcePath.string().c_str(), &width, &height, &sourceChannels, 4);
            if (pixels == nullptr)
            {
                if (errorMessage != nullptr)
                    *errorMessage = "failed to decode texture source '" + sourcePath.string() + "'";
                return false;
            }
            (void)sourceChannels;

            const uint32_t dataSize = static_cast<uint32_t>(width * height * 4);
            const BootstrapTextureHeader header{
                .version = 1,
                .width = static_cast<uint32_t>(width),
                .height = static_cast<uint32_t>(height),
                .channelCount = 4,
                .pixelFormat = 1,
                .dataSize = dataSize,
            };

            std::ofstream out(cookedPath, std::ios::binary | std::ios::trunc);
            if (!out.is_open())
            {
                stbi_image_free(pixels);
                if (errorMessage != nullptr)
                    *errorMessage = "failed to open cooked texture artifact for writing: " + cookedPath.string();
                return false;
            }

            out.write(reinterpret_cast<const char *>(&header), sizeof(header));
            out.write(reinterpret_cast<const char *>(pixels), dataSize);
            stbi_image_free(pixels);

            if (!out.good())
            {
                if (errorMessage != nullptr)
                    *errorMessage = "failed to write cooked texture artifact: " + cookedPath.string();
                return false;
            }
        }
        else
        {
            std::filesystem::copy_file(sourcePath, cookedPath, std::filesystem::copy_options::overwrite_existing, ec);
            if (ec)
            {
                if (errorMessage != nullptr)
                    *errorMessage = "failed to copy source artifact '" + sourcePath.string() + "': " + ec.message();
                return false;
            }
        }

        cookedArtifact = sourceArtifact;
        cookedArtifact.relativePath = cookedRelativePath.generic_string();
        cookedArtifact.format = cookedFormat;
        cookedArtifact.profileTag = "cooked";
        return true;
    }
} // namespace

namespace Resource
{
    bool CookRepositoryCatalogs(const std::filesystem::path &rootPath,
                                const std::filesystem::path &cookedRootPath,
                                std::string_view projectContentDirName,
                                std::string *errorMessage)
    {
        for (const auto &mount : DiscoverReadableSourceMounts(rootPath, cookedRootPath, projectContentDirName))
        {
            const auto sourceCatalogPath = mount.sourceRoot / ".rtr" / "catalog.json";
            if (!std::filesystem::exists(sourceCatalogPath))
            {
                if (errorMessage != nullptr)
                    *errorMessage = "missing source catalog for mount root: " + mount.sourceRoot.string();
                return false;
            }

            std::vector<ResourceCatalogEntry> sourceEntries;
            if (!LoadCatalogEntries(sourceCatalogPath, sourceEntries, errorMessage))
                return false;

            std::vector<ResourceCatalogEntry> cookedEntries;
            cookedEntries.reserve(sourceEntries.size());

            for (const auto &sourceEntry : sourceEntries)
            {
                if (sourceEntry.artifacts.empty())
                {
                    if (errorMessage != nullptr)
                        *errorMessage = "source catalog entry has no artifacts: " + sourceEntry.logicalPath;
                    return false;
                }

                ResourceCatalogEntry cookedEntry;
                cookedEntry.logicalPath = sourceEntry.logicalPath;
                cookedEntry.sourceRelativePath = sourceEntry.sourceRelativePath;

                for (const auto &sourceArtifact : sourceEntry.artifacts)
                {
                    ArtifactRecord cookedArtifact;
                    if (!CopySourceArtifact(
                            mount.sourceRoot, sourceEntry, sourceArtifact, mount.cookedRoot, cookedArtifact, errorMessage))
                        return false;

                    cookedEntry.artifacts.push_back(std::move(cookedArtifact));
                }

                cookedEntries.push_back(std::move(cookedEntry));
            }

            if (!WriteSourceCatalogJson(mount.cookedRoot / ".rtr" / "catalog.json", cookedEntries, errorMessage))
                return false;
        }

        return true;
    }
} // namespace Resource
