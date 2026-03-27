#pragma once

/// @file PropertyTree.h
/// @brief Format-neutral intermediate representation for serialization.
///
/// A lightweight value tree (similar in shape to a JSON DOM) that decouples
/// domain types from any specific on-disk format.  Backends convert between
/// PropertyTree and their wire format; traits convert between domain types
/// and PropertyTree.

#include <string>
#include <vector>
#include <map>
#include <variant>
#include <optional>
#include <cstdint>

namespace Serialization
{

    class PropertyTree
    {
    public:
        // --- Supported value types ---
        using Null = std::monostate;
        using Bool = bool;
        using Int = int64_t;
        using Float = double;
        using String = std::string;
        using Array = std::vector<PropertyTree>;
        using Object = std::map<std::string, PropertyTree>; // ordered for stable output

        using Value = std::variant<Null, Bool, Int, Float, String, Array, Object>;

        // --- Construction ---
        PropertyTree() = default;
        PropertyTree(std::nullptr_t);
        PropertyTree(bool v);
        PropertyTree(int v);
        PropertyTree(int64_t v);
        PropertyTree(double v);
        PropertyTree(float v);
        PropertyTree(const char *v);
        PropertyTree(std::string v);
        PropertyTree(Array v);
        PropertyTree(Object v);

        // --- Type queries ---
        bool IsNull() const;
        bool IsBool() const;
        bool IsInt() const;
        bool IsFloat() const;
        bool IsString() const;
        bool IsArray() const;
        bool IsObject() const;

        /// True if the value is numeric (Int or Float).
        bool IsNumber() const;

        // --- Typed access (throws std::bad_variant_access on mismatch) ------
        Bool AsBool() const;
        Int AsInt() const;
        Float AsFloat() const; // also accepts Int and promotes
        const String &AsString() const;
        const Array &AsArray() const;
        const Object &AsObject() const;

        // Mutable access
        Array &AsArray();
        Object &AsObject();

        // --- Object helpers ---
        bool Contains(const std::string &key) const;
        const PropertyTree &operator[](const std::string &key) const;
        PropertyTree &operator[](const std::string &key);

        /// Get a child value with fallback - never throws.
        template <typename T>
        T GetOr(const std::string &key, const T &fallback) const;

        // --- Array helpers ---------------
        size_t Size() const; // Array or Object size
        const PropertyTree &operator[](size_t index) const;
        PropertyTree &operator[](size_t index);

        // --- Direct value access ------
        const Value &GetValue() const { return m_Value; }

    private:
        Value m_Value;
    };

    // --- Template implementation ------

    template <typename T>
    T PropertyTree::GetOr(const std::string &key, const T &fallback) const
    {
        if (!IsObject())
            return fallback;
        auto &obj = std::get<Object>(m_Value);
        auto it = obj.find(key);
        if (it == obj.end())
            return fallback;

        if constexpr (std::is_same_v<T, bool>)
        {
            return it->second.IsBool() ? it->second.AsBool() : fallback;
        }
        else if constexpr (std::is_integral_v<T>)
        {
            return it->second.IsInt() ? static_cast<T>(it->second.AsInt()) : fallback;
        }
        else if constexpr (std::is_floating_point_v<T>)
        {
            return it->second.IsNumber() ? static_cast<T>(it->second.AsFloat()) : fallback;
        }
        else if constexpr (std::is_same_v<T, std::string>)
        {
            return it->second.IsString() ? it->second.AsString() : fallback;
        }
        else
        {
            return fallback;
        }
    }

} // namespace Serialization
