#pragma once

#include "Core/Resource/Path/PathParser.h"

#include <optional>
#include <string>
#include <string_view>

namespace Resource
{
    class AssetPath
    {
    public:
        AssetPath() = default;

        static bool IsValid(std::string_view path)
        {
            const auto parsed = ParseVirtualPath(path);
            if (!parsed.has_value() || parsed->relativePath.empty() || parsed->mountName.has_value())
                return false;

            switch (parsed->domain)
            {
            case PathDomain::Project:
            case PathDomain::Engine:
                break;
            case PathDomain::Saved:
            case PathDomain::Cache:
                return false;
            }

            const size_t slashPos = parsed->relativePath.find_last_of('/');
            const std::string_view fileName = slashPos == std::string_view::npos
                                                  ? std::string_view(parsed->relativePath)
                                                  : std::string_view(parsed->relativePath).substr(slashPos + 1);
            const size_t dotPos = fileName.find_last_of('.');
            return dotPos == std::string_view::npos || dotPos == 0 || dotPos + 1 == fileName.size();
        }

        static std::optional<AssetPath> TryCreate(std::string_view path)
        {
            if (!IsValid(path))
                return std::nullopt;

            AssetPath result;
            result.m_Path = std::string(path);
            return result;
        }

        const std::string &String() const
        {
            return m_Path;
        }

        std::string_view View() const
        {
            return m_Path;
        }

        bool Empty() const
        {
            return m_Path.empty();
        }

        friend bool operator==(const AssetPath &lhs, const AssetPath &rhs) = default;

    private:
        std::string m_Path;
    };
} // namespace Resource
