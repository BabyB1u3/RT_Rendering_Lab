#pragma once

#include "Core/Resource/Catalog/ResourceCatalog.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Resource
{
    bool BuildSourceCatalogMap(const std::filesystem::path &mountRoot,
                               const VirtualPath &mountPath,
                               std::unordered_map<std::string, ResourceCatalogEntry> &entries,
                               std::string *errorMessage = nullptr);

    bool BuildSourceCatalogEntries(const std::filesystem::path &mountRoot,
                                   const VirtualPath &mountPath,
                                   std::vector<ResourceCatalogEntry> &entries,
                                   std::string *errorMessage = nullptr);

    bool WriteSourceCatalogJson(const std::filesystem::path &catalogPath,
                                const std::vector<ResourceCatalogEntry> &entries,
                                std::string *errorMessage = nullptr);

    bool IndexRepositorySourceCatalogs(const std::filesystem::path &rootPath,
                                       std::string_view projectContentDirName,
                                       std::string *errorMessage = nullptr);
} // namespace Resource
