#pragma once

#include <optional>
#include <string>

namespace Resource
{
    enum class PathDomain
    {
        Project,
        Engine,
        Saved,
        Cache,
    };

    struct VirtualPath
    {
        PathDomain domain;
        std::optional<std::string> mountName;
        std::string relativePath;
    };
} // namespace Resource
