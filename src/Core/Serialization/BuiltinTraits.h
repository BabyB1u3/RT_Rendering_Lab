#pragma once

/// @file BuiltinTraits.h
/// @brief Built-in Serialize/Deserialize overloads for primitives, GLM types,
///        enums (via magic_enum), and standard containers.

#include "Core/Serialization/SerializationTraits.h"
#include "Core/Diagnostics/Logging/LogCategories.h"
#include "Core/Diagnostics/Logging/LogMacros.h"
#include "Core/Resource/Catalog/AssetPath.h"

#include <glm/glm.hpp>
#include <magic_enum.hpp>

#include <optional>
#include <limits>
#include <string>
#include <vector>
#include <unordered_map>

namespace Serialization
{

    // ════════════════════════════════════════════════════════════════════�?
    // Primitives
    // ════════════════════════════════════════════════════════════════════�?

    inline void Serialize(PropertyTree &tree, bool v) { tree = PropertyTree(v); }
    inline void Serialize(PropertyTree &tree, int v) { tree = PropertyTree(v); }
    inline void Serialize(PropertyTree &tree, int64_t v) { tree = PropertyTree(v); }
    inline void Serialize(PropertyTree &tree, float v) { tree = PropertyTree(v); }
    inline void Serialize(PropertyTree &tree, double v) { tree = PropertyTree(v); }
    inline void Serialize(PropertyTree &tree, const std::string &v) { tree = PropertyTree(v); }

    inline bool Deserialize(const PropertyTree &tree, bool &v)
    {
        if (!tree.IsBool())
            return false;
        v = tree.AsBool();
        return true;
    }

    inline bool Deserialize(const PropertyTree &tree, int &v)
    {
        if (!tree.IsInt())
            return false;
        v = static_cast<int>(tree.AsInt());
        return true;
    }

    inline bool Deserialize(const PropertyTree &tree, int64_t &v)
    {
        if (!tree.IsInt())
            return false;
        v = tree.AsInt();
        return true;
    }

    inline bool Deserialize(const PropertyTree &tree, float &v)
    {
        if (!tree.IsNumber())
            return false;
        v = static_cast<float>(tree.AsFloat());
        return true;
    }

    inline bool Deserialize(const PropertyTree &tree, double &v)
    {
        if (!tree.IsNumber())
            return false;
        v = tree.AsFloat();
        return true;
    }

    inline bool Deserialize(const PropertyTree &tree, std::string &v)
    {
        if (!tree.IsString())
            return false;
        v = tree.AsString();
        return true;
    }

    inline void Serialize(PropertyTree &tree, const Resource::AssetPath &path)
    {
        tree = PropertyTree(path.String());
    }

    inline bool Deserialize(const PropertyTree &tree, Resource::AssetPath &path)
    {
        if (!tree.IsString())
            return false;

        const auto parsed = Resource::AssetPath::TryCreate(tree.AsString());
        if (!parsed.has_value())
        {
            LOG_WARN_CAT(LogCategory::Serialization,
                         "BuiltinTraits: invalid asset path '{}'",
                         tree.AsString());
            return false;
        }

        path = *parsed;
        return true;
    }

    inline void Serialize(PropertyTree &tree, uint8_t v) { tree = PropertyTree(static_cast<int>(v)); }
    inline void Serialize(PropertyTree &tree, uint16_t v) { tree = PropertyTree(static_cast<int>(v)); }

    inline bool Deserialize(const PropertyTree &tree, uint8_t &v)
    {
        if (!tree.IsInt())
            return false;
        const auto raw = tree.AsInt();
        if (raw < 0 || raw > static_cast<int64_t>(std::numeric_limits<uint8_t>::max()))
            return false;
        v = static_cast<uint8_t>(raw);
        return true;
    }

    inline bool Deserialize(const PropertyTree &tree, uint16_t &v)
    {
        if (!tree.IsInt())
            return false;
        const auto raw = tree.AsInt();
        if (raw < 0 || raw > static_cast<int64_t>(std::numeric_limits<uint16_t>::max()))
            return false;
        v = static_cast<uint16_t>(raw);
        return true;
    }

    // ════════════════════════════════════════════════════════════════════�?
    // Named enums (via magic_enum)
    // ════════════════════════════════════════════════════════════════════�?
    // Default path: enumerator name as string token.
    // Provide a custom overload to opt out (e.g., Key::Code uses InputNames.h).

    template <typename E>
        requires std::is_enum_v<E>
    void Serialize(PropertyTree &tree, E value)
    {
        auto name = magic_enum::enum_name(value);
        tree = PropertyTree(std::string(name));
    }

    template <typename E>
        requires std::is_enum_v<E>
    bool Deserialize(const PropertyTree &tree, E &value)
    {
        if (!tree.IsString())
            return false;
        auto result = magic_enum::enum_cast<E>(tree.AsString());
        if (!result.has_value())
        {
            LOG_WARN_CAT(LogCategory::Core, "BuiltinTraits: unknown enum value '{}' for type {}",
                         tree.AsString(), magic_enum::enum_type_name<E>());
            return false;
        }
        value = *result;
        return true;
    }

    // ════════════════════════════════════════════════════════════════════�?
    // GLM types
    // ════════════════════════════════════════════════════════════════════�?

    // --- glm::vec2 ---
    inline void Serialize(PropertyTree &tree, const glm::vec2 &v)
    {
        tree = PropertyTree::Array{PropertyTree(v.x), PropertyTree(v.y)};
    }

    inline bool Deserialize(const PropertyTree &tree, glm::vec2 &v)
    {
        if (!tree.IsArray() || tree.Size() != 2)
            return false;
        v.x = static_cast<float>(tree[size_t(0)].AsFloat());
        v.y = static_cast<float>(tree[size_t(1)].AsFloat());
        return true;
    }

    // --- glm::vec3 ---
    inline void Serialize(PropertyTree &tree, const glm::vec3 &v)
    {
        tree = PropertyTree::Array{PropertyTree(v.x), PropertyTree(v.y), PropertyTree(v.z)};
    }

    inline bool Deserialize(const PropertyTree &tree, glm::vec3 &v)
    {
        if (!tree.IsArray() || tree.Size() != 3)
            return false;
        v.x = static_cast<float>(tree[size_t(0)].AsFloat());
        v.y = static_cast<float>(tree[size_t(1)].AsFloat());
        v.z = static_cast<float>(tree[size_t(2)].AsFloat());
        return true;
    }

    // --- glm::vec4 ---
    inline void Serialize(PropertyTree &tree, const glm::vec4 &v)
    {
        tree = PropertyTree::Array{
            PropertyTree(v.x), PropertyTree(v.y),
            PropertyTree(v.z), PropertyTree(v.w)};
    }

    inline bool Deserialize(const PropertyTree &tree, glm::vec4 &v)
    {
        if (!tree.IsArray() || tree.Size() != 4)
            return false;
        v.x = static_cast<float>(tree[size_t(0)].AsFloat());
        v.y = static_cast<float>(tree[size_t(1)].AsFloat());
        v.z = static_cast<float>(tree[size_t(2)].AsFloat());
        v.w = static_cast<float>(tree[size_t(3)].AsFloat());
        return true;
    }

    // --- glm::mat4 (16 floats, column-major) ---
    inline void Serialize(PropertyTree &tree, const glm::mat4 &m)
    {
        PropertyTree::Array arr;
        arr.reserve(16);
        const float *p = &m[0][0];
        for (int i = 0; i < 16; ++i)
            arr.push_back(PropertyTree(p[i]));
        tree = std::move(arr);
    }

    inline bool Deserialize(const PropertyTree &tree, glm::mat4 &m)
    {
        if (!tree.IsArray() || tree.Size() != 16)
            return false;
        float *p = &m[0][0];
        for (int i = 0; i < 16; ++i)
            p[i] = static_cast<float>(tree[size_t(i)].AsFloat());
        return true;
    }

    // ════════════════════════════════════════════════════════════════════�?
    // Standard containers
    // ════════════════════════════════════════════════════════════════════�?

    // --- std::vector<T> ---

    template <Serializable T>
    void Serialize(PropertyTree &tree, const std::vector<T> &vec)
    {
        PropertyTree::Array arr;
        arr.reserve(vec.size());
        for (const auto &elem : vec)
        {
            PropertyTree child;
            Serialize(child, elem);
            arr.push_back(std::move(child));
        }
        tree = std::move(arr);
    }

    template <Serializable T>
    bool Deserialize(const PropertyTree &tree, std::vector<T> &vec)
    {
        if (!tree.IsArray())
            return false;
        const auto &arr = tree.AsArray();
        std::vector<T> result;
        result.reserve(arr.size());
        for (const auto &elem : arr)
        {
            T val{};
            if (!Deserialize(elem, val))
                return false;
            result.push_back(std::move(val));
        }
        vec = std::move(result);
        return true;
    }

    // --- std::unordered_map<std::string, T> ---

    template <Serializable T>
    void Serialize(PropertyTree &tree, const std::unordered_map<std::string, T> &map)
    {
        PropertyTree::Object obj;
        for (const auto &[key, val] : map)
        {
            PropertyTree child;
            Serialize(child, val);
            obj[key] = std::move(child);
        }
        tree = std::move(obj);
    }

    template <Serializable T>
    bool Deserialize(const PropertyTree &tree, std::unordered_map<std::string, T> &map)
    {
        if (!tree.IsObject())
            return false;
        std::unordered_map<std::string, T> result;
        for (const auto &[key, child] : tree.AsObject())
        {
            T val{};
            if (!Deserialize(child, val))
                return false;
            result[key] = std::move(val);
        }
        map = std::move(result);
        return true;
    }

    // --- std::optional<T> ---

    template <Serializable T>
    void Serialize(PropertyTree &tree, const std::optional<T> &opt)
    {
        if (opt.has_value())
            Serialize(tree, *opt);
        else
            tree = PropertyTree(nullptr);
    }

    template <Serializable T>
    bool Deserialize(const PropertyTree &tree, std::optional<T> &opt)
    {
        if (tree.IsNull())
        {
            opt = std::nullopt;
            return true;
        }
        T val{};
        if (!Deserialize(tree, val))
            return false;
        opt = std::move(val);
        return true;
    }

} // namespace Serialization
