#pragma once

#include "Core/Resource/Path/PathTypes.h"

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
    std::filesystem::path GetPhysicalRelativePath(const VirtualPath &virtualPath);
} // namespace Resource
