#pragma once

#include "Core/Resource/Path/PathTypes.h"

#include <optional>
#include <string_view>

namespace Resource
{
bool IsVirtualPath(std::string_view path);
std::optional<VirtualPath> ParseVirtualPath(std::string_view path);
} // namespace Resource
