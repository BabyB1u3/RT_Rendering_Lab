#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <initializer_list>
#include <cassert>

#include <glad/glad.h>

// TODO: Implement functions

// Type of the vertex/shader data
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
    case ShaderDataType::Float:  return 4;
    case ShaderDataType::Float2: return 4 * 2;
    case ShaderDataType::Float3: return 4 * 3;
    case ShaderDataType::Float4: return 4 * 4;
    case ShaderDataType::Mat3:   return 4 * 3 * 3;
    case ShaderDataType::Mat4:   return 4 * 4 * 4;
    case ShaderDataType::Int:    return 4;
    case ShaderDataType::Int2:   return 4 * 2;
    case ShaderDataType::Int3:   return 4 * 3;
    case ShaderDataType::Int4:   return 4 * 4;
    case ShaderDataType::Bool:   return 1;
    case ShaderDataType::None:   return 0;
    }

    assert(false && "Unknown ShaderDataType");
    return 0;
}

// A single element/attribute in a vertex buffer layout
struct BufferElement
{
    std::string Name;
    ShaderDataType Type = ShaderDataType::None;
    size_t Offset = 0;
    uint32_t Size = 0;
    bool Normalized = false;

    BufferElement() = default;

    BufferElement(ShaderDataType type, const std::string& name, bool normalized = false)
        : Name(name), Type(type), Offset(0), Size(ShaderDataTypeSize(type)), Normalized(normalized)
    {
    }

    uint32_t GetComponentCount() const
    {
        switch (Type)
        {
        case ShaderDataType::Float:  return 1;
        case ShaderDataType::Float2: return 2;
        case ShaderDataType::Float3: return 3;
        case ShaderDataType::Float4: return 4;
        case ShaderDataType::Mat3:   return 3;
        case ShaderDataType::Mat4:   return 4;
        case ShaderDataType::Int:    return 1;
        case ShaderDataType::Int2:   return 2;
        case ShaderDataType::Int3:   return 3;
        case ShaderDataType::Int4:   return 4;
        case ShaderDataType::Bool:   return 1;
        case ShaderDataType::None:   return 0;
        }

        assert(false && "Unknown ShaderDataType");
        return 0;
    }
};

// Describes the layout of vertex data in a vertex buffer
class BufferLayout
{
public:
    BufferLayout() = default;

    BufferLayout(const std::initializer_list<BufferElement>& elements)
        : m_Elements(elements)
    {
        CalculateOffsetsAndStride();
    }

    uint32_t GetStride() const { return m_Stride; }
    const std::vector<BufferElement>& GetElements() const { return m_Elements; }

    std::vector<BufferElement>::iterator begin() { return m_Elements.begin(); }
    std::vector<BufferElement>::iterator end() { return m_Elements.end(); }
    std::vector<BufferElement>::const_iterator begin() const { return m_Elements.begin(); }
    std::vector<BufferElement>::const_iterator end() const { return m_Elements.end(); }

private:
    void CalculateOffsetsAndStride()
    {
        size_t offset = 0;
        m_Stride = 0;

        for (auto& element : m_Elements)
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

inline GLenum ToOpenGLBufferUsage(BufferUsage usage)
{
    switch (usage)
    {
    case BufferUsage::StaticDraw:  return GL_STATIC_DRAW;
    case BufferUsage::DynamicDraw: return GL_DYNAMIC_DRAW;
    case BufferUsage::StreamDraw:  return GL_STREAM_DRAW;
    }

    return GL_STATIC_DRAW;
}

//-------------------- Vertex Buffer --------------------
class VertexBuffer
{
public:
    VertexBuffer(uint32_t size, BufferUsage usage = BufferUsage::DynamicDraw);
    VertexBuffer(const void* data, uint32_t size, BufferUsage usage = BufferUsage::StaticDraw);
    ~VertexBuffer();

    VertexBuffer(const VertexBuffer&) = delete;
    VertexBuffer& operator=(const VertexBuffer&) = delete;

    VertexBuffer(VertexBuffer&& other) noexcept;
    VertexBuffer& operator=(VertexBuffer&& other) noexcept;

    void Bind() const;
    void Unbind() const;

    void SetData(const void* data, uint32_t size, uint32_t offset = 0);

    void SetLayout(const BufferLayout& layout) { m_Layout = layout; }
    const BufferLayout& GetLayout() const { return m_Layout; }

    uint32_t GetRendererID() const { return m_RendererID; }

private:
    uint32_t m_RendererID = 0;
    BufferLayout m_Layout;
    BufferUsage m_Usage = BufferUsage::StaticDraw;
};

//-------------------- Index Buffer ---------------------

class IndexBuffer
{
public:
    IndexBuffer(const uint32_t* indices, uint32_t count);
    ~IndexBuffer();

    IndexBuffer(const IndexBuffer&) = delete;
    IndexBuffer& operator=(const IndexBuffer&) = delete;

    IndexBuffer(IndexBuffer&& other) noexcept;
    IndexBuffer& operator=(IndexBuffer&& other) noexcept;

    void Bind() const;
    void Unbind() const;

    uint32_t GetCount() const { return m_Count; }
    uint32_t GetRendererID() const { return m_RendererID; }

private:
    uint32_t m_RendererID = 0;
    uint32_t m_Count = 0;
};