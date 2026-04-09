#pragma once

#include <filesystem>
#include <string_view>

namespace Resource
{
    std::filesystem::path DiscoverRootPath(std::string_view assetDirName);
} // namespace Resource
