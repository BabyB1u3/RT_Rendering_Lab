#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Resource
{
    inline constexpr std::string_view kPakArchiveExtension = ".rtrpak";

    std::filesystem::path GetGamePackagedArchivePath(const std::filesystem::path &packagedRootPath);

    bool BuildPakArchive(const std::filesystem::path &sourceRoot,
                         const std::filesystem::path &pakPath,
                         std::string *errorMessage = nullptr);

    std::optional<std::vector<uint8_t>> ReadPakEntry(const std::filesystem::path &pakPath,
                                                     const std::filesystem::path &relativePath,
                                                     std::string *errorMessage = nullptr);

    bool PakEntryExists(const std::filesystem::path &pakPath,
                        const std::filesystem::path &relativePath,
                        std::string *errorMessage = nullptr);

    bool PackageCookedRepositoryCatalogs(const std::filesystem::path &cookedRootPath,
                                         const std::filesystem::path &packagedRootPath,
                                         std::string *errorMessage = nullptr);
} // namespace Resource
