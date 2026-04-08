#include "Core/Resource/Config/ConfigResolver.h"

#include "Core/Diagnostics/LogCategories.h"
#include "Core/Diagnostics/LogMacros.h"

namespace
{
    std::filesystem::path CopyConfigToSaved(std::string_view relativePath,
                                            const std::filesystem::path &savedPath,
                                            const std::filesystem::path &sourcePath,
                                            std::string_view sourceLabel)
    {
        std::filesystem::create_directories(savedPath.parent_path());
        std::error_code ec;
        std::filesystem::copy_file(sourcePath, savedPath, ec);
        if (ec)
        {
            LOG_WARN_CAT(LogCategory::FileSystem, "Failed to copy {} config '{}' to saved: {}",
                         sourceLabel, relativePath, ec.message());
            return sourcePath;
        }

        LOG_INFO_CAT(LogCategory::FileSystem, "Copied {} config to saved: {}", sourceLabel, savedPath.string());
        return savedPath;
    }
} // namespace

namespace Resource
{
    std::filesystem::path ResolveConfigPath(std::string_view relativePath,
                                            const std::filesystem::path &savedPath,
                                            const std::optional<std::filesystem::path> &projectDefaultPath,
                                            const std::optional<std::filesystem::path> &engineDefaultPath)
    {
        if (std::filesystem::exists(savedPath))
            return savedPath;

        if (projectDefaultPath.has_value() && std::filesystem::exists(*projectDefaultPath))
            return CopyConfigToSaved(relativePath, savedPath, *projectDefaultPath, "project");

        if (engineDefaultPath.has_value() && std::filesystem::exists(*engineDefaultPath))
            return CopyConfigToSaved(relativePath, savedPath, *engineDefaultPath, "engine");

        return {};
    }
} // namespace Resource
