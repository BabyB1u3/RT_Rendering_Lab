#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Resource
{
inline constexpr std::string_view k_CookedTextureArtifactExtension = ".rtrtex";
inline constexpr std::string_view k_CookedTextureArtifactFormat = "rtrtex";

enum class CookedTexturePixelFormat : uint32_t
{
    Unknown = 0,
    RGBA8_UNORM = 1,
};

struct CookedTextureMetadata
{
    uint32_t m_Width = 0;
    uint32_t m_Height = 0;
    uint32_t m_ChannelCount = 0;
    uint32_t m_MipLevelCount = 0;
    uint32_t m_RowPitch = 0;
    uint32_t m_DataSize = 0;
    CookedTexturePixelFormat m_PixelFormat = CookedTexturePixelFormat::Unknown;
};

struct CookedTextureData
{
    uint32_t m_Width = 0;
    uint32_t m_Height = 0;
    uint32_t m_ChannelCount = 0;
    uint32_t m_MipLevelCount = 0;
    uint32_t m_RowPitch = 0;
    CookedTexturePixelFormat m_PixelFormat = CookedTexturePixelFormat::Unknown;
    std::vector<uint8_t> m_PixelData;
};

std::optional<CookedTextureMetadata> ReadCookedTextureMetadata(const std::filesystem::path& artifactPath,
                                                               std::string* errorMessage = nullptr);

std::optional<CookedTextureData> LoadCookedTexture(const std::filesystem::path& artifactPath,
                                                   std::string* errorMessage = nullptr);

bool CookRepositoryCatalogs(const std::filesystem::path& rootPath,
                            const std::filesystem::path& cookedRootPath,
                            std::string_view projectContentDirName,
                            std::string* errorMessage = nullptr);
} // namespace Resource
