#pragma once

#include "Core/Resource/PathTypes.h"

#include <filesystem>
#include <optional>
#include <string_view>

namespace Resource
{
    struct WritableRoots
    {
        std::filesystem::path savedDir;
        std::filesystem::path cacheDir;
    };

    WritableRoots ResolveWritableRoots(const std::filesystem::path &rootPath, std::string_view appName);
    std::filesystem::path GetDomainBasePath(const std::filesystem::path &rootPath,
                                            const std::filesystem::path &engineDir,
                                            const std::filesystem::path &savedDir,
                                            const std::filesystem::path &cacheDir,
                                            const VirtualPath &virtualPath,
                                            std::string_view assetDirName);
    std::filesystem::path GetPhysicalRelativePath(const VirtualPath &virtualPath);
    std::optional<std::filesystem::path> ResolvePhysicalPath(const std::filesystem::path &rootPath,
                                                             const std::filesystem::path &engineDir,
                                                             const std::filesystem::path &savedDir,
                                                             const std::filesystem::path &cacheDir,
                                                             const VirtualPath &virtualPath,
                                                             std::string_view assetDirName);
} // namespace Resource
