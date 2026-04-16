#pragma once

#include <optional>
#include <string>

namespace Resource
{
enum class PathDomain
{
    Project,
    Engine,
    DLC,
    Mod,
    Saved,
    Cache,
};

struct VirtualPath
{
    PathDomain m_Domain;
    std::optional<std::string> m_MountName;
    std::string m_RelativePath;
};
} // namespace Resource
