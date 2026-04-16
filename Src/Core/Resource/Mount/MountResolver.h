#pragma once

#include "Core/Resource/Path/PathTypes.h"

#include <filesystem>
#include <optional>
#include <string_view>

namespace Resource
{
struct WritableRoots
{
    std::filesystem::path m_SavedDir;
    std::filesystem::path m_CacheDir;
};

WritableRoots ResolveWritableRoots(const std::filesystem::path& rootPath, std::string_view appName);
std::filesystem::path GetPhysicalRelativePath(const VirtualPath& virtualPath);
} // namespace Resource
