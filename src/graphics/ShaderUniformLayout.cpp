#include "graphics/ShaderUniformLayout.h"

#include <cstring>

#include <spdlog/fmt/fmt.h>

uint32_t GetShaderUniformValueTypeSize(ShaderUniformValueType type)
{
    switch (type)
    {
    case ShaderUniformValueType::Bool:
    case ShaderUniformValueType::Int:
    case ShaderUniformValueType::Float:
        return 4;
    case ShaderUniformValueType::Int2:
    case ShaderUniformValueType::Float2:
        return 8;
    case ShaderUniformValueType::Int3:
    case ShaderUniformValueType::Float3:
        return 12;
    case ShaderUniformValueType::Int4:
    case ShaderUniformValueType::Float4:
        return 16;
    case ShaderUniformValueType::Mat3:
        return 36;
    case ShaderUniformValueType::Mat4:
        return 64;
    case ShaderUniformValueType::Unknown:
    default:
        return 0;
    }
}

ShaderUniformBlockLayout::ShaderUniformBlockLayout(std::string name, uint32_t binding, uint32_t size)
    : m_Name(std::move(name)), m_Binding(binding), m_Size(size)
{
}

bool ShaderUniformBlockLayout::AddField(ShaderUniformFieldInfo field)
{
    if (field.Name.empty())
        return false;

    if (m_FieldLookup.find(field.Name) != m_FieldLookup.end())
        return false;

    m_FieldLookup[field.Name] = m_Fields.size();
    m_Fields.push_back(std::move(field));
    return true;
}

const ShaderUniformFieldInfo *ShaderUniformBlockLayout::FindField(std::string_view fieldName) const
{
    auto it = m_FieldLookup.find(std::string(fieldName));
    if (it == m_FieldLookup.end())
        return nullptr;

    return &m_Fields[it->second];
}

PackedUniformBlock::PackedUniformBlock(const ShaderUniformBlockLayout &layout)
    : m_Layout(layout), m_Data(layout.GetSize(), std::byte{0})
{
}

bool PackedUniformBlock::WriteRaw(const std::string &fieldName, const void *data, uint32_t size)
{
    const ShaderUniformFieldInfo *field = m_Layout.FindField(fieldName);
    if (!field)
    {
        SetError(fmt::format("Field '{}' was not found in reflected layout for block '{}'",
                             fieldName, m_Layout.GetName()));
        return false;
    }

    if (!data)
    {
        SetError(fmt::format("Field '{}' write used a null data pointer", fieldName));
        return false;
    }

    const uint32_t logicalSize = GetShaderUniformValueTypeSize(field->Type);
    const bool exactFieldWrite = (size == field->Size);
    const bool logicalWriteFits = (logicalSize != 0) && (size == logicalSize) && (logicalSize <= field->Size);
    if (!exactFieldWrite && !logicalWriteFits)
    {
        SetError(fmt::format(
            "Field '{}' size mismatch: tried to write {} bytes, expected {}",
            fieldName, size, field->Size));
        return false;
    }

    if (field->Offset + field->Size > m_Data.size())
    {
        SetError(fmt::format("Field '{}' range [{}..{}) exceeds block size {}",
                             fieldName, field->Offset, field->Offset + field->Size, m_Data.size()));
        return false;
    }

    if (size < field->Size)
    {
        // Preserve reflected padding semantics for logical-sized writes like float3 -> 16-byte slot.
        std::memset(m_Data.data() + field->Offset, 0, field->Size);
    }

    std::memcpy(m_Data.data() + field->Offset, data, size);
    return true;
}

void PackedUniformBlock::SetError(std::string error)
{
    m_LastError = std::move(error);
}
