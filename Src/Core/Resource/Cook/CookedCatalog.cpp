#include "Core/Resource/Cook/CookedCatalog.h"

#include "Core/Resource/Catalog/ResourceCatalog.h"
#include "Core/Resource/Catalog/SourceCatalog.h"
#include "Core/Resource/Path/PathParser.h"

#include <stb_image.h>

#include <algorithm>
#include <array>
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

struct CookedTextureHeaderV2
{
    char magic[8] = {'R', 'T', 'R', 'T', 'E', 'X', '0', '1'};
    uint32_t version = 2;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t channelCount = 4;
    uint32_t pixelFormat = 1; // RGBA8_UNORM bootstrap payload
    uint32_t mipLevelCount = 1;
    uint32_t rowPitch = 0;
    uint32_t dataOffset = 0;
    uint32_t dataSize = 0;
};

static_assert(sizeof(CookedTextureHeaderV2) == 44);

constexpr std::array<char, 8> kBootstrapTextureMagic{'R', 'T', 'R', 'T', 'E', 'X', '0', '1'};
constexpr uint32_t kBootstrapTextureVersion = 1;
constexpr uint32_t kCookedTextureVersion = 2;

bool HasExpectedMagic(const BootstrapTextureHeader& header)
{
    return std::equal(kBootstrapTextureMagic.begin(), kBootstrapTextureMagic.end(), header.magic);
}

bool HasExpectedMagic(const CookedTextureHeaderV2& header)
{
    return std::equal(kBootstrapTextureMagic.begin(), kBootstrapTextureMagic.end(), header.magic);
}

std::optional<Resource::CookedTexturePixelFormat> ParseCookedTexturePixelFormat(uint32_t pixelFormat)
{
    switch (pixelFormat)
    {
        case static_cast<uint32_t>(Resource::CookedTexturePixelFormat::RGBA8_UNorm):
            return Resource::CookedTexturePixelFormat::RGBA8_UNorm;
        default:
            return std::nullopt;
    }
}

bool ReadCookedTextureMetadataFromStream(std::ifstream& in,
                                         const std::filesystem::path& artifactPath,
                                         Resource::CookedTextureMetadata& metadata,
                                         std::streamoff& dataOffset,
                                         std::string* errorMessage)
{
    BootstrapTextureHeader baseHeader{};
    in.read(reinterpret_cast<char*>(&baseHeader), sizeof(baseHeader));
    if (in.gcount() != static_cast<std::streamsize>(sizeof(baseHeader)))
    {
        if (errorMessage != nullptr)
            *errorMessage = "cooked texture artifact is truncated: " + artifactPath.string();
        return false;
    }

    if (!HasExpectedMagic(baseHeader))
    {
        if (errorMessage != nullptr)
            *errorMessage = "cooked texture artifact has invalid magic: " + artifactPath.string();
        return false;
    }

    if (baseHeader.version == kBootstrapTextureVersion)
    {
        const auto pixelFormat = ParseCookedTexturePixelFormat(baseHeader.pixelFormat);
        if (!pixelFormat.has_value())
        {
            if (errorMessage != nullptr)
                *errorMessage = "unsupported cooked texture pixel format in artifact: " + artifactPath.string();
            return false;
        }

        if (baseHeader.width == 0 || baseHeader.height == 0 || baseHeader.channelCount == 0)
        {
            if (errorMessage != nullptr)
                *errorMessage = "cooked texture artifact has invalid dimensions: " + artifactPath.string();
            return false;
        }

        const uint64_t expectedDataSize = static_cast<uint64_t>(baseHeader.width) *
                                          static_cast<uint64_t>(baseHeader.height) *
                                          static_cast<uint64_t>(baseHeader.channelCount);
        if (expectedDataSize != baseHeader.dataSize)
        {
            if (errorMessage != nullptr)
                *errorMessage = "cooked texture artifact data size does not match metadata: " + artifactPath.string();
            return false;
        }

        if (*pixelFormat == Resource::CookedTexturePixelFormat::RGBA8_UNorm && baseHeader.channelCount != 4)
        {
            if (errorMessage != nullptr)
                *errorMessage = "cooked RGBA8 texture artifact must contain 4 channels: " + artifactPath.string();
            return false;
        }

        metadata.width = baseHeader.width;
        metadata.height = baseHeader.height;
        metadata.channelCount = baseHeader.channelCount;
        metadata.mipLevelCount = 1;
        metadata.rowPitch = baseHeader.width * baseHeader.channelCount;
        metadata.dataSize = baseHeader.dataSize;
        metadata.pixelFormat = *pixelFormat;
        dataOffset = sizeof(BootstrapTextureHeader);
        return true;
    }

    if (baseHeader.version != kCookedTextureVersion)
    {
        if (errorMessage != nullptr)
            *errorMessage = "unsupported cooked texture version in artifact: " + artifactPath.string();
        return false;
    }

    in.seekg(0, std::ios::beg);
    CookedTextureHeaderV2 header{};
    in.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (in.gcount() != static_cast<std::streamsize>(sizeof(header)))
    {
        if (errorMessage != nullptr)
            *errorMessage = "cooked texture artifact is truncated: " + artifactPath.string();
        return false;
    }

    if (!HasExpectedMagic(header))
    {
        if (errorMessage != nullptr)
            *errorMessage = "cooked texture artifact has invalid magic: " + artifactPath.string();
        return false;
    }

    const auto pixelFormat = ParseCookedTexturePixelFormat(header.pixelFormat);
    if (!pixelFormat.has_value())
    {
        if (errorMessage != nullptr)
            *errorMessage = "unsupported cooked texture pixel format in artifact: " + artifactPath.string();
        return false;
    }

    if (header.width == 0 || header.height == 0 || header.channelCount == 0 || header.mipLevelCount == 0)
    {
        if (errorMessage != nullptr)
            *errorMessage = "cooked texture artifact has invalid dimensions: " + artifactPath.string();
        return false;
    }

    if (*pixelFormat == Resource::CookedTexturePixelFormat::RGBA8_UNorm && header.channelCount != 4)
    {
        if (errorMessage != nullptr)
            *errorMessage = "cooked RGBA8 texture artifact must contain 4 channels: " + artifactPath.string();
        return false;
    }

    const uint64_t minimumRowPitch = static_cast<uint64_t>(header.width) * static_cast<uint64_t>(header.channelCount);
    if (header.rowPitch < minimumRowPitch)
    {
        if (errorMessage != nullptr)
            *errorMessage =
                "cooked texture artifact row pitch is smaller than the texel width: " + artifactPath.string();
        return false;
    }

    if (header.dataOffset < sizeof(CookedTextureHeaderV2))
    {
        if (errorMessage != nullptr)
            *errorMessage = "cooked texture artifact data offset is invalid: " + artifactPath.string();
        return false;
    }

    const uint64_t minimumDataSize = static_cast<uint64_t>(header.rowPitch) * static_cast<uint64_t>(header.height);
    if (header.dataSize < minimumDataSize)
    {
        if (errorMessage != nullptr)
            *errorMessage = "cooked texture artifact data size does not match metadata: " + artifactPath.string();
        return false;
    }

    metadata.width = header.width;
    metadata.height = header.height;
    metadata.channelCount = header.channelCount;
    metadata.mipLevelCount = header.mipLevelCount;
    metadata.rowPitch = header.rowPitch;
    metadata.dataSize = header.dataSize;
    metadata.pixelFormat = *pixelFormat;
    dataOffset = header.dataOffset;
    return true;
}

std::vector<SourceMountDescriptor> DiscoverReadableSourceMounts(const std::filesystem::path& rootPath,
                                                                const std::filesystem::path& cookedRootPath,
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

    const auto engineRoot = rootPath / "Engine";
    if (std::filesystem::exists(engineRoot))
    {
        mounts.push_back(SourceMountDescriptor{
            Resource::VirtualPath{Resource::PathDomain::Engine, std::nullopt, {}},
            engineRoot,
            cookedRootPath / "Engine",
        });
    }

    return mounts;
}

bool WriteCookedCatalogJson(const std::filesystem::path& catalogPath,
                            const std::vector<Resource::ResourceCatalogEntry>& entries,
                            std::string* errorMessage)
{
    Json root;
    root["version"] = 2;
    root["kind"] = "cooked";
    root["entries"] = Json::array();

    for (const auto& entry : entries)
    {
        Json entryJson;
        entryJson["logicalPath"] = entry.logicalPath;
        entryJson["artifacts"] = Json::array();

        for (const auto& artifact : entry.artifacts)
        {
            Json artifactJson;
            artifactJson["relativePath"] = artifact.relativePath;
            artifactJson["format"] = artifact.format;
            artifactJson["platformTag"] = artifact.platformTag;
            artifactJson["backendTag"] = artifact.backendTag;
            artifactJson["profileTag"] = artifact.profileTag;
            if (artifact.contentHash != 0)
                artifactJson["contentHash"] = artifact.contentHash;
            entryJson["artifacts"].push_back(std::move(artifactJson));
        }

        root["entries"].push_back(std::move(entryJson));
    }

    std::error_code ec;
    std::filesystem::create_directories(catalogPath.parent_path(), ec);
    if (ec)
    {
        if (errorMessage != nullptr)
            *errorMessage = "failed to create cooked catalog directory: " + ec.message();
        return false;
    }

    std::ofstream out(catalogPath, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!out.is_open())
    {
        if (errorMessage != nullptr)
            *errorMessage = "failed to open cooked catalog for writing: " + catalogPath.string();
        return false;
    }

    out << root.dump(2) << '\n';
    if (!out.good())
    {
        if (errorMessage != nullptr)
            *errorMessage = "failed to write cooked catalog: " + catalogPath.string();
        return false;
    }

    return true;
}

bool CopySourceArtifact(const std::filesystem::path& sourceMountRoot,
                        const Resource::ResourceCatalogEntry& sourceEntry,
                        const Resource::ArtifactRecord& sourceArtifact,
                        const std::filesystem::path& cookedMountRoot,
                        Resource::ArtifactRecord& cookedArtifact,
                        std::string* errorMessage)
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

    const std::string lowerFormat = [&]()
    {
        std::string value = sourceArtifact.format;
        std::transform(value.begin(),
                       value.end(),
                       value.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }();

    const bool isTextureArtifact = parsedLogicalPath->relativePath.rfind("Textures/", 0) == 0 &&
                                   (lowerFormat == "jpg" || lowerFormat == "jpeg" || lowerFormat == "png");
    if (isTextureArtifact)
    {
        cookedRelativePath = std::filesystem::path(parsedLogicalPath->relativePath +
                                                   std::string(Resource::kCookedTextureArtifactExtension));
        cookedFormat = std::string(Resource::kCookedTextureArtifactFormat);
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
        stbi_uc* pixels = stbi_load(sourcePath.string().c_str(), &width, &height, &sourceChannels, 4);
        if (pixels == nullptr)
        {
            if (errorMessage != nullptr)
                *errorMessage = "failed to decode texture source '" + sourcePath.string() + "'";
            return false;
        }
        (void)sourceChannels;

        const uint32_t rowPitch = static_cast<uint32_t>(width * 4);
        const uint32_t dataSize = static_cast<uint32_t>(rowPitch * height);
        const CookedTextureHeaderV2 header{
            .version = kCookedTextureVersion,
            .width = static_cast<uint32_t>(width),
            .height = static_cast<uint32_t>(height),
            .channelCount = 4,
            .pixelFormat = static_cast<uint32_t>(Resource::CookedTexturePixelFormat::RGBA8_UNorm),
            .mipLevelCount = 1,
            .rowPitch = rowPitch,
            .dataOffset = sizeof(CookedTextureHeaderV2),
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

        out.write(reinterpret_cast<const char*>(&header), sizeof(header));
        out.write(reinterpret_cast<const char*>(pixels), dataSize);
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
std::optional<CookedTextureMetadata> ReadCookedTextureMetadata(const std::filesystem::path& artifactPath,
                                                               std::string* errorMessage)
{
    std::ifstream in(artifactPath, std::ios::binary);
    if (!in.is_open())
    {
        if (errorMessage != nullptr)
            *errorMessage = "failed to open cooked texture artifact: " + artifactPath.string();
        return std::nullopt;
    }

    CookedTextureMetadata metadata;
    std::streamoff dataOffset = 0;
    if (!ReadCookedTextureMetadataFromStream(in, artifactPath, metadata, dataOffset, errorMessage))
        return std::nullopt;

    return metadata;
}

std::optional<CookedTextureData> LoadCookedTexture(const std::filesystem::path& artifactPath, std::string* errorMessage)
{
    std::ifstream in(artifactPath, std::ios::binary);
    if (!in.is_open())
    {
        if (errorMessage != nullptr)
            *errorMessage = "failed to open cooked texture artifact: " + artifactPath.string();
        return std::nullopt;
    }

    CookedTextureMetadata metadata;
    std::streamoff dataOffset = 0;
    if (!ReadCookedTextureMetadataFromStream(in, artifactPath, metadata, dataOffset, errorMessage))
        return std::nullopt;

    CookedTextureData texture;
    texture.width = metadata.width;
    texture.height = metadata.height;
    texture.channelCount = metadata.channelCount;
    texture.mipLevelCount = metadata.mipLevelCount;
    texture.rowPitch = metadata.rowPitch;
    texture.pixelFormat = metadata.pixelFormat;
    texture.pixelData.resize(metadata.dataSize);

    in.seekg(dataOffset, std::ios::beg);
    if (!in.good())
    {
        if (errorMessage != nullptr)
            *errorMessage = "failed to seek to cooked texture payload: " + artifactPath.string();
        return std::nullopt;
    }

    in.read(reinterpret_cast<char*>(texture.pixelData.data()), static_cast<std::streamsize>(texture.pixelData.size()));
    if (in.gcount() != static_cast<std::streamsize>(texture.pixelData.size()))
    {
        if (errorMessage != nullptr)
            *errorMessage = "cooked texture artifact payload is truncated: " + artifactPath.string();
        return std::nullopt;
    }

    return texture;
}

bool CookRepositoryCatalogs(const std::filesystem::path& rootPath,
                            const std::filesystem::path& cookedRootPath,
                            std::string_view projectContentDirName,
                            std::string* errorMessage)
{
    for (const auto& mount : DiscoverReadableSourceMounts(rootPath, cookedRootPath, projectContentDirName))
    {
        std::vector<ResourceCatalogEntry> sourceEntries;
        if (!BuildSourceCatalogEntries(mount.sourceRoot, mount.mountPath, sourceEntries, errorMessage))
            return false;

        std::vector<ResourceCatalogEntry> cookedEntries;
        cookedEntries.reserve(sourceEntries.size());

        for (const auto& sourceEntry : sourceEntries)
        {
            if (sourceEntry.artifacts.empty())
            {
                if (errorMessage != nullptr)
                    *errorMessage = "source catalog entry has no artifacts: " + sourceEntry.logicalPath;
                return false;
            }

            ResourceCatalogEntry cookedEntry;
            cookedEntry.logicalPath = sourceEntry.logicalPath;
            if (cookedEntry.logicalPath.empty() || cookedEntry.logicalPath != sourceEntry.logicalPath)
            {
                if (errorMessage != nullptr)
                    *errorMessage =
                        "cooked catalog entry must preserve source logical path: " + sourceEntry.logicalPath;
                return false;
            }

            for (const auto& sourceArtifact : sourceEntry.artifacts)
            {
                ArtifactRecord cookedArtifact;
                if (!CopySourceArtifact(
                        mount.sourceRoot, sourceEntry, sourceArtifact, mount.cookedRoot, cookedArtifact, errorMessage))
                    return false;

                cookedEntry.artifacts.push_back(std::move(cookedArtifact));
            }

            cookedEntries.push_back(std::move(cookedEntry));
        }

        if (!WriteCookedCatalogJson(mount.cookedRoot / ".rtr" / "catalog.json", cookedEntries, errorMessage))
            return false;
    }

    return true;
}
} // namespace Resource
