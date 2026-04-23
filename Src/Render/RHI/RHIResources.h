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
    uint64_t m_Size = 0;
    BufferUsage m_UsageMask = BufferUsage::None;
    MemoryUsage m_MemoryUsage = MemoryUsage::GpuOnly;
    const char* m_DebugName = nullptr;
};

class Buffer
{
public:
    virtual ~Buffer() = default;

    virtual const BufferDesc& GetDesc() const = 0;
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
    uint32_t m_Width = 1;
    uint32_t m_Height = 1;
    uint32_t m_Depth = 1;
};

struct TextureDesc
{
    TextureType m_Type = TextureType::Tex2D;
    Format m_Format = Format::Unknown;
    Extent3D m_Extent;
    uint32_t m_MipLevels = 1;
    uint32_t m_ArrayLayers = 1;
    TextureUsage m_UsageMask = TextureUsage::None;
    const char* m_DebugName = nullptr;
};

class Texture
{
public:
    virtual ~Texture() = default;

    virtual const TextureDesc& GetDesc() const = 0;
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
    FilterMode m_MinFilter = FilterMode::Linear;
    FilterMode m_MagFilter = FilterMode::Linear;
    MipFilterMode m_MipFilter = MipFilterMode::Linear;
    AddressMode m_AddressU = AddressMode::Repeat;
    AddressMode m_AddressV = AddressMode::Repeat;
    AddressMode m_AddressW = AddressMode::Repeat;
    float m_MinLod = 0.0f;
    float m_MaxLod = FLT_MAX;
    float m_MipLodBias = 0.0f;
    bool m_AnisotropyEnable = false;
    float m_MaxAnisotropy = 1.0f;
};

class Sampler
{
public:
    virtual ~Sampler() = default;

    virtual const SamplerDesc& GetDesc() const = 0;
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
    TextureType m_Type = TextureType::Tex2D;
    Format m_Format = Format::Unknown;
    TextureAspect m_Aspect = TextureAspect::Color;
    uint32_t m_BaseMipLevel = 0;
    uint32_t m_MipLevelCount = 1;
    uint32_t m_BaseArrayLayer = 0;
    uint32_t m_ArrayLayerCount = 1;
};

class TextureView
{
public:
    virtual ~TextureView() = default;

    virtual Texture* GetTexture() const = 0;
    virtual const TextureViewDesc& GetDesc() const = 0;
};

struct SwapchainDesc
{
    uint32_t m_Width = 0;
    uint32_t m_Height = 0;
    Format m_Format = Format::BGRA8_UNORM;
    uint32_t m_ImageCount = 2;
    bool m_Vsync = true;
};

class Swapchain
{
public:
    virtual ~Swapchain() = default;

    virtual uint32_t AcquireNextImage() = 0;
    virtual Texture* GetImage(uint32_t imageIndex) const = 0;
    virtual TextureView* GetImageView(uint32_t imageIndex) const = 0;
    virtual void Present(uint32_t imageIndex) = 0;
    virtual void Resize(uint32_t newWidth, uint32_t newHeight) = 0;
    virtual uint32_t GetWidth() const = 0;
    virtual uint32_t GetHeight() const = 0;
    virtual Format GetFormat() const = 0;
    virtual uint32_t GetImageCount() const = 0;
};

class ParameterBlockData
{
public:
    void SetRaw(uint32_t offset, const void* data, size_t size)
    {
        if (size == 0)
            return;

        if (offset + size > m_Data.size())
            m_Data.resize(offset + size);

        std::memcpy(m_Data.data() + offset, data, size);
    }

    template <typename T> void Set(uint32_t offset, const T& value) { SetRaw(offset, &value, sizeof(T)); }

    const void* GetData() const { return m_Data.empty() ? nullptr : m_Data.data(); }
    size_t GetSize() const { return m_Data.size(); }
    void Resize(size_t bytes) { m_Data.resize(bytes); }

private:
    std::vector<uint8_t> m_Data;
};

struct BufferBinding
{
    Buffer* m_Buffer = nullptr;
    uint64_t m_Offset = 0;
    uint64_t m_Size = 0;
};

struct TextureBinding
{
    Texture* m_Texture = nullptr;
    TextureView* m_View = nullptr;
};

struct SamplerBinding
{
    Sampler* m_Sampler = nullptr;
};

class ResourceSet
{
public:
    virtual ~ResourceSet() = default;

    virtual PipelineLayout* GetLayout() const = 0;
    virtual uint32_t GetSetIndex() const = 0;

    virtual const ParameterBlockData& GetConstants() const = 0;
    virtual void SetConstantDataRaw(uint32_t offset, const void* data, size_t size) = 0;

    template <typename T> void SetConstantData(uint32_t offset, const T& value)
    {
        SetConstantDataRaw(offset, &value, sizeof(T));
    }

    virtual void SetBuffer(uint32_t binding, const BufferBinding& bufferBinding) = 0;
    virtual void SetTexture(uint32_t binding, const TextureBinding& textureBinding) = 0;
    virtual void SetSampler(uint32_t binding, const SamplerBinding& samplerBinding) = 0;

    virtual uint32_t GetVersion() const = 0;
};
