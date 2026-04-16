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
    Resource::VirtualPath m_MountPath;
    std::filesystem::path m_SourceRoot;
    std::filesystem::path m_CookedRoot;
};

struct BootstrapTextureHeader
{
    char m_Magic[8] = {'R', 'T', 'R', 'T', 'E', 'X', '0', '1'};
    uint32_t m_Version = 1;
    uint32_t m_Width = 0;
    uint32_t m_Height = 0;
    uint32_t m_ChannelCount = 4;
    uint32_t m_PixelFormat = 1; // RGBA8_UNORM bootstrap payload
    uint32_t m_DataSize = 0;
};

static_assert(sizeof(BootstrapTextureHeader) == 32);

struct CookedTextureHeaderV2
{
    char m_Magic[8] = {'R', 'T', 'R', 'T', 'E', 'X', '0', '1'};
    uint32_t m_Version = 2;
    uint32_t m_Width = 0;
    uint32_t m_Height = 0;
    uint32_t m_ChannelCount = 4;
    uint32_t m_PixelFormat = 1; // RGBA8_UNORM bootstrap payload
    uint32_t m_MipLevelCount = 1;
    uint32_t m_RowPitch = 0;
    uint32_t m_DataOffset = 0;
    uint32_t m_DataSize = 0;
};

static_assert(sizeof(CookedTextureHeaderV2) == 44);

constexpr std::array<char, 8> k_BootstrapTextureMagic{'R', 'T', 'R', 'T', 'E', 'X', '0', '1'};
constexpr uint32_t k_BootstrapTextureVersion = 1;
constexpr uint32_t k_CookedTextureVersion = 2;

bool HasExpectedMagic(const BootstrapTextureHeader& header)
{
    return std::equal(k_BootstrapTextureMagic.begin(), k_BootstrapTextureMagic.end(), header.m_Magic);
}

bool HasExpectedMagic(const CookedTextureHeaderV2& header)
{
    return std::equal(k_BootstrapTextureMagic.begin(), k_BootstrapTextureMagic.end(), header.m_Magic);
}

std::optional<Resource::CookedTexturePixelFormat> ParseCookedTexturePixelFormat(uint32_t pixelFormat)
{
    switch (pixelFormat)
    {
        case static_cast<uint32_t>(Resource::CookedTexturePixelFormat::RGBA8_UNORM):
            return Resource::CookedTexturePixelFormat::RGBA8_UNORM;
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

    if (baseHeader.m_Version == k_BootstrapTextureVersion)
    {
        const auto pixelFormat = ParseCookedTexturePixelFormat(baseHeader.m_PixelFormat);
        if (!pixelFormat.has_value())
        {
            if (errorMessage != nullptr)
                *errorMessage = "unsupported cooked texture pixel format in artifact: " + artifactPath.string();
            return false;
        }

        if (baseHeader.m_Width == 0 || baseHeader.m_Height == 0 || baseHeader.m_ChannelCount == 0)
        {
            if (errorMessage != nullptr)
                *errorMessage = "cooked texture artifact has invalid dimensions: " + artifactPath.string();
            return false;
        }

        const uint64_t expectedDataSize = static_cast<uint64_t>(baseHeader.m_Width) *
                                          static_cast<uint64_t>(baseHeader.m_Height) *
                                          static_cast<uint64_t>(baseHeader.m_ChannelCount);
        if (expectedDataSize != baseHeader.m_DataSize)
        {
            if (errorMessage != nullptr)
                *errorMessage = "cooked texture artifact data size does not match metadata: " + artifactPath.string();
            return false;
        }

        if (*pixelFormat == Resource::CookedTexturePixelFormat::RGBA8_UNORM && baseHeader.m_ChannelCount != 4)
        {
            if (errorMessage != nullptr)
                *errorMessage = "cooked RGBA8 texture artifact must contain 4 channels: " + artifactPath.string();
            return false;
        }

        metadata.m_Width = baseHeader.m_Width;
        metadata.m_Height = baseHeader.m_Height;
        metadata.m_ChannelCount = baseHeader.m_ChannelCount;
        metadata.m_MipLevelCount = 1;
        metadata.m_RowPitch = baseHeader.m_Width * baseHeader.m_ChannelCount;
        metadata.m_DataSize = baseHeader.m_DataSize;
        metadata.m_PixelFormat = *pixelFormat;
        dataOffset = sizeof(BootstrapTextureHeader);
        return true;
    }

    if (baseHeader.m_Version != k_CookedTextureVersion)
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

    const auto pixelFormat = ParseCookedTexturePixelFormat(header.m_PixelFormat);
    if (!pixelFormat.has_value())
    {
        if (errorMessage != nullptr)
            *errorMessage = "unsupported cooked texture pixel format in artifact: " + artifactPath.string();
        return false;
    }

    if (header.m_Width == 0 || header.m_Height == 0 || header.m_ChannelCount == 0 || header.m_MipLevelCount == 0)
    {
        if (errorMessage != nullptr)
            *errorMessage = "cooked texture artifact has invalid dimensions: " + artifactPath.string();
        return false;
    }

    if (*pixelFormat == Resource::CookedTexturePixelFormat::RGBA8_UNORM && header.m_ChannelCount != 4)
    {
        if (errorMessage != nullptr)
            *errorMessage = "cooked RGBA8 texture artifact must contain 4 channels: " + artifactPath.string();
        return false;
    }

    const uint64_t minimumRowPitch =
        static_cast<uint64_t>(header.m_Width) * static_cast<uint64_t>(header.m_ChannelCount);
    if (header.m_RowPitch < minimumRowPitch)
    {
        if (errorMessage != nullptr)
            *errorMessage =
                "cooked texture artifact row pitch is smaller than the texel width: " + artifactPath.string();
        return false;
    }

    if (header.m_DataOffset < sizeof(CookedTextureHeaderV2))
    {
        if (errorMessage != nullptr)
            *errorMessage = "cooked texture artifact data offset is invalid: " + artifactPath.string();
        return false;
    }

    const uint64_t minimumDataSize = static_cast<uint64_t>(header.m_RowPitch) * static_cast<uint64_t>(header.m_Height);
    if (header.m_DataSize < minimumDataSize)
    {
        if (errorMessage != nullptr)
            *errorMessage = "cooked texture artifact data size does not match metadata: " + artifactPath.string();
        return false;
    }

    metadata.m_Width = header.m_Width;
    metadata.m_Height = header.m_Height;
    metadata.m_ChannelCount = header.m_ChannelCount;
    metadata.m_MipLevelCount = header.m_MipLevelCount;
    metadata.m_RowPitch = header.m_RowPitch;
    metadata.m_DataSize = header.m_DataSize;
    metadata.m_PixelFormat = *pixelFormat;
    dataOffset = header.m_DataOffset;
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
        entryJson["logicalPath"] = entry.m_LogicalPath;
        entryJson["artifacts"] = Json::array();

        for (const auto& artifact : entry.m_Artifacts)
        {
            Json artifactJson;
            artifactJson["relativePath"] = artifact.m_RelativePath;
            artifactJson["format"] = artifact.m_Format;
            artifactJson["platformTag"] = artifact.m_PlatformTag;
            artifactJson["backendTag"] = artifact.m_BackendTag;
            artifactJson["profileTag"] = artifact.m_ProfileTag;
            if (artifact.m_ContentHash != 0)
                artifactJson["contentHash"] = artifact.m_ContentHash;
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
    const auto sourcePath = sourceMountRoot / sourceArtifact.m_RelativePath;
    const auto parsedLogicalPath = Resource::ParseVirtualPath(sourceEntry.m_LogicalPath);
    if (!parsedLogicalPath.has_value())
    {
        if (errorMessage != nullptr)
            *errorMessage = "invalid logical path in source catalog: " + sourceEntry.m_LogicalPath;
        return false;
    }

    std::filesystem::path cookedRelativePath = sourceArtifact.m_RelativePath;
    std::string cookedFormat = sourceArtifact.m_Format;

    const std::string lowerFormat = [&]()
    {
        std::string value = sourceArtifact.m_Format;
        std::transform(value.begin(),
                       value.end(),
                       value.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }();

    const bool isTextureArtifact = parsedLogicalPath->m_RelativePath.rfind("Textures/", 0) == 0 &&
                                   (lowerFormat == "jpg" || lowerFormat == "jpeg" || lowerFormat == "png");
    if (isTextureArtifact)
    {
        cookedRelativePath = std::filesystem::path(parsedLogicalPath->m_RelativePath +
                                                   std::string(Resource::k_CookedTextureArtifactExtension));
        cookedFormat = std::string(Resource::k_CookedTextureArtifactFormat);
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
            .m_Version = k_CookedTextureVersion,
            .m_Width = static_cast<uint32_t>(width),
            .m_Height = static_cast<uint32_t>(height),
            .m_ChannelCount = 4,
            .m_PixelFormat = static_cast<uint32_t>(Resource::CookedTexturePixelFormat::RGBA8_UNORM),
            .m_MipLevelCount = 1,
            .m_RowPitch = rowPitch,
            .m_DataOffset = sizeof(CookedTextureHeaderV2),
            .m_DataSize = dataSize,
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
    cookedArtifact.m_RelativePath = cookedRelativePath.generic_string();
    cookedArtifact.m_Format = cookedFormat;
    cookedArtifact.m_ProfileTag = "cooked";
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
    texture.m_Width = metadata.m_Width;
    texture.m_Height = metadata.m_Height;
    texture.m_ChannelCount = metadata.m_ChannelCount;
    texture.m_MipLevelCount = metadata.m_MipLevelCount;
    texture.m_RowPitch = metadata.m_RowPitch;
    texture.m_PixelFormat = metadata.m_PixelFormat;
    texture.m_PixelData.resize(metadata.m_DataSize);

    in.seekg(dataOffset, std::ios::beg);
    if (!in.good())
    {
        if (errorMessage != nullptr)
            *errorMessage = "failed to seek to cooked texture payload: " + artifactPath.string();
        return std::nullopt;
    }

    in.read(reinterpret_cast<char*>(texture.m_PixelData.data()),
            static_cast<std::streamsize>(texture.m_PixelData.size()));
    if (in.gcount() != static_cast<std::streamsize>(texture.m_PixelData.size()))
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
        if (!BuildSourceCatalogEntries(mount.m_SourceRoot, mount.m_MountPath, sourceEntries, errorMessage))
            return false;

        std::vector<ResourceCatalogEntry> cookedEntries;
        cookedEntries.reserve(sourceEntries.size());

        for (const auto& sourceEntry : sourceEntries)
        {
            if (sourceEntry.m_Artifacts.empty())
            {
                if (errorMessage != nullptr)
                    *errorMessage = "source catalog entry has no artifacts: " + sourceEntry.m_LogicalPath;
                return false;
            }

            ResourceCatalogEntry cookedEntry;
            cookedEntry.m_LogicalPath = sourceEntry.m_LogicalPath;
            if (cookedEntry.m_LogicalPath.empty() || cookedEntry.m_LogicalPath != sourceEntry.m_LogicalPath)
            {
                if (errorMessage != nullptr)
                    *errorMessage =
                        "cooked catalog entry must preserve source logical path: " + sourceEntry.m_LogicalPath;
                return false;
            }

            for (const auto& sourceArtifact : sourceEntry.m_Artifacts)
            {
                ArtifactRecord cookedArtifact;
                if (!CopySourceArtifact(mount.m_SourceRoot,
                                        sourceEntry,
                                        sourceArtifact,
                                        mount.m_CookedRoot,
                                        cookedArtifact,
                                        errorMessage))
                    return false;

                cookedEntry.m_Artifacts.push_back(std::move(cookedArtifact));
            }

            cookedEntries.push_back(std::move(cookedEntry));
        }

        if (!WriteCookedCatalogJson(mount.m_CookedRoot / ".rtr" / "catalog.json", cookedEntries, errorMessage))
            return false;
    }

    return true;
}
} // namespace Resource
