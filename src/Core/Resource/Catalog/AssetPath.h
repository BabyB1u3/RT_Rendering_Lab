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
            return !path.empty() && IsCatalogBackedPath(path);
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
