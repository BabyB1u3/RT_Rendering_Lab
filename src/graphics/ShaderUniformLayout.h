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

    template<typename T>
    bool Write(const std::string &fieldName, const T &value)
    {
        if constexpr (std::is_same_v<T, bool>)
        {
            const int32_t encoded = value ? 1 : 0;
            return WriteRaw(fieldName, &encoded, sizeof(encoded));
        }
        else
        {
            return WriteRaw(fieldName, &value, sizeof(T));
        }
    }

    bool WriteRaw(const std::string &fieldName, const void *data, uint32_t size);

    const void *Data() const { return m_Data.data(); }
    uint32_t Size() const { return static_cast<uint32_t>(m_Data.size()); }
    const ShaderUniformBlockLayout &GetLayout() const { return m_Layout; }
    const std::string &GetLastError() const { return m_LastError; }
    bool HasError() const { return !m_LastError.empty(); }

private:
    void SetError(std::string error);

private:
    const ShaderUniformBlockLayout &m_Layout;
    std::vector<std::byte> m_Data;
    std::string m_LastError;
};
