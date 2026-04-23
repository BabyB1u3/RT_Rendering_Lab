#include "Render/RHI/Backends/Vulkan/VulkanConversions.h"

namespace VulkanRHI
{
VkFormat ToVkFormat(Format format)
{
    switch (format)
    {
        case Format::R8_UNORM:
            return VK_FORMAT_R8_UNORM;
        case Format::RG8_UNORM:
            return VK_FORMAT_R8G8_UNORM;
        case Format::R16F:
            return VK_FORMAT_R16_SFLOAT;
        case Format::RG16F:
            return VK_FORMAT_R16G16_SFLOAT;
        case Format::RGBA16F:
            return VK_FORMAT_R16G16B16A16_SFLOAT;
        case Format::R32F:
            return VK_FORMAT_R32_SFLOAT;
        case Format::RG32F:
            return VK_FORMAT_R32G32_SFLOAT;
        case Format::RGBA32F:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        case Format::R32_UINT:
            return VK_FORMAT_R32_UINT;
        case Format::BGRA8_UNORM:
            return VK_FORMAT_B8G8R8A8_UNORM;
        case Format::BGRA8_SRGB:
            return VK_FORMAT_B8G8R8A8_SRGB;
        case Format::RGBA8_UNORM:
            return VK_FORMAT_R8G8B8A8_UNORM;
        case Format::RGBA8_SRGB:
            return VK_FORMAT_R8G8B8A8_SRGB;
        case Format::D16_UNORM:
            return VK_FORMAT_D16_UNORM;
        case Format::D32_SFLOAT:
            return VK_FORMAT_D32_SFLOAT;
        case Format::D24_UNORM_S8_UINT:
            return VK_FORMAT_D24_UNORM_S8_UINT;
        case Format::D32_SFLOAT_S8_UINT:
            return VK_FORMAT_D32_SFLOAT_S8_UINT;
        default:
            RTRLAB_ASSERTF(false, "Unsupported Vulkan RHI format {}", static_cast<uint32_t>(format));
            return VK_FORMAT_UNDEFINED;
    }
}

Format ToRhiFormat(VkFormat format)
{
    switch (format)
    {
        case VK_FORMAT_R8_UNORM:
            return Format::R8_UNORM;
        case VK_FORMAT_R8G8_UNORM:
            return Format::RG8_UNORM;
        case VK_FORMAT_B8G8R8A8_UNORM:
            return Format::BGRA8_UNORM;
        case VK_FORMAT_B8G8R8A8_SRGB:
            return Format::BGRA8_SRGB;
        case VK_FORMAT_R8G8B8A8_UNORM:
            return Format::RGBA8_UNORM;
        case VK_FORMAT_R8G8B8A8_SRGB:
            return Format::RGBA8_SRGB;
        case VK_FORMAT_R16_SFLOAT:
            return Format::R16F;
        case VK_FORMAT_R16G16_SFLOAT:
            return Format::RG16F;
        case VK_FORMAT_R16G16B16A16_SFLOAT:
            return Format::RGBA16F;
        case VK_FORMAT_R32_SFLOAT:
            return Format::R32F;
        case VK_FORMAT_R32G32_SFLOAT:
            return Format::RG32F;
        case VK_FORMAT_R32G32B32A32_SFLOAT:
            return Format::RGBA32F;
        case VK_FORMAT_R32_UINT:
            return Format::R32_UINT;
        case VK_FORMAT_D16_UNORM:
            return Format::D16_UNORM;
        case VK_FORMAT_D32_SFLOAT:
            return Format::D32_SFLOAT;
        case VK_FORMAT_D24_UNORM_S8_UINT:
            return Format::D24_UNORM_S8_UINT;
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return Format::D32_SFLOAT_S8_UINT;
        default:
            return Format::Unknown;
    }
}

bool IsDepthFormat(Format format)
{
    switch (format)
    {
        case Format::D16_UNORM:
        case Format::D32_SFLOAT:
        case Format::D24_UNORM_S8_UINT:
        case Format::D32_SFLOAT_S8_UINT:
            return true;
        default:
            return false;
    }
}

bool HasStencilComponent(Format format)
{
    return format == Format::D24_UNORM_S8_UINT || format == Format::D32_SFLOAT_S8_UINT;
}

VkBufferUsageFlags ToVkBufferUsage(BufferUsage usageMask)
{
    VkBufferUsageFlags usage = 0;

    if ((usageMask & BufferUsage::Vertex) != BufferUsage::None)
        usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if ((usageMask & BufferUsage::Index) != BufferUsage::None)
        usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if ((usageMask & BufferUsage::Uniform) != BufferUsage::None)
        usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if ((usageMask & BufferUsage::Storage) != BufferUsage::None)
        usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if ((usageMask & BufferUsage::CopySrc) != BufferUsage::None)
        usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if ((usageMask & BufferUsage::CopyDst) != BufferUsage::None)
        usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if ((usageMask & BufferUsage::Indirect) != BufferUsage::None)
        usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

    return usage;
}

VkImageUsageFlags ToVkImageUsage(TextureUsage usageMask)
{
    VkImageUsageFlags usage = 0;

    if ((usageMask & TextureUsage::Sampled) != TextureUsage::None)
        usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if ((usageMask & TextureUsage::Storage) != TextureUsage::None)
        usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    if ((usageMask & TextureUsage::RenderTarget) != TextureUsage::None)
        usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if ((usageMask & TextureUsage::DepthStencil) != TextureUsage::None)
        usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if ((usageMask & TextureUsage::CopySrc) != TextureUsage::None)
        usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if ((usageMask & TextureUsage::CopyDst) != TextureUsage::None)
        usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    return usage;
}

VkImageType ToVkImageType(TextureType type)
{
    switch (type)
    {
        case TextureType::Tex2D:
        case TextureType::Tex2DArray:
        case TextureType::Cube:
            return VK_IMAGE_TYPE_2D;
        case TextureType::Tex3D:
            return VK_IMAGE_TYPE_3D;
    }

    return VK_IMAGE_TYPE_2D;
}

VkImageViewType ToVkImageViewType(TextureType type)
{
    switch (type)
    {
        case TextureType::Tex2D:
            return VK_IMAGE_VIEW_TYPE_2D;
        case TextureType::Tex2DArray:
            return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        case TextureType::Tex3D:
            return VK_IMAGE_VIEW_TYPE_3D;
        case TextureType::Cube:
            return VK_IMAGE_VIEW_TYPE_CUBE;
    }

    return VK_IMAGE_VIEW_TYPE_2D;
}

VkFilter ToVkFilter(FilterMode mode)
{
    return mode == FilterMode::Nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
}

VkSamplerMipmapMode ToVkMipmapMode(MipFilterMode mode)
{
    switch (mode)
    {
        case MipFilterMode::None:
        case MipFilterMode::Nearest:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        case MipFilterMode::Linear:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }

    return VK_SAMPLER_MIPMAP_MODE_LINEAR;
}

VkSamplerAddressMode ToVkAddressMode(AddressMode mode)
{
    switch (mode)
    {
        case AddressMode::Repeat:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case AddressMode::MirroredRepeat:
            return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case AddressMode::ClampToEdge:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case AddressMode::ClampToBorder:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    }

    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

VkShaderStageFlags ToVkShaderStageFlags(ShaderStage stageMask)
{
    VkShaderStageFlags flags = 0;
    if ((stageMask & ShaderStage::Vertex) != ShaderStage::None)
        flags |= VK_SHADER_STAGE_VERTEX_BIT;
    if ((stageMask & ShaderStage::Fragment) != ShaderStage::None)
        flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    if ((stageMask & ShaderStage::Compute) != ShaderStage::None)
        flags |= VK_SHADER_STAGE_COMPUTE_BIT;
    return flags;
}

VkPrimitiveTopology ToVkPrimitiveTopology(PrimitiveTopology topology)
{
    switch (topology)
    {
        case PrimitiveTopology::TriangleList:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case PrimitiveTopology::TriangleStrip:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case PrimitiveTopology::LineList:
            return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case PrimitiveTopology::LineStrip:
            return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case PrimitiveTopology::PointList:
            return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    }

    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}

VkCullModeFlags ToVkCullMode(CullMode cullMode)
{
    switch (cullMode)
    {
        case CullMode::None:
            return VK_CULL_MODE_NONE;
        case CullMode::Front:
            return VK_CULL_MODE_FRONT_BIT;
        case CullMode::Back:
            return VK_CULL_MODE_BACK_BIT;
    }

    return VK_CULL_MODE_BACK_BIT;
}

VkFrontFace ToVkFrontFace(FrontFace frontFace)
{
    // The Vulkan backend applies a negative-height viewport to align the public
    // render policy with Metal and with the engine's logical clip-space
    // convention (+Y is up / top in renderer-facing math). That viewport flip
    // also inverts framebuffer-space winding, so front-face selection must be
    // inverted here to preserve the public RasterState contract.
    return frontFace == FrontFace::CW ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
}

VkPolygonMode ToVkPolygonMode(FillMode fillMode)
{
    switch (fillMode)
    {
        case FillMode::Solid:
            return VK_POLYGON_MODE_FILL;
        case FillMode::Wireframe:
            return VK_POLYGON_MODE_LINE;
    }

    return VK_POLYGON_MODE_FILL;
}

VkCompareOp ToVkCompareOp(CompareOp compareOp)
{
    switch (compareOp)
    {
        case CompareOp::Never:
            return VK_COMPARE_OP_NEVER;
        case CompareOp::Less:
            return VK_COMPARE_OP_LESS;
        case CompareOp::Equal:
            return VK_COMPARE_OP_EQUAL;
        case CompareOp::LessEqual:
            return VK_COMPARE_OP_LESS_OR_EQUAL;
        case CompareOp::Greater:
            return VK_COMPARE_OP_GREATER;
        case CompareOp::NotEqual:
            return VK_COMPARE_OP_NOT_EQUAL;
        case CompareOp::GreaterEqual:
            return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case CompareOp::Always:
            return VK_COMPARE_OP_ALWAYS;
    }

    return VK_COMPARE_OP_LESS;
}

VkBlendFactor ToVkBlendFactor(BlendFactor blendFactor)
{
    switch (blendFactor)
    {
        case BlendFactor::Zero:
            return VK_BLEND_FACTOR_ZERO;
        case BlendFactor::One:
            return VK_BLEND_FACTOR_ONE;
        case BlendFactor::SrcColor:
            return VK_BLEND_FACTOR_SRC_COLOR;
        case BlendFactor::OneMinusSrcColor:
            return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case BlendFactor::DstColor:
            return VK_BLEND_FACTOR_DST_COLOR;
        case BlendFactor::OneMinusDstColor:
            return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        case BlendFactor::SrcAlpha:
            return VK_BLEND_FACTOR_SRC_ALPHA;
        case BlendFactor::OneMinusSrcAlpha:
            return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case BlendFactor::DstAlpha:
            return VK_BLEND_FACTOR_DST_ALPHA;
        case BlendFactor::OneMinusDstAlpha:
            return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    }

    return VK_BLEND_FACTOR_ONE;
}

VkBlendOp ToVkBlendOp(BlendOp blendOp)
{
    switch (blendOp)
    {
        case BlendOp::Add:
            return VK_BLEND_OP_ADD;
        case BlendOp::Subtract:
            return VK_BLEND_OP_SUBTRACT;
        case BlendOp::ReverseSubtract:
            return VK_BLEND_OP_REVERSE_SUBTRACT;
        case BlendOp::Min:
            return VK_BLEND_OP_MIN;
        case BlendOp::Max:
            return VK_BLEND_OP_MAX;
    }

    return VK_BLEND_OP_ADD;
}

VkColorComponentFlags ToVkColorWriteMask(uint8_t colorWriteMask)
{
    VkColorComponentFlags flags = 0;
    if ((colorWriteMask & 0x1u) != 0)
        flags |= VK_COLOR_COMPONENT_R_BIT;
    if ((colorWriteMask & 0x2u) != 0)
        flags |= VK_COLOR_COMPONENT_G_BIT;
    if ((colorWriteMask & 0x4u) != 0)
        flags |= VK_COLOR_COMPONENT_B_BIT;
    if ((colorWriteMask & 0x8u) != 0)
        flags |= VK_COLOR_COMPONENT_A_BIT;
    return flags;
}

VkDescriptorType ToVkDescriptorType(ResourceKind resourceKind)
{
    switch (resourceKind)
    {
        case ResourceKind::UniformBuffer:
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case ResourceKind::StorageBuffer:
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case ResourceKind::SampledTexture:
            return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case ResourceKind::StorageTexture:
            return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case ResourceKind::Sampler:
            return VK_DESCRIPTOR_TYPE_SAMPLER;
    }

    return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
}

VkImageAspectFlags ToVkImageAspect(TextureAspect aspect, Format format)
{
    if (aspect == TextureAspect::None)
    {
        if (IsDepthFormat(format))
            aspect =
                HasStencilComponent(format) ? (TextureAspect::Depth | TextureAspect::Stencil) : TextureAspect::Depth;
        else
            aspect = TextureAspect::Color;
    }

    VkImageAspectFlags result = 0;
    if ((aspect & TextureAspect::Color) != TextureAspect::None)
        result |= VK_IMAGE_ASPECT_COLOR_BIT;
    if ((aspect & TextureAspect::Depth) != TextureAspect::None)
        result |= VK_IMAGE_ASPECT_DEPTH_BIT;
    if ((aspect & TextureAspect::Stencil) != TextureAspect::None)
        result |= VK_IMAGE_ASPECT_STENCIL_BIT;
    return result;
}

VmaMemoryUsage ToVmaMemoryUsage(MemoryUsage memoryUsage)
{
    switch (memoryUsage)
    {
        case MemoryUsage::GpuOnly:
            return VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        case MemoryUsage::CpuToGpu:
        case MemoryUsage::GpuToCpu:
            return VMA_MEMORY_USAGE_AUTO;
    }

    return VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
}

VmaAllocationCreateFlags ToVmaAllocationCreateFlags(MemoryUsage memoryUsage)
{
    switch (memoryUsage)
    {
        case MemoryUsage::GpuOnly:
            return 0;
        case MemoryUsage::CpuToGpu:
            return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        case MemoryUsage::GpuToCpu:
            return VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    }

    return 0;
}

VkIndexType ToVkIndexType(IndexType indexType)
{
    switch (indexType)
    {
        case IndexType::UInt16:
            return VK_INDEX_TYPE_UINT16;
        case IndexType::UInt32:
            return VK_INDEX_TYPE_UINT32;
    }

    return VK_INDEX_TYPE_UINT32;
}

bool HasDebugName(const char* debugName)
{
    return debugName != nullptr && debugName[0] != '\0';
}

void SetVulkanDebugName(VkDevice device, VkObjectType objectType, uint64_t objectHandle, const char* debugName)
{
    if (device == VK_NULL_HANDLE || objectHandle == 0 || !HasDebugName(debugName) ||
        vkSetDebugUtilsObjectNameEXT == nullptr)
        return;

    VkDebugUtilsObjectNameInfoEXT nameInfo =
        MakeVkStruct<VkDebugUtilsObjectNameInfoEXT, VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT>();
    nameInfo.objectType = objectType;
    nameInfo.objectHandle = objectHandle;
    nameInfo.pObjectName = debugName;
    CheckVk(vkSetDebugUtilsObjectNameEXT(device, &nameInfo), "vkSetDebugUtilsObjectNameEXT");
}

std::string MakeTextureViewDebugName(const Texture& texture)
{
    const char* debugName = texture.GetDesc().m_DebugName;
    if (!HasDebugName(debugName))
        return {};

    return std::string(debugName) + ".View";
}
} // namespace VulkanRHI
