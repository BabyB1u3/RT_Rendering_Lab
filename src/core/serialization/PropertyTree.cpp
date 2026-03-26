#include "core/serialization/PropertyTree.h"

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
        return std::get<Bool>(m_Value);
    }

    PropertyTree::Int PropertyTree::AsInt() const
    {
        return std::get<Int>(m_Value);
    }

    PropertyTree::Float PropertyTree::AsFloat() const
    {
        // Promote Int → Float transparently
        if (IsInt())
            return static_cast<Float>(std::get<Int>(m_Value));
        return std::get<Float>(m_Value);
    }

    const PropertyTree::String &PropertyTree::AsString() const
    {
        return std::get<String>(m_Value);
    }

    const PropertyTree::Array &PropertyTree::AsArray() const
    {
        return std::get<Array>(m_Value);
    }

    const PropertyTree::Object &PropertyTree::AsObject() const
    {
        return std::get<Object>(m_Value);
    }

    PropertyTree::Array &PropertyTree::AsArray()
    {
        return std::get<Array>(m_Value);
    }

    PropertyTree::Object &PropertyTree::AsObject()
    {
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
        auto &obj = std::get<Object>(m_Value);
        auto it = obj.find(key);
        if (it == obj.end())
            throw std::out_of_range("PropertyTree: key not found: " + key);
        return it->second;
    }

    PropertyTree &PropertyTree::operator[](const std::string &key)
    {
        // Auto-promote to object if currently null
        if (IsNull())
            m_Value = Object{};
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
        return std::get<Array>(m_Value).at(index);
    }

    PropertyTree &PropertyTree::operator[](size_t index)
    {
        return std::get<Array>(m_Value).at(index);
    }

} // namespace Serialization
