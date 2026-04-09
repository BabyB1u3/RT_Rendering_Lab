#pragma once

#include "Core/Resource/Cook/CookedCatalog.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Resource
{
    inline constexpr std::string_view kPakArchiveExtension = ".rtrpak";

    std::filesystem::path GetPackagedOutputRoot(const std::filesystem::path &rootPath, CookOutputLayout layout);

    bool BuildPakArchive(const std::filesystem::path &sourceRoot,
                         const std::filesystem::path &pakPath,
                         std::string *errorMessage = nullptr);

    std::optional<std::vector<uint8_t>> ReadPakEntry(const std::filesystem::path &pakPath,
                                                     const std::filesystem::path &relativePath,
                                                     std::string *errorMessage = nullptr);

    bool PakEntryExists(const std::filesystem::path &pakPath,
                        const std::filesystem::path &relativePath,
                        std::string *errorMessage = nullptr);

    std::optional<std::filesystem::path> MaterializePakEntry(const std::filesystem::path &pakPath,
                                                             const std::filesystem::path &relativePath,
                                                             const std::filesystem::path &outputRoot,
                                                             std::string *errorMessage = nullptr);

    bool PackageCookedRepositoryCatalogs(const std::filesystem::path &cookedRootPath,
                                         const std::filesystem::path &packagedRootPath,
                                         std::string *errorMessage = nullptr);
} // namespace Resource
