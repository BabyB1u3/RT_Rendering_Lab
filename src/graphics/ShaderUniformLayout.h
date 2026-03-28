#pragma once

/// @file ShaderUniformLayout.h
/// @brief Shared runtime metadata for reflected uniform blocks and packed uploads.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "core/diagnostics/Assert.h"

enum class ShaderUniformValueType : uint8_t
{
    Unknown = 0,
    Bool,
    Int,
    Int2,
    Int3,
    Int4,
    Float,
    Float2,
    Float3,
    Float4,
    Mat3,
    Mat4,
};

uint32_t GetShaderUniformValueTypeSize(ShaderUniformValueType type);

/// Normalize a raw GL uniform name as returned by glGetProgramResourceName() to the
/// canonical leaf field name used in PackedUniformBlock::Write() / WriteRequired().
///
/// Three transforms are applied in order:
///   1. Strip block prefix:   "BlockName.fieldName" → "fieldName"
///   2. Strip array suffix:   "fieldName[0]"        → "fieldName"
///   3. Strip numeric suffix: "fieldName_0"         → "fieldName"
///      (Slang may append _N to disambiguate GLSL output; the Slang source name has no suffix.)
///
/// The result must match the field name used in Write() / WriteRequired() and the name
/// stored in the Metal reflection sidecar. See Section 5.5 of uniform_reflection_migration.md.
std::string NormalizeGLUniformFieldName(std::string name);

struct ShaderUniformFieldInfo
{
    std::string Name;
    uint32_t Offset = 0;
    uint32_t Size = 0;
    ShaderUniformValueType Type = ShaderUniformValueType::Unknown;
};

class ShaderUniformBlockLayout
{
public:
    ShaderUniformBlockLayout() = default;
    ShaderUniformBlockLayout(std::string name, uint32_t binding, uint32_t size);

    const std::string &GetName() const { return m_Name; }
    uint32_t GetBinding() const { return m_Binding; }
    uint32_t GetSize() const { return m_Size; }
    const std::vector<ShaderUniformFieldInfo> &GetFields() const { return m_Fields; }

    void SetName(std::string name) { m_Name = std::move(name); }
    void SetBinding(uint32_t binding) { m_Binding = binding; }
    void SetSize(uint32_t size) { m_Size = size; }

    bool AddField(ShaderUniformFieldInfo field);
    const ShaderUniformFieldInfo *FindField(std::string_view fieldName) const;

private:
    std::string m_Name;
    uint32_t m_Binding = 0;
    uint32_t m_Size = 0;
    std::vector<ShaderUniformFieldInfo> m_Fields;
    std::unordered_map<std::string, size_t> m_FieldLookup;
};

class PackedUniformBlock
{
public:
    explicit PackedUniformBlock(const ShaderUniformBlockLayout &layout);

    /// Write a field by name. Returns false if the field is not found or the size
    /// is incompatible with the reflected type. Prefer WriteRequired() for fields
    /// that must be present; use Write() only when a missing field is recoverable.
    template<typename T>
    bool Write(const std::string &fieldName, const T &value)
    {
        if constexpr (std::is_same_v<T, bool>)
        {
            return WriteBool(fieldName, value);
        }
        else
        {
            return WriteRaw(fieldName, &value, sizeof(T));
        }
    }

    /// Write a field that must exist in the reflected layout. Asserts on failure.
    /// Use this for fields that are structurally required by the shader — a miss
    /// indicates a field name mismatch between C++ and the reflection data, which
    /// is always a bug and should never be silently ignored.
    template<typename T>
    void WriteRequired(const std::string &fieldName, const T &value)
    {
        RTRLAB_ASSERTF(Write(fieldName, value),
                       "Required field '{}' write failed in block '{}': {}",
                       fieldName, m_Layout.GetName(), m_LastError);
    }

    bool WriteRaw(const std::string &fieldName, const void *data, uint32_t size);

    const void *Data() const { return m_Data.data(); }
    uint32_t Size() const { return static_cast<uint32_t>(m_Data.size()); }
    const ShaderUniformBlockLayout &GetLayout() const { return m_Layout; }
    const std::string &GetLastError() const { return m_LastError; }
    bool HasError() const { return !m_LastError.empty(); }

private:
    bool WriteBool(const std::string &fieldName, bool value);
    void SetError(std::string error);

private:
    const ShaderUniformBlockLayout &m_Layout;
    std::vector<std::byte> m_Data;
    std::string m_LastError;
};
