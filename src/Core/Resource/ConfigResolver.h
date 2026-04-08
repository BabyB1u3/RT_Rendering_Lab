#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

namespace Resource
{
    std::filesystem::path ResolveConfigPath(std::string_view relativePath,
                                            const std::filesystem::path &savedPath,
                                            const std::optional<std::filesystem::path> &projectDefaultPath,
                                            const std::optional<std::filesystem::path> &engineDefaultPath,
                                            const std::filesystem::path &legacyDefaultPath);
} // namespace Resource
