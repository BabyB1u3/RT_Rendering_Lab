#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Resource
{
    inline constexpr std::string_view kCookedTextureArtifactExtension = ".rtrtex";
    inline constexpr std::string_view kCookedTextureArtifactFormat = "rtrtex";

    enum class CookedTexturePixelFormat : uint32_t
    {
        Unknown = 0,
        RGBA8_UNorm = 1,
    };

    struct CookedTextureData
    {
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t channelCount = 0;
        CookedTexturePixelFormat pixelFormat = CookedTexturePixelFormat::Unknown;
        std::vector<uint8_t> pixelData;
    };

    std::optional<CookedTextureData> LoadCookedTexture(const std::filesystem::path &artifactPath,
                                                       std::string *errorMessage = nullptr);

    bool CookRepositoryCatalogs(const std::filesystem::path &rootPath,
                                const std::filesystem::path &cookedRootPath,
                                std::string_view projectContentDirName,
                                std::string *errorMessage = nullptr);
} // namespace Resource
