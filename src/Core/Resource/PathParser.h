#pragma once

#include "Core/Resource/PathTypes.h"

#include <optional>
#include <string_view>

namespace Resource
{
    bool IsVirtualPath(std::string_view path);
    std::optional<VirtualPath> ParseVirtualPath(std::string_view path);
    bool IsCatalogBackedPath(std::string_view path);
    bool IsDocumentPath(std::string_view path);
} // namespace Resource
