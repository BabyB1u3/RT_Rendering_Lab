#include "core/serialization/JsonBackend.h"
#include "core/diagnostics/LogMacros.h"

#include <json.hpp>

namespace Serialization
{

    namespace
    {
        // --- PropertyTree -> nlohmann::json ---

        nlohmann::json TreeToJson(const PropertyTree &tree)
        {
            return std::visit([](const auto &val) -> nlohmann::json
                              {
            using T = std::decay_t<decltype(val)>;

            if constexpr (std::is_same_v<T, PropertyTree::Null>)
                return nullptr;
            else if constexpr (std::is_same_v<T, PropertyTree::Bool>)
                return val;
            else if constexpr (std::is_same_v<T, PropertyTree::Int>)
                return val;
            else if constexpr (std::is_same_v<T, PropertyTree::Float>)
                return val;
            else if constexpr (std::is_same_v<T, PropertyTree::String>)
                return val;
            else if constexpr (std::is_same_v<T, PropertyTree::Array>)
            {
                auto arr = nlohmann::json::array();
                for (const auto &elem : val)
                    arr.push_back(TreeToJson(elem));
                return arr;
            }
            else if constexpr (std::is_same_v<T, PropertyTree::Object>)
            {
                auto obj = nlohmann::json::object();
                for (const auto &[key, child] : val)
                    obj[key] = TreeToJson(child);
                return obj;
            } }, tree.GetValue());
        }

        // --- nlohmann::json -> PropertyTree ---

        PropertyTree JsonToTree(const nlohmann::json &j)
        {
            switch (j.type())
            {
            case nlohmann::json::value_t::null:
                return PropertyTree(nullptr);

            case nlohmann::json::value_t::boolean:
                return PropertyTree(j.get<bool>());

            case nlohmann::json::value_t::number_integer:
            case nlohmann::json::value_t::number_unsigned:
                return PropertyTree(j.get<int64_t>());

            case nlohmann::json::value_t::number_float:
                return PropertyTree(j.get<double>());

            case nlohmann::json::value_t::string:
                return PropertyTree(j.get<std::string>());

            case nlohmann::json::value_t::array:
            {
                PropertyTree::Array arr;
                arr.reserve(j.size());
                for (const auto &elem : j)
                    arr.push_back(JsonToTree(elem));
                return PropertyTree(std::move(arr));
            }

            case nlohmann::json::value_t::object:
            {
                PropertyTree::Object obj;
                for (auto &[key, val] : j.items())
                    obj[key] = JsonToTree(val);
                return PropertyTree(std::move(obj));
            }

            default:
                return PropertyTree(nullptr);
            }
        }
    } // namespace

    // --- IFormatBackend implementation ---

    std::string JsonBackend::WriteToString(const PropertyTree &tree) const
    {
        return TreeToJson(tree).dump(m_Indent);
    }

    bool JsonBackend::ReadFromString(const std::string &data, PropertyTree &tree) const
    {
        try
        {
            auto j = nlohmann::json::parse(data);
            tree = JsonToTree(j);
            return true;
        }
        catch (const nlohmann::json::parse_error &e)
        {
            LOG_ERROR("JsonBackend: parse error: {}", e.what());
            return false;
        }
    }

} // namespace Serialization
