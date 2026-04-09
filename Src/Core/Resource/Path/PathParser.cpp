#include "Core/Resource/Path/PathParser.h"

#include <string>
#include <vector>

namespace
{
    bool IsAsciiAlpha(char c)
    {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    }

    bool IsAsciiDigit(char c)
    {
        return c >= '0' && c <= '9';
    }

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

    bool HasDocumentExtension(std::string_view relativePath)
    {
        const size_t slashPos = relativePath.find_last_of('/');
        const std::string_view fileName = slashPos == std::string_view::npos
                                              ? relativePath
                                              : relativePath.substr(slashPos + 1);
        if (fileName.empty())
            return false;

        const size_t dotPos = fileName.find_last_of('.');
        return dotPos != std::string_view::npos && dotPos > 0 && dotPos + 1 < fileName.size();
    }
} // namespace

namespace Resource
{
    bool IsValidPluginMountName(std::string_view mountName)
    {
        if (mountName.empty() || !IsAsciiAlpha(mountName.front()))
            return false;

        for (const char c : mountName)
        {
            if (!IsAsciiAlpha(c) && !IsAsciiDigit(c) && c != '_')
                return false;
        }

        return true;
    }

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

        if (segments[0] == "Plugins" && segments.size() >= 2)
        {
            if (!IsValidPluginMountName(segments[1]))
                return std::nullopt;

            virtualPath.domain = PathDomain::Plugin;
            virtualPath.mountName = segments[1];
            virtualPath.relativePath = JoinSegments(segments, 2);
            return virtualPath;
        }

        return std::nullopt;
    }

    bool IsCatalogBackedPath(std::string_view path)
    {
        const auto virtualPath = ParseVirtualPath(path);
        if (!virtualPath.has_value())
            return false;

        switch (virtualPath->domain)
        {
        case PathDomain::Project:
        case PathDomain::Engine:
        case PathDomain::Plugin:
            return !virtualPath->relativePath.empty() && !IsDocumentPath(path);
        case PathDomain::Saved:
        case PathDomain::Cache:
            return false;
        }

        return false;
    }

    bool IsDocumentPath(std::string_view path)
    {
        const auto virtualPath = ParseVirtualPath(path);
        return virtualPath.has_value() && HasDocumentExtension(virtualPath->relativePath);
    }
} // namespace Resource
