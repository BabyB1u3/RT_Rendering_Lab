#pragma once

/// @file Buffers.h
/// @brief Backend-agnostic buffer types: ShaderDataType, BufferElement, BufferLayout, BufferUsage.
///
/// Concrete buffer classes live in the backend subdirectory (e.g. opengl/GLVertexBuffer).
/// Create buffers via GetDevice()->CreateVertexBuffer() / CreateIndexBuffer().
///
/// Typical usage:
///   1. Create a BufferLayout describing the vertex attributes.
///   2. Create an IVertexBuffer via GetDevice() and assign the layout.
///   3. Create an IIndexBuffer via GetDevice().
///   4. Feed both into an IVertexArray via AddVertexBuffer / SetIndexBuffer.

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <initializer_list>

#include "core/diagnostics/Assert.h"

/// Enumerates the data types that can appear in a vertex attribute.
enum class ShaderDataType
{
    None = 0,
    Float,
    Float2,
    Float3,
    Float4,
    Mat3,
    Mat4,
    Int,
    Int2,
    Int3,
    Int4,
    Bool
};

// Get the size in bytes of each shader data type
inline uint32_t ShaderDataTypeSize(ShaderDataType type)
{
    switch (type)
    {
    case ShaderDataType::Float:
        return 4;
    case ShaderDataType::Float2:
        return 4 * 2;
    case ShaderDataType::Float3:
        return 4 * 3;
    case ShaderDataType::Float4:
        return 4 * 4;
    case ShaderDataType::Mat3:
        return 4 * 3 * 3;
    case ShaderDataType::Mat4:
        return 4 * 4 * 4;
    case ShaderDataType::Int:
        return 4;
    case ShaderDataType::Int2:
        return 4 * 2;
    case ShaderDataType::Int3:
        return 4 * 3;
    case ShaderDataType::Int4:
        return 4 * 4;
    case ShaderDataType::Bool:
        return 1;
    case ShaderDataType::None:
        return 0;
    }

    RTRLAB_ASSERT_MSG(false, "Unknown ShaderDataType");
    return 0;
}

/// A single vertex attribute description (name, type, byte offset, divisor).
struct BufferElement
{
    std::string Name;
    ShaderDataType Type = ShaderDataType::None;
    size_t Offset = 0;
    uint32_t Size = 0;
    uint32_t Divisor = 0;
    bool Normalized = false;

    BufferElement() = default;

    BufferElement(ShaderDataType type, const std::string &name, bool normalized = false, uint32_t divisor = 0)
        : Name(name), Type(type), Offset(0), Size(ShaderDataTypeSize(type)), Divisor(divisor), Normalized(normalized)
    {
    }

    uint32_t GetComponentCount() const
    {
        switch (Type)
        {
        case ShaderDataType::Float:
            return 1;
        case ShaderDataType::Float2:
            return 2;
        case ShaderDataType::Float3:
            return 3;
        case ShaderDataType::Float4:
            return 4;
        case ShaderDataType::Mat3:
            return 3;
        case ShaderDataType::Mat4:
            return 4;
        case ShaderDataType::Int:
            return 1;
        case ShaderDataType::Int2:
            return 2;
        case ShaderDataType::Int3:
            return 3;
        case ShaderDataType::Int4:
            return 4;
        case ShaderDataType::Bool:
            return 1;
        case ShaderDataType::None:
            return 0;
        }

        RTRLAB_ASSERT_MSG(false, "Unknown ShaderDataType");
        return 0;
    }
};

/// Describes the memory layout of interleaved vertex data.
/// Automatically computes per-element byte offsets and the total stride.
class BufferLayout
{
public:
    BufferLayout() = default;

    BufferLayout(const std::initializer_list<BufferElement> &elements)
        : m_Elements(elements)
    {
        CalculateOffsetsAndStride();
    }

    uint32_t GetStride() const { return m_Stride; }
    const std::vector<BufferElement> &GetElements() const { return m_Elements; }

    std::vector<BufferElement>::iterator begin() { return m_Elements.begin(); }
    std::vector<BufferElement>::iterator end() { return m_Elements.end(); }
    std::vector<BufferElement>::const_iterator begin() const { return m_Elements.begin(); }
    std::vector<BufferElement>::const_iterator end() const { return m_Elements.end(); }

private:
    void CalculateOffsetsAndStride()
    {
        size_t offset = 0;
        m_Stride = 0;

        for (auto &element : m_Elements)
        {
            element.Offset = offset;
            offset += element.Size;
            m_Stride += element.Size;
        }
    }

private:
    std::vector<BufferElement> m_Elements;
    uint32_t m_Stride = 0;
};

enum class BufferUsage
{
    StaticDraw,
    DynamicDraw,
    StreamDraw
};
