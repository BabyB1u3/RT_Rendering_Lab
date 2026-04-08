#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace Resource
{
    bool CookRepositoryCatalogs(const std::filesystem::path &rootPath,
                                const std::filesystem::path &cookedRootPath,
                                std::string_view projectContentDirName,
                                std::string *errorMessage = nullptr);
} // namespace Resource
