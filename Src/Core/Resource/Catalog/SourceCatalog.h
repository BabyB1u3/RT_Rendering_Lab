#pragma once

#include "Core/Resource/Catalog/ResourceCatalog.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Resource
{
bool BuildSourceCatalogEntries(const std::filesystem::path& mountRoot,
                               const VirtualPath& mountPath,
                               std::vector<ResourceCatalogEntry>& entries,
                               std::string* errorMessage = nullptr);
} // namespace Resource
