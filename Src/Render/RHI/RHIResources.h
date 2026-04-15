#pragma once

/// @file RHIResources.h
/// @brief Public RHI resource types, swapchain types, and resource-set value types.

#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "Core/Util/Base.h"
#include "Render/RHI/NativeWindowHandle.h"

class PipelineLayout;

enum class Format
{
    Unknown,

    R8_UNORM,
    RG8_UNORM,
    RGBA8_UNORM,
    RGBA8_SRGB,
    BGRA8_UNORM,
    BGRA8_SRGB,

    R16F,
    RG16F,
    RGBA16F,

    R32F,
    RG32F,
    RGBA32F,

    R32_UINT,

    D16_UNORM,
    D32_SFLOAT,
    D24_UNORM_S8_UINT,
    D32_SFLOAT_S8_UINT,
};

enum class BufferUsage : uint32_t
{
    None = 0,
    Vertex = BIT(0),
    Index = BIT(1),
    Uniform = BIT(2),
    Storage = BIT(3),
    CopySrc = BIT(4),
    CopyDst = BIT(5),
    Indirect = BIT(6),
};

constexpr BufferUsage operator|(BufferUsage lhs, BufferUsage rhs)
{
    return static_cast<BufferUsage>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

constexpr BufferUsage operator&(BufferUsage lhs, BufferUsage rhs)
{
    return static_cast<BufferUsage>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}

constexpr BufferUsage& operator|=(BufferUsage& lhs, BufferUsage rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

enum class MemoryUsage
{
    GpuOnly,
    CpuToGpu,
    GpuToCpu,
};

struct BufferDesc
{
    uint64_t size = 0;
    BufferUsage usageMask = BufferUsage::None;
    MemoryUsage memoryUsage = MemoryUsage::GpuOnly;
    const char* debugName = nullptr;
};

class Buffer
{
public:
    virtual ~Buffer() = default;

    virtual const BufferDesc& getDesc() const = 0;
};

enum class TextureType
{
    Tex2D,
    Tex2DArray,
    Tex3D,
    Cube,
};

enum class TextureUsage : uint32_t
{
    None = 0,
    Sampled = BIT(0),
    Storage = BIT(1),
    RenderTarget = BIT(2),
    DepthStencil = BIT(3),
    CopySrc = BIT(4),
    CopyDst = BIT(5),
};

constexpr TextureUsage operator|(TextureUsage lhs, TextureUsage rhs)
{
    return static_cast<TextureUsage>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

constexpr TextureUsage operator&(TextureUsage lhs, TextureUsage rhs)
{
    return static_cast<TextureUsage>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}

constexpr TextureUsage& operator|=(TextureUsage& lhs, TextureUsage rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

struct Extent3D
{
    uint32_t width = 1;
    uint32_t height = 1;
    uint32_t depth = 1;
};

struct TextureDesc
{
    TextureType type = TextureType::Tex2D;
    Format format = Format::Unknown;
    Extent3D extent;
    uint32_t mipLevels = 1;
    uint32_t arrayLayers = 1;
    TextureUsage usageMask = TextureUsage::None;
    const char* debugName = nullptr;
};

class Texture
{
public:
    virtual ~Texture() = default;

    virtual const TextureDesc& getDesc() const = 0;
};

enum class FilterMode
{
    Nearest,
    Linear,
};

enum class MipFilterMode
{
    None,
    Nearest,
    Linear,
};

enum class AddressMode
{
    Repeat,
    MirroredRepeat,
    ClampToEdge,
    ClampToBorder,
};

struct SamplerDesc
{
    FilterMode minFilter = FilterMode::Linear;
    FilterMode magFilter = FilterMode::Linear;
    MipFilterMode mipFilter = MipFilterMode::Linear;
    AddressMode addressU = AddressMode::Repeat;
    AddressMode addressV = AddressMode::Repeat;
    AddressMode addressW = AddressMode::Repeat;
    float minLod = 0.0f;
    float maxLod = FLT_MAX;
    float mipLodBias = 0.0f;
    bool anisotropyEnable = false;
    float maxAnisotropy = 1.0f;
};

class Sampler
{
public:
    virtual ~Sampler() = default;

    virtual const SamplerDesc& getDesc() const = 0;
};

enum class TextureAspect : uint32_t
{
    None = 0,
    Color = BIT(0),
    Depth = BIT(1),
    Stencil = BIT(2),
};

constexpr TextureAspect operator|(TextureAspect lhs, TextureAspect rhs)
{
    return static_cast<TextureAspect>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

constexpr TextureAspect operator&(TextureAspect lhs, TextureAspect rhs)
{
    return static_cast<TextureAspect>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}

constexpr TextureAspect& operator|=(TextureAspect& lhs, TextureAspect rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

struct TextureViewDesc
{
    TextureType type = TextureType::Tex2D;
    Format format = Format::Unknown;
    TextureAspect aspect = TextureAspect::Color;
    uint32_t baseMipLevel = 0;
    uint32_t mipLevelCount = 1;
    uint32_t baseArrayLayer = 0;
    uint32_t arrayLayerCount = 1;
};

class TextureView
{
public:
    virtual ~TextureView() = default;

    virtual Texture* getTexture() const = 0;
    virtual const TextureViewDesc& getDesc() const = 0;
};

struct SwapchainDesc
{
    uint32_t width = 0;
    uint32_t height = 0;
    Format format = Format::BGRA8_UNORM;
    uint32_t imageCount = 2;
    bool vsync = true;
};

class Swapchain
{
public:
    virtual ~Swapchain() = default;

    virtual uint32_t acquireNextImage() = 0;
    virtual Texture* getImage(uint32_t imageIndex) const = 0;
    virtual TextureView* getImageView(uint32_t imageIndex) const = 0;
    virtual void present(uint32_t imageIndex) = 0;
    virtual void resize(uint32_t newWidth, uint32_t newHeight) = 0;
    virtual uint32_t width() const = 0;
    virtual uint32_t height() const = 0;
    virtual Format format() const = 0;
    virtual uint32_t imageCount() const = 0;
};

class ParameterBlockData
{
public:
    void setRaw(uint32_t offset, const void* data, size_t size)
    {
        if (size == 0)
            return;

        if (offset + size > m_Data.size())
            m_Data.resize(offset + size);

        std::memcpy(m_Data.data() + offset, data, size);
    }

    template <typename T> void set(uint32_t offset, const T& value) { setRaw(offset, &value, sizeof(T)); }

    const void* data() const { return m_Data.empty() ? nullptr : m_Data.data(); }
    size_t size() const { return m_Data.size(); }
    void resize(size_t bytes) { m_Data.resize(bytes); }

private:
    std::vector<uint8_t> m_Data;
};

struct BufferBinding
{
    Buffer* buffer = nullptr;
    uint64_t offset = 0;
    uint64_t size = 0;
};

struct TextureBinding
{
    Texture* texture = nullptr;
    TextureView* view = nullptr;
};

struct SamplerBinding
{
    Sampler* sampler = nullptr;
};

class ResourceSet
{
public:
    virtual ~ResourceSet() = default;

    virtual PipelineLayout* getLayout() const = 0;
    virtual uint32_t getSetIndex() const = 0;

    virtual ParameterBlockData& constants() = 0;
    virtual const ParameterBlockData& constants() const = 0;

    virtual void setBuffer(uint32_t binding, const BufferBinding& bufferBinding) = 0;
    virtual void setTexture(uint32_t binding, const TextureBinding& textureBinding) = 0;
    virtual void setSampler(uint32_t binding, const SamplerBinding& samplerBinding) = 0;

    virtual uint32_t version() const = 0;
};
