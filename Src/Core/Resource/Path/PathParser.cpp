#include "Core/Resource/Path/PathParser.h"

#include <string>
#include <vector>

namespace
{
std::string JoinSegments(const std::vector<std::string>& segments, size_t firstSegment)
{
    std::string joined;
    for (size_t i = firstSegment; i < segments.size(); ++i)
    {
        if (!joined.empty())
            joined += '/';
        joined += segments[i];
    }

    return joined;
}
} // namespace

namespace Resource
{
bool IsVirtualPath(std::string_view path)
{
    return ParseVirtualPath(path).has_value();
}

std::optional<VirtualPath> ParseVirtualPath(std::string_view path)
{
    if (path.empty() || path.front() != '/')
        return std::nullopt;

    std::vector<std::string> segments;
    size_t index = 1;
    while (index < path.size())
    {
        while (index < path.size() && path[index] == '/')
            ++index;

        if (index >= path.size())
            break;

        const size_t start = index;
        while (index < path.size() && path[index] != '/')
        {
            if (path[index] == '\\')
                return std::nullopt;
            ++index;
        }

        const std::string segment(path.substr(start, index - start));
        if (segment.empty() || segment == "." || segment == "..")
            return std::nullopt;

        segments.push_back(segment);
    }

    if (segments.empty())
        return std::nullopt;

    VirtualPath virtualPath{};
    if (segments[0] == "Project")
    {
        virtualPath.m_Domain = PathDomain::Project;
        virtualPath.m_RelativePath = JoinSegments(segments, 1);
        return virtualPath;
    }

    if (segments[0] == "Engine")
    {
        virtualPath.m_Domain = PathDomain::Engine;
        virtualPath.m_RelativePath = JoinSegments(segments, 1);
        return virtualPath;
    }

    if (segments[0] == "Saved")
    {
        virtualPath.m_Domain = PathDomain::Saved;
        virtualPath.m_RelativePath = JoinSegments(segments, 1);
        return virtualPath;
    }

    if (segments[0] == "Cache")
    {
        virtualPath.m_Domain = PathDomain::Cache;
        virtualPath.m_RelativePath = JoinSegments(segments, 1);
        return virtualPath;
    }

    if (segments[0] == "DLC")
    {
        if (segments.size() < 2)
            return std::nullopt;

        virtualPath.m_Domain = PathDomain::DLC;
        virtualPath.m_MountName = segments[1];
        virtualPath.m_RelativePath = JoinSegments(segments, 2);
        return virtualPath;
    }

    if (segments[0] == "Mod")
    {
        if (segments.size() < 2)
            return std::nullopt;

        virtualPath.m_Domain = PathDomain::Mod;
        virtualPath.m_MountName = segments[1];
        virtualPath.m_RelativePath = JoinSegments(segments, 2);
        return virtualPath;
    }

    return std::nullopt;
}
} // namespace Resource
