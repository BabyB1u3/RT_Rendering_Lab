#include "core/serialization/PropertyTree.h"

#include "core/diagnostics/Assert.h"

namespace Serialization
{

    // ── Construction ─────────────────────────────────────────────────────

    PropertyTree::PropertyTree(std::nullptr_t) : m_Value(Null{}) {}
    PropertyTree::PropertyTree(bool v) : m_Value(v) {}
    PropertyTree::PropertyTree(int v) : m_Value(static_cast<Int>(v)) {}
    PropertyTree::PropertyTree(int64_t v) : m_Value(v) {}
    PropertyTree::PropertyTree(double v) : m_Value(v) {}
    PropertyTree::PropertyTree(float v) : m_Value(static_cast<Float>(v)) {}
    PropertyTree::PropertyTree(const char *v) : m_Value(String(v)) {}
    PropertyTree::PropertyTree(std::string v) : m_Value(std::move(v)) {}
    PropertyTree::PropertyTree(Array v) : m_Value(std::move(v)) {}
    PropertyTree::PropertyTree(Object v) : m_Value(std::move(v)) {}

    // ── Type queries ─────────────────────────────────────────────────────

    bool PropertyTree::IsNull() const { return std::holds_alternative<Null>(m_Value); }
    bool PropertyTree::IsBool() const { return std::holds_alternative<Bool>(m_Value); }
    bool PropertyTree::IsInt() const { return std::holds_alternative<Int>(m_Value); }
    bool PropertyTree::IsFloat() const { return std::holds_alternative<Float>(m_Value); }
    bool PropertyTree::IsString() const { return std::holds_alternative<String>(m_Value); }
    bool PropertyTree::IsArray() const { return std::holds_alternative<Array>(m_Value); }
    bool PropertyTree::IsObject() const { return std::holds_alternative<Object>(m_Value); }
    bool PropertyTree::IsNumber() const { return IsInt() || IsFloat(); }

    // ── Typed access ─────────────────────────────────────────────────────

    PropertyTree::Bool PropertyTree::AsBool() const
    {
        RTRLAB_ASSERT_MSG(IsBool(), "PropertyTree::AsBool requires a bool value");
        return std::get<Bool>(m_Value);
    }

    PropertyTree::Int PropertyTree::AsInt() const
    {
        RTRLAB_ASSERT_MSG(IsInt(), "PropertyTree::AsInt requires an integer value");
        return std::get<Int>(m_Value);
    }

    PropertyTree::Float PropertyTree::AsFloat() const
    {
        // Promote Int → Float transparently
        if (IsInt())
            return static_cast<Float>(std::get<Int>(m_Value));
        RTRLAB_ASSERT_MSG(IsFloat(), "PropertyTree::AsFloat requires a numeric value");
        return std::get<Float>(m_Value);
    }

    const PropertyTree::String &PropertyTree::AsString() const
    {
        RTRLAB_ASSERT_MSG(IsString(), "PropertyTree::AsString requires a string value");
        return std::get<String>(m_Value);
    }

    const PropertyTree::Array &PropertyTree::AsArray() const
    {
        RTRLAB_ASSERT_MSG(IsArray(), "PropertyTree::AsArray requires an array value");
        return std::get<Array>(m_Value);
    }

    const PropertyTree::Object &PropertyTree::AsObject() const
    {
        RTRLAB_ASSERT_MSG(IsObject(), "PropertyTree::AsObject requires an object value");
        return std::get<Object>(m_Value);
    }

    PropertyTree::Array &PropertyTree::AsArray()
    {
        RTRLAB_ASSERT_MSG(IsArray(), "PropertyTree::AsArray requires an array value");
        return std::get<Array>(m_Value);
    }

    PropertyTree::Object &PropertyTree::AsObject()
    {
        RTRLAB_ASSERT_MSG(IsObject(), "PropertyTree::AsObject requires an object value");
        return std::get<Object>(m_Value);
    }

    // ── Object helpers ───────────────────────────────────────────────────

    bool PropertyTree::Contains(const std::string &key) const
    {
        if (!IsObject())
            return false;
        auto &obj = std::get<Object>(m_Value);
        return obj.find(key) != obj.end();
    }

    const PropertyTree &PropertyTree::operator[](const std::string &key) const
    {
        RTRLAB_ASSERT_MSG(IsObject(), "PropertyTree::operator[](key) requires an object value");
        auto &obj = std::get<Object>(m_Value);
        auto it = obj.find(key);
        RTRLAB_ASSERTF(it != obj.end(), "PropertyTree: key not found: {}", key);
        return it->second;
    }

    PropertyTree &PropertyTree::operator[](const std::string &key)
    {
        // Auto-promote to object if currently null
        if (IsNull())
            m_Value = Object{};
        RTRLAB_ASSERT_MSG(IsObject(), "PropertyTree::operator[](key) requires an object-compatible value");
        return std::get<Object>(m_Value)[key];
    }

    // ── Array helpers ────────────────────────────────────────────────────

    size_t PropertyTree::Size() const
    {
        if (IsArray())
            return std::get<Array>(m_Value).size();
        if (IsObject())
            return std::get<Object>(m_Value).size();
        return 0;
    }

    const PropertyTree &PropertyTree::operator[](size_t index) const
    {
        RTRLAB_ASSERT_MSG(IsArray(), "PropertyTree::operator[](index) requires an array value");
        const auto &array = std::get<Array>(m_Value);
        RTRLAB_ASSERTF(index < array.size(), "PropertyTree: index {} out of range (size={})", index, array.size());
        return array[index];
    }

    PropertyTree &PropertyTree::operator[](size_t index)
    {
        RTRLAB_ASSERT_MSG(IsArray(), "PropertyTree::operator[](index) requires an array value");
        auto &array = std::get<Array>(m_Value);
        RTRLAB_ASSERTF(index < array.size(), "PropertyTree: index {} out of range (size={})", index, array.size());
        return array[index];
    }

} // namespace Serialization
