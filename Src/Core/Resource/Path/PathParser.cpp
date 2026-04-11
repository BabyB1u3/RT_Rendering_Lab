#include "Core/Resource/Path/PathParser.h"

#include <string>
#include <vector>

namespace
{
    std::string JoinSegments(const std::vector<std::string> &segments, size_t firstSegment)
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
            virtualPath.domain = PathDomain::Project;
            virtualPath.relativePath = JoinSegments(segments, 1);
            return virtualPath;
        }

        if (segments[0] == "Engine")
        {
            virtualPath.domain = PathDomain::Engine;
            virtualPath.relativePath = JoinSegments(segments, 1);
            return virtualPath;
        }

        if (segments[0] == "Saved")
        {
            virtualPath.domain = PathDomain::Saved;
            virtualPath.relativePath = JoinSegments(segments, 1);
            return virtualPath;
        }

        if (segments[0] == "Cache")
        {
            virtualPath.domain = PathDomain::Cache;
            virtualPath.relativePath = JoinSegments(segments, 1);
            return virtualPath;
        }

        if (segments[0] == "DLC")
        {
            if (segments.size() < 2)
                return std::nullopt;

            virtualPath.domain = PathDomain::DLC;
            virtualPath.mountName = segments[1];
            virtualPath.relativePath = JoinSegments(segments, 2);
            return virtualPath;
        }

        if (segments[0] == "Mod")
        {
            if (segments.size() < 2)
                return std::nullopt;

            virtualPath.domain = PathDomain::Mod;
            virtualPath.mountName = segments[1];
            virtualPath.relativePath = JoinSegments(segments, 2);
            return virtualPath;
        }

        return std::nullopt;
    }
} // namespace Resource
