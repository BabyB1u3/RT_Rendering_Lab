#include "Render/RHI/Backends/Vulkan/VulkanDevice.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <string>
#include <vector>

#include "Core/Diagnostics/Assert/Assert.h"

#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#if defined(GLAB_GLFW_X11_NATIVE)
#include <X11/Xlib.h>
#ifdef None
#undef None
#endif
#ifdef Always
#undef Always
#endif
#endif
#if defined(GLAB_GLFW_WAYLAND_NATIVE)
#include <wayland-client.h>
#endif
#endif

class VulkanSwapchainTexture final : public Texture
{
public:
    VulkanSwapchainTexture(VkImage image, const TextureDesc& desc) : m_Image(image), m_Desc(desc) {}

    const TextureDesc& GetDesc() const override { return m_Desc; }
    VkImage GetVkImage() const { return m_Image; }
    VkImageLayout GetCurrentLayout() const { return m_CurrentLayout; }
    void SetCurrentLayout(VkImageLayout layout) { m_CurrentLayout = layout; }

private:
    VkImage m_Image = VK_NULL_HANDLE;
    TextureDesc m_Desc;
    VkImageLayout m_CurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
};

class VulkanSwapchainImageView final : public TextureView
{
public:
    VulkanSwapchainImageView(VkDevice device, Texture* texture, VkImageView imageView, const TextureViewDesc& desc)
        : m_Device(device), m_Texture(texture), m_ImageView(imageView), m_Desc(desc)
    {
    }

    ~VulkanSwapchainImageView() override
    {
        if (m_Device != VK_NULL_HANDLE && m_ImageView != VK_NULL_HANDLE)
            vkDestroyImageView(m_Device, m_ImageView, nullptr);
    }

    Texture* GetTexture() const override { return m_Texture; }
    const TextureViewDesc& GetDesc() const override { return m_Desc; }
    VkImageView GetVkImageView() const { return m_ImageView; }

private:
    VkDevice m_Device = VK_NULL_HANDLE;
    Texture* m_Texture = nullptr;
    VkImageView m_ImageView = VK_NULL_HANDLE;
    TextureViewDesc m_Desc;
};

class VulkanBuffer final : public Buffer
{
public:
    VulkanBuffer(VkDevice device, VkBuffer buffer, VkDeviceMemory memory, const BufferDesc& desc)
        : m_Device(device), m_Buffer(buffer), m_Memory(memory), m_Desc(desc)
    {
    }

    ~VulkanBuffer() override
    {
        if (m_Device != VK_NULL_HANDLE && m_Buffer != VK_NULL_HANDLE)
            vkDestroyBuffer(m_Device, m_Buffer, nullptr);
        if (m_Device != VK_NULL_HANDLE && m_Memory != VK_NULL_HANDLE)
            vkFreeMemory(m_Device, m_Memory, nullptr);
    }

    const BufferDesc& GetDesc() const override { return m_Desc; }
    VkBuffer GetVkBuffer() const { return m_Buffer; }
    VkDeviceMemory GetVkMemory() const { return m_Memory; }

private:
    VkDevice m_Device = VK_NULL_HANDLE;
    VkBuffer m_Buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_Memory = VK_NULL_HANDLE;
    BufferDesc m_Desc;
};

class VulkanTexture final : public Texture
{
public:
    VulkanTexture(VkDevice device, VkImage image, VkDeviceMemory memory, const TextureDesc& desc)
        : m_Device(device), m_Image(image), m_Memory(memory), m_Desc(desc)
    {
    }

    ~VulkanTexture() override
    {
        if (m_Device != VK_NULL_HANDLE && m_Image != VK_NULL_HANDLE)
            vkDestroyImage(m_Device, m_Image, nullptr);
        if (m_Device != VK_NULL_HANDLE && m_Memory != VK_NULL_HANDLE)
            vkFreeMemory(m_Device, m_Memory, nullptr);
    }

    const TextureDesc& GetDesc() const override { return m_Desc; }
    VkImage GetVkImage() const { return m_Image; }

private:
    VkDevice m_Device = VK_NULL_HANDLE;
    VkImage m_Image = VK_NULL_HANDLE;
    VkDeviceMemory m_Memory = VK_NULL_HANDLE;
    TextureDesc m_Desc;
};

class VulkanTextureView final : public TextureView
{
public:
    VulkanTextureView(VkDevice device, Texture* texture, VkImageView imageView, const TextureViewDesc& desc)
        : m_Device(device), m_Texture(texture), m_ImageView(imageView), m_Desc(desc)
    {
    }

    ~VulkanTextureView() override
    {
        if (m_Device != VK_NULL_HANDLE && m_ImageView != VK_NULL_HANDLE)
            vkDestroyImageView(m_Device, m_ImageView, nullptr);
    }

    Texture* GetTexture() const override { return m_Texture; }
    const TextureViewDesc& GetDesc() const override { return m_Desc; }
    VkImageView GetVkImageView() const { return m_ImageView; }

private:
    VkDevice m_Device = VK_NULL_HANDLE;
    Texture* m_Texture = nullptr;
    VkImageView m_ImageView = VK_NULL_HANDLE;
    TextureViewDesc m_Desc;
};

class VulkanSampler final : public Sampler
{
public:
    VulkanSampler(VkDevice device, VkSampler sampler, const SamplerDesc& desc)
        : m_Device(device), m_Sampler(sampler), m_Desc(desc)
    {
    }

    ~VulkanSampler() override
    {
        if (m_Device != VK_NULL_HANDLE && m_Sampler != VK_NULL_HANDLE)
            vkDestroySampler(m_Device, m_Sampler, nullptr);
    }

    const SamplerDesc& GetDesc() const override { return m_Desc; }
    VkSampler GetVkSampler() const { return m_Sampler; }

private:
    VkDevice m_Device = VK_NULL_HANDLE;
    VkSampler m_Sampler = VK_NULL_HANDLE;
    SamplerDesc m_Desc;
};

class VulkanShaderProgram final : public ShaderProgram
{
public:
    struct StageModule
    {
        ShaderStage m_Stage = ShaderStage::None;
        VkShaderModule m_Module = VK_NULL_HANDLE;
    };

    VulkanShaderProgram(VkDevice device, const CompiledShaderProgramDesc& desc, std::vector<StageModule>&& modules)
        : m_Device(device), m_Reflection(desc.m_Reflection), m_Modules(std::move(modules))
    {
    }

    ~VulkanShaderProgram() override
    {
        if (m_Device == VK_NULL_HANDLE)
            return;

        for (const StageModule& module : m_Modules)
        {
            if (module.m_Module != VK_NULL_HANDLE)
                vkDestroyShaderModule(m_Device, module.m_Module, nullptr);
        }
    }

    const ShaderReflectionData& GetReflection() const override { return m_Reflection; }
    PipelineLayoutDesc DerivePipelineLayoutDesc() const override
    {
        return RHIInternal::BuildPipelineLayoutDescFromReflection(m_Reflection);
    }

    const StageModule* FindStage(ShaderStage stage) const
    {
        const auto it = std::find_if(
            m_Modules.begin(), m_Modules.end(), [stage](const StageModule& module) { return module.m_Stage == stage; });
        return it != m_Modules.end() ? &(*it) : nullptr;
    }

private:
    VkDevice m_Device = VK_NULL_HANDLE;
    ShaderReflectionData m_Reflection;
    std::vector<StageModule> m_Modules;
};

class VulkanVertexInputLayout final : public VertexInputLayout
{
public:
    VulkanVertexInputLayout(const VertexInputLayoutDesc& desc,
                            std::vector<VkVertexInputBindingDescription>&& bindings,
                            std::vector<VkVertexInputAttributeDescription>&& attributes)
        : m_Desc(desc), m_Bindings(std::move(bindings)), m_Attributes(std::move(attributes))
    {
    }

    const VertexInputLayoutDesc& GetDesc() const override { return m_Desc; }
    const std::vector<VkVertexInputBindingDescription>& GetVkBindings() const { return m_Bindings; }
    const std::vector<VkVertexInputAttributeDescription>& GetVkAttributes() const { return m_Attributes; }

private:
    VertexInputLayoutDesc m_Desc;
    std::vector<VkVertexInputBindingDescription> m_Bindings;
    std::vector<VkVertexInputAttributeDescription> m_Attributes;
};

class VulkanGraphicsPipeline final : public GraphicsPipeline
{
public:
    VulkanGraphicsPipeline(VkDevice device,
                           const GraphicsPipelineDesc& desc,
                           VkPipelineLayout pipelineLayout,
                           std::vector<VkDescriptorSetLayout>&& descriptorSetLayouts,
                           VkPipeline pipeline)
        : m_Device(device),
          m_Desc(desc),
          m_PipelineLayout(pipelineLayout),
          m_DescriptorSetLayouts(std::move(descriptorSetLayouts)),
          m_Pipeline(pipeline)
    {
    }

    ~VulkanGraphicsPipeline() override
    {
        if (m_Device == VK_NULL_HANDLE)
            return;

        if (m_Pipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(m_Device, m_Pipeline, nullptr);

        for (VkDescriptorSetLayout descriptorSetLayout : m_DescriptorSetLayouts)
        {
            if (descriptorSetLayout != VK_NULL_HANDLE)
                vkDestroyDescriptorSetLayout(m_Device, descriptorSetLayout, nullptr);
        }

        if (m_PipelineLayout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);
    }

    const GraphicsPipelineDesc& GetDesc() const override { return m_Desc; }
    VkPipeline GetVkPipeline() const { return m_Pipeline; }
    VkPipelineLayout GetVkPipelineLayout() const { return m_PipelineLayout; }

private:
    VkDevice m_Device = VK_NULL_HANDLE;
    GraphicsPipelineDesc m_Desc;
    VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSetLayout> m_DescriptorSetLayouts;
    VkPipeline m_Pipeline = VK_NULL_HANDLE;
};

namespace
{

void CheckVk(VkResult result, const char* what)
{
    RTRLAB_ASSERTF(result == VK_SUCCESS, "{} failed with VkResult={}", what, static_cast<int>(result));
}

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

bool HasDebugName(const char* debugName)
{
    return debugName != nullptr && debugName[0] != '\0';
}

void SetVulkanDebugName(VkDevice device, VkObjectType objectType, uint64_t objectHandle, const char* debugName)
{
    if (device == VK_NULL_HANDLE || objectHandle == 0 || !HasDebugName(debugName) ||
        vkSetDebugUtilsObjectNameEXT == nullptr)
        return;

    VkDebugUtilsObjectNameInfoEXT nameInfo{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
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
    return frontFace == FrontFace::CW ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
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

VkMemoryPropertyFlags GetRequiredMemoryProperties(MemoryUsage memoryUsage)
{
    switch (memoryUsage)
    {
        case MemoryUsage::GpuOnly:
            return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        case MemoryUsage::CpuToGpu:
            return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        case MemoryUsage::GpuToCpu:
            return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    }

    return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
}

VkMemoryPropertyFlags GetFallbackMemoryProperties(MemoryUsage memoryUsage)
{
    switch (memoryUsage)
    {
        case MemoryUsage::GpuOnly:
            return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        case MemoryUsage::CpuToGpu:
            return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        case MemoryUsage::GpuToCpu:
            return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    }

    return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
}

uint32_t FindMemoryType(VkPhysicalDevice physicalDevice,
                        uint32_t memoryTypeBits,
                        VkMemoryPropertyFlags requiredProperties,
                        VkMemoryPropertyFlags fallbackProperties)
{
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

    const auto matches = [memoryTypeBits, &memoryProperties](uint32_t index, VkMemoryPropertyFlags properties)
    {
        return ((memoryTypeBits & (1u << index)) != 0) &&
               ((memoryProperties.memoryTypes[index].propertyFlags & properties) == properties);
    };

    for (uint32_t index = 0; index < memoryProperties.memoryTypeCount; ++index)
    {
        if (matches(index, requiredProperties))
            return index;
    }

    for (uint32_t index = 0; index < memoryProperties.memoryTypeCount; ++index)
    {
        if (matches(index, fallbackProperties))
            return index;
    }

    RTRLAB_ASSERT_MSG(false, "Failed to find a compatible Vulkan memory type.");
    return 0;
}

const VulkanShaderProgram& GetVulkanShaderProgram(ShaderProgram* shaderProgram)
{
    auto* vulkanShaderProgram = dynamic_cast<VulkanShaderProgram*>(shaderProgram);
    RTRLAB_ASSERT_MSG(vulkanShaderProgram != nullptr, "GraphicsPipeline requires a Vulkan shader program.");
    return *vulkanShaderProgram;
}

const VulkanVertexInputLayout& GetVulkanVertexInputLayout(VertexInputLayout* vertexInputLayout)
{
    auto* vulkanVertexInputLayout = dynamic_cast<VulkanVertexInputLayout*>(vertexInputLayout);
    RTRLAB_ASSERT_MSG(vulkanVertexInputLayout != nullptr, "GraphicsPipeline requires a Vulkan vertex input layout.");
    return *vulkanVertexInputLayout;
}

const VulkanGraphicsPipeline& GetVulkanGraphicsPipeline(GraphicsPipeline* graphicsPipeline)
{
    auto* vulkanGraphicsPipeline = dynamic_cast<VulkanGraphicsPipeline*>(graphicsPipeline);
    RTRLAB_ASSERT_MSG(vulkanGraphicsPipeline != nullptr, "Graphics pipeline is not owned by the Vulkan backend.");
    return *vulkanGraphicsPipeline;
}

VulkanBuffer& GetVulkanBuffer(Buffer* buffer)
{
    auto* vulkanBuffer = dynamic_cast<VulkanBuffer*>(buffer);
    RTRLAB_ASSERT_MSG(vulkanBuffer != nullptr, "Buffer is not owned by the Vulkan backend.");
    return *vulkanBuffer;
}

VkBuffer GetVkBufferFromBuffer(Buffer* buffer)
{
    return GetVulkanBuffer(buffer).GetVkBuffer();
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

std::vector<VkDescriptorSetLayout> CreateVkDescriptorSetLayouts(VkDevice device, const PipelineLayoutDesc& desc)
{
    uint32_t maxSetIndex = 0;
    bool hasBindings = false;
    for (const BindingInfo& binding : desc.m_Bindings)
    {
        maxSetIndex = std::max(maxSetIndex, binding.m_SetIndex);
        hasBindings = true;
    }

    if (!hasBindings)
        return {};

    std::vector<std::vector<VkDescriptorSetLayoutBinding>> bindingsPerSet(maxSetIndex + 1);
    for (const BindingInfo& binding : desc.m_Bindings)
    {
        VkDescriptorSetLayoutBinding vkBinding{};
        vkBinding.binding = binding.m_Binding;
        vkBinding.descriptorType = ToVkDescriptorType(binding.m_Kind);
        vkBinding.descriptorCount = std::max(binding.m_ArrayCount, 1u);
        vkBinding.stageFlags = ToVkShaderStageFlags(binding.m_StageMask);
        bindingsPerSet[binding.m_SetIndex].push_back(vkBinding);
    }

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts(bindingsPerSet.size(), VK_NULL_HANDLE);
    for (size_t setIndex = 0; setIndex < bindingsPerSet.size(); ++setIndex)
    {
        VkDescriptorSetLayoutCreateInfo createInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        createInfo.bindingCount = static_cast<uint32_t>(bindingsPerSet[setIndex].size());
        createInfo.pBindings = bindingsPerSet[setIndex].empty() ? nullptr : bindingsPerSet[setIndex].data();
        CheckVk(vkCreateDescriptorSetLayout(device, &createInfo, nullptr, &descriptorSetLayouts[setIndex]),
                "vkCreateDescriptorSetLayout");
    }

    return descriptorSetLayouts;
}

VkPipelineLayout CreateVkPipelineLayout(VkDevice device,
                                        const PipelineLayoutDesc& desc,
                                        const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts)
{
    std::vector<VkPushConstantRange> pushConstantRanges;
    pushConstantRanges.reserve(desc.m_PushConstants.size());
    for (const PushConstantRangeDesc& pushConstant : desc.m_PushConstants)
    {
        VkPushConstantRange vkPushConstantRange{};
        vkPushConstantRange.stageFlags = ToVkShaderStageFlags(pushConstant.m_StageMask);
        vkPushConstantRange.offset = pushConstant.m_Offset;
        vkPushConstantRange.size = pushConstant.m_Size;
        pushConstantRanges.push_back(vkPushConstantRange);
    }

    VkPipelineLayoutCreateInfo createInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    createInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    createInfo.pSetLayouts = descriptorSetLayouts.data();
    createInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
    createInfo.pPushConstantRanges = pushConstantRanges.data();

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    CheckVk(vkCreatePipelineLayout(device, &createInfo, nullptr, &pipelineLayout), "vkCreatePipelineLayout");
    return pipelineLayout;
}

constexpr const char* kVulkanShaderEntryPoint = "main";

VkImage GetVkImageFromOwnedTexture(Texture* texture)
{
    if (auto* ownedTexture = dynamic_cast<VulkanTexture*>(texture))
        return ownedTexture->GetVkImage();

    RTRLAB_ASSERT_MSG(false,
                      "Vulkan CreateTextureView only accepts device-created textures. "
                      "Swapchain images expose views via Swapchain::GetImageView().");
    return VK_NULL_HANDLE;
}

std::vector<const char*> GetPlatformInstanceExtensionCandidates()
{
    std::vector<const char*> extensions = {VK_KHR_SURFACE_EXTENSION_NAME, VK_EXT_DEBUG_UTILS_EXTENSION_NAME};

#if defined(_WIN32)
    extensions.push_back("VK_KHR_win32_surface");
#elif defined(__APPLE__)
    extensions.push_back("VK_EXT_metal_surface");
#elif defined(__linux__)
#if defined(VK_USE_PLATFORM_XLIB_KHR)
    extensions.push_back("VK_KHR_xlib_surface");
#endif
#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
    extensions.push_back("VK_KHR_wayland_surface");
#endif
#endif

    return extensions;
}

std::vector<const char*> GetSupportedInstanceExtensions()
{
    uint32_t extensionCount = 0;
    CheckVk(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr),
            "vkEnumerateInstanceExtensionProperties(count)");

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    CheckVk(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data()),
            "vkEnumerateInstanceExtensionProperties(list)");

    std::vector<const char*> enabledExtensions;
    for (const char* candidate : GetPlatformInstanceExtensionCandidates())
    {
        const auto it = std::find_if(availableExtensions.begin(),
                                     availableExtensions.end(),
                                     [candidate](const VkExtensionProperties& extension)
                                     { return std::strcmp(extension.extensionName, candidate) == 0; });
        if (it != availableExtensions.end())
            enabledExtensions.push_back(candidate);
    }

    const auto hasEnabledExtension = [&enabledExtensions](const char* extensionName)
    {
        return std::find_if(enabledExtensions.begin(),
                            enabledExtensions.end(),
                            [extensionName](const char* enabledExtension)
                            { return std::strcmp(enabledExtension, extensionName) == 0; }) != enabledExtensions.end();
    };

    const bool hasSurfaceExtension = hasEnabledExtension(VK_KHR_SURFACE_EXTENSION_NAME);
    RTRLAB_ASSERT_MSG(hasSurfaceExtension, "Vulkan instance creation requires VK_KHR_surface support.");
    return enabledExtensions;
}

VkSurfaceKHR CreateSurface(VkInstance instance, const NativeWindowHandle& nativeWindowHandle)
{
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    switch (nativeWindowHandle.m_System)
    {
        case NativeWindowSystem::Win32:
        {
#if defined(_WIN32)
            VkWin32SurfaceCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
            createInfo.hinstance = GetModuleHandleW(nullptr);
            createInfo.hwnd = reinterpret_cast<HWND>(nativeWindowHandle.m_Window);
            CheckVk(vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, &surface), "vkCreateWin32SurfaceKHR");
#else
            RTRLAB_ASSERT_MSG(false, "Win32 Vulkan surface creation is unavailable on this platform.");
#endif
            break;
        }
        case NativeWindowSystem::Cocoa:
            RTRLAB_ASSERT_MSG(false, "Cocoa Vulkan surface creation is not compiled in this backend build.");
            break;
        case NativeWindowSystem::Xlib:
        {
#if defined(VK_USE_PLATFORM_XLIB_KHR)
            VkXlibSurfaceCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR};
            createInfo.dpy = static_cast<Display*>(nativeWindowHandle.m_Display);
            createInfo.window = static_cast<::Window>(nativeWindowHandle.m_Window);
            CheckVk(vkCreateXlibSurfaceKHR(instance, &createInfo, nullptr, &surface), "vkCreateXlibSurfaceKHR");
#else
            RTRLAB_ASSERT_MSG(false, "Xlib Vulkan surface creation is unavailable in this build.");
#endif
            break;
        }
        case NativeWindowSystem::Xcb:
            RTRLAB_ASSERT_MSG(false, "XCB native window handles are not currently produced by the platform layer.");
            break;
        case NativeWindowSystem::Wayland:
        {
#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
            VkWaylandSurfaceCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR};
            createInfo.display = static_cast<wl_display*>(nativeWindowHandle.m_Display);
            createInfo.surface = reinterpret_cast<wl_surface*>(nativeWindowHandle.m_Window);
            CheckVk(vkCreateWaylandSurfaceKHR(instance, &createInfo, nullptr, &surface), "vkCreateWaylandSurfaceKHR");
#else
            RTRLAB_ASSERT_MSG(false, "Wayland Vulkan surface creation is unavailable in this build.");
#endif
            break;
        }
    }

    return surface;
}

struct QueueFamilySelection
{
    uint32_t m_GraphicsFamily = std::numeric_limits<uint32_t>::max();
    uint32_t m_PresentFamily = std::numeric_limits<uint32_t>::max();

    bool IsComplete() const
    {
        return m_GraphicsFamily != std::numeric_limits<uint32_t>::max() &&
               m_PresentFamily != std::numeric_limits<uint32_t>::max();
    }
};

QueueFamilySelection FindQueueFamilies(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
    uint32_t familyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, nullptr);

    std::vector<VkQueueFamilyProperties> familyProperties(familyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, familyProperties.data());

    QueueFamilySelection selection{};

    for (uint32_t familyIndex = 0; familyIndex < familyCount; ++familyIndex)
    {
        const VkQueueFamilyProperties& properties = familyProperties[familyIndex];
        if ((properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
            selection.m_GraphicsFamily = familyIndex;

        VkBool32 presentSupported = VK_FALSE;
        CheckVk(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, familyIndex, surface, &presentSupported),
                "vkGetPhysicalDeviceSurfaceSupportKHR");
        if (presentSupported == VK_TRUE)
            selection.m_PresentFamily = familyIndex;

        if (selection.IsComplete())
            break;
    }

    return selection;
}

uint32_t FindGraphicsQueueFamily(VkPhysicalDevice physicalDevice)
{
    uint32_t familyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, nullptr);

    std::vector<VkQueueFamilyProperties> familyProperties(familyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, familyProperties.data());

    for (uint32_t familyIndex = 0; familyIndex < familyCount; ++familyIndex)
    {
        if ((familyProperties[familyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
            return familyIndex;
    }

    RTRLAB_ASSERT_MSG(false, "Failed to find a Vulkan graphics queue family.");
    return std::numeric_limits<uint32_t>::max();
}

bool SupportsPresentOnQueueFamily(VkPhysicalDevice physicalDevice, uint32_t queueFamily, VkSurfaceKHR surface)
{
    VkBool32 presentSupported = VK_FALSE;
    CheckVk(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamily, surface, &presentSupported),
            "vkGetPhysicalDeviceSurfaceSupportKHR");
    return presentSupported == VK_TRUE;
}

bool SupportsRequiredDeviceExtensions(VkPhysicalDevice physicalDevice)
{
    uint32_t extensionCount = 0;
    CheckVk(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr),
            "vkEnumerateDeviceExtensionProperties(count)");

    std::vector<VkExtensionProperties> extensions(extensionCount);
    CheckVk(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, extensions.data()),
            "vkEnumerateDeviceExtensionProperties(list)");

    for (const VkExtensionProperties& extension : extensions)
    {
        if (std::strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
            return true;
    }

    return false;
}

VkPhysicalDevice PickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface, QueueFamilySelection& selection)
{
    uint32_t deviceCount = 0;
    CheckVk(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr), "vkEnumeratePhysicalDevices(count)");
    RTRLAB_ASSERT_MSG(deviceCount > 0, "No Vulkan physical devices are available.");

    std::vector<VkPhysicalDevice> devices(deviceCount);
    CheckVk(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()), "vkEnumeratePhysicalDevices(list)");

    for (VkPhysicalDevice device : devices)
    {
        if (!SupportsRequiredDeviceExtensions(device))
            continue;

        QueueFamilySelection candidateSelection = FindQueueFamilies(device, surface);
        if (!candidateSelection.IsComplete())
            continue;

        selection = candidateSelection;
        return device;
    }

    RTRLAB_ASSERT_MSG(false, "Failed to find a Vulkan physical device with graphics, present, and swapchain support.");
    return VK_NULL_HANDLE;
}

VkPhysicalDevice PickPhysicalDevice(VkInstance instance, uint32_t& graphicsQueueFamily)
{
    uint32_t deviceCount = 0;
    CheckVk(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr), "vkEnumeratePhysicalDevices(count)");
    RTRLAB_ASSERT_MSG(deviceCount > 0, "No Vulkan physical devices are available.");

    std::vector<VkPhysicalDevice> devices(deviceCount);
    CheckVk(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()), "vkEnumeratePhysicalDevices(list)");

    for (VkPhysicalDevice device : devices)
    {
        if (!SupportsRequiredDeviceExtensions(device))
            continue;

        graphicsQueueFamily = FindGraphicsQueueFamily(device);
        if (graphicsQueueFamily == std::numeric_limits<uint32_t>::max())
            continue;

        return device;
    }

    RTRLAB_ASSERT_MSG(false, "Failed to find a Vulkan physical device with graphics and swapchain support.");
    return VK_NULL_HANDLE;
}

VkSurfaceFormatKHR ChooseSurfaceFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, Format preferredFormat)
{
    uint32_t formatCount = 0;
    CheckVk(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr),
            "vkGetPhysicalDeviceSurfaceFormatsKHR(count)");
    RTRLAB_ASSERT_MSG(formatCount > 0, "Vulkan surface reports no supported formats.");

    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    CheckVk(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data()),
            "vkGetPhysicalDeviceSurfaceFormatsKHR(list)");

    const VkFormat preferredVkFormat = ToVkFormat(preferredFormat);
    for (const VkSurfaceFormatKHR& surfaceFormat : formats)
    {
        if (surfaceFormat.format == preferredVkFormat)
            return surfaceFormat;
    }

    return formats.front();
}

VkPresentModeKHR ChoosePresentMode(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, bool vsync)
{
    uint32_t modeCount = 0;
    CheckVk(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &modeCount, nullptr),
            "vkGetPhysicalDeviceSurfacePresentModesKHR(count)");
    RTRLAB_ASSERT_MSG(modeCount > 0, "Vulkan surface reports no present modes.");

    std::vector<VkPresentModeKHR> presentModes(modeCount);
    CheckVk(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &modeCount, presentModes.data()),
            "vkGetPhysicalDeviceSurfacePresentModesKHR(list)");

    if (vsync)
        return VK_PRESENT_MODE_FIFO_KHR;

    for (VkPresentModeKHR presentMode : presentModes)
    {
        if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            return presentMode;
    }

    for (VkPresentModeKHR presentMode : presentModes)
    {
        if (presentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
            return presentMode;
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D
ChooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t requestedWidth, uint32_t requestedHeight)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        return capabilities.currentExtent;

    VkExtent2D extent{};
    extent.width = std::clamp(requestedWidth, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    extent.height = std::clamp(requestedHeight, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    return extent;
}

void TransitionImageLayout(VkCommandBuffer commandBuffer,
                           VkImage image,
                           VkImageLayout oldLayout,
                           VkImageLayout newLayout,
                           VkPipelineStageFlags srcStageMask,
                           VkPipelineStageFlags dstStageMask,
                           VkAccessFlags srcAccessMask,
                           VkAccessFlags dstAccessMask)
{
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcAccessMask = srcAccessMask;
    barrier.dstAccessMask = dstAccessMask;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

VkPipelineStageFlags SourceStageForLayout(VkImageLayout layout)
{
    switch (layout)
    {
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        case VK_IMAGE_LAYOUT_UNDEFINED:
        default:
            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }
}

VkAccessFlags SourceAccessForLayout(VkImageLayout layout)
{
    switch (layout)
    {
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        case VK_IMAGE_LAYOUT_UNDEFINED:
        default:
            return 0;
    }
}
} // namespace

VulkanCommandList::~VulkanCommandList()
{
    Shutdown();
}

void VulkanCommandList::Initialize(VkDevice device, VkCommandPool commandPool)
{
    Shutdown();

    m_Device = device;
    m_CommandPool = commandPool;

    VkCommandBufferAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocateInfo.commandPool = commandPool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;
    CheckVk(vkAllocateCommandBuffers(device, &allocateInfo, &m_CommandBuffer), "vkAllocateCommandBuffers");
}

void VulkanCommandList::Shutdown()
{
    // Idempotent: VulkanDevice tears the command list down explicitly before member destruction.
    if (m_Device != VK_NULL_HANDLE && m_CommandBuffer != VK_NULL_HANDLE)
    {
        vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &m_CommandBuffer);
    }

    m_CommandBuffer = VK_NULL_HANDLE;
    m_CommandPool = VK_NULL_HANDLE;
    m_Device = VK_NULL_HANDLE;
}

void VulkanCommandList::BeginRendering(const RenderingInfo& renderingInfo)
{
    ShellCommandListBase::BeginRendering(renderingInfo);

    RTRLAB_ASSERT_MSG(renderingInfo.m_ColorAttachments.size() == 1,
                      "Early Vulkan bring-up currently supports exactly one color attachment.");
    RTRLAB_ASSERT_MSG(renderingInfo.m_DepthAttachment.m_View == nullptr,
                      "Early Vulkan bring-up does not support depth attachments yet.");

    const ColorAttachmentInfo& colorAttachment = renderingInfo.m_ColorAttachments.front();
    auto* imageView = dynamic_cast<VulkanSwapchainImageView*>(colorAttachment.m_View);
    RTRLAB_ASSERT_MSG(imageView != nullptr, "Vulkan BeginRendering currently expects a swapchain image view.");

    auto* texture = dynamic_cast<VulkanSwapchainTexture*>(imageView->GetTexture());
    RTRLAB_ASSERT_MSG(texture != nullptr, "Vulkan BeginRendering currently expects a swapchain texture.");

    TransitionImageLayout(m_CommandBuffer,
                          texture->GetVkImage(),
                          texture->GetCurrentLayout(),
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                          SourceStageForLayout(texture->GetCurrentLayout()),
                          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          SourceAccessForLayout(texture->GetCurrentLayout()),
                          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    texture->SetCurrentLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    const VkClearValue clearValue = {{
        colorAttachment.m_ClearValue.m_R,
        colorAttachment.m_ClearValue.m_G,
        colorAttachment.m_ClearValue.m_B,
        colorAttachment.m_ClearValue.m_A,
    }};

    VkRenderingAttachmentInfo colorAttachmentInfo{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    colorAttachmentInfo.imageView = imageView->GetVkImageView();
    colorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachmentInfo.loadOp = colorAttachment.m_LoadOp == LoadOp::Clear      ? VK_ATTACHMENT_LOAD_OP_CLEAR
                                 : colorAttachment.m_LoadOp == LoadOp::DontCare ? VK_ATTACHMENT_LOAD_OP_DONT_CARE
                                                                                : VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachmentInfo.storeOp = colorAttachment.m_StoreOp == StoreOp::DontCare ? VK_ATTACHMENT_STORE_OP_DONT_CARE
                                                                                 : VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentInfo.clearValue = clearValue;

    VkRenderingInfo vkRenderingInfo{VK_STRUCTURE_TYPE_RENDERING_INFO};
    vkRenderingInfo.renderArea.offset = {renderingInfo.m_RenderArea.m_X, renderingInfo.m_RenderArea.m_Y};
    vkRenderingInfo.renderArea.extent = {renderingInfo.m_RenderArea.m_Width, renderingInfo.m_RenderArea.m_Height};
    vkRenderingInfo.layerCount = 1;
    vkRenderingInfo.colorAttachmentCount = 1;
    vkRenderingInfo.pColorAttachments = &colorAttachmentInfo;

    vkCmdBeginRendering(m_CommandBuffer, &vkRenderingInfo);
    SetViewport(static_cast<float>(renderingInfo.m_RenderArea.m_X),
                static_cast<float>(renderingInfo.m_RenderArea.m_Y),
                static_cast<float>(renderingInfo.m_RenderArea.m_Width),
                static_cast<float>(renderingInfo.m_RenderArea.m_Height),
                0.0f,
                1.0f);
    SetScissor(renderingInfo.m_RenderArea.m_X,
               renderingInfo.m_RenderArea.m_Y,
               renderingInfo.m_RenderArea.m_Width,
               renderingInfo.m_RenderArea.m_Height);
}

void VulkanCommandList::EndRendering()
{
    RTRLAB_ASSERT_MSG(m_IsRendering, "Vulkan EndRendering requires an active rendering scope.");

    const ColorAttachmentInfo& colorAttachment = m_RenderingInfo.m_ColorAttachments.front();
    auto* imageView = dynamic_cast<VulkanSwapchainImageView*>(colorAttachment.m_View);
    RTRLAB_ASSERT_MSG(imageView != nullptr, "Vulkan EndRendering currently expects a swapchain image view.");

    auto* texture = dynamic_cast<VulkanSwapchainTexture*>(imageView->GetTexture());
    RTRLAB_ASSERT_MSG(texture != nullptr, "Vulkan EndRendering currently expects a swapchain texture.");

    vkCmdEndRendering(m_CommandBuffer);

    TransitionImageLayout(m_CommandBuffer,
                          texture->GetVkImage(),
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                          VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                          0);
    texture->SetCurrentLayout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    ShellCommandListBase::EndRendering();
}

void VulkanCommandList::BindGraphicsPipeline(GraphicsPipeline* pipeline)
{
    ShellCommandListBase::BindGraphicsPipeline(pipeline);

    if (pipeline == nullptr)
        return;

    const VulkanGraphicsPipeline& vulkanPipeline = GetVulkanGraphicsPipeline(pipeline);
    vkCmdBindPipeline(m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkanPipeline.GetVkPipeline());
}

void VulkanCommandList::BindMesh(const MeshBinding& meshBinding, const uint64_t* vertexOffsets)
{
    ShellCommandListBase::BindMesh(meshBinding, vertexOffsets);

    if (!meshBinding.m_VertexBuffers.empty())
    {
        BindVertexBuffers(0,
                          meshBinding.m_VertexBuffers.data(),
                          static_cast<uint32_t>(meshBinding.m_VertexBuffers.size()),
                          vertexOffsets);
    }

    m_IndexBuffer = meshBinding.m_IndexBuffer;
    m_IndexOffset = 0;
    m_IndexType = meshBinding.m_IndexType;
    if (meshBinding.m_IndexBuffer != nullptr)
        BindIndexBuffer(meshBinding.m_IndexBuffer, 0, meshBinding.m_IndexType);
}

void VulkanCommandList::BindVertexBuffers(uint32_t firstSlot,
                                          Buffer* const* buffers,
                                          uint32_t count,
                                          const uint64_t* offsets)
{
    ShellCommandListBase::BindVertexBuffers(firstSlot, buffers, count, offsets);

    if (count == 0)
        return;

    std::vector<VkBuffer> vkBuffers(count, VK_NULL_HANDLE);
    std::vector<VkDeviceSize> vkOffsets(count, 0);
    for (uint32_t index = 0; index < count; ++index)
    {
        RTRLAB_ASSERT_MSG(buffers[index] != nullptr, "Vulkan BindVertexBuffers requires non-null buffers.");
        vkBuffers[index] = GetVkBufferFromBuffer(buffers[index]);
        vkOffsets[index] = offsets != nullptr ? offsets[index] : 0;
    }

    vkCmdBindVertexBuffers(m_CommandBuffer, firstSlot, count, vkBuffers.data(), vkOffsets.data());
}

void VulkanCommandList::BindIndexBuffer(Buffer* buffer, uint64_t offset, IndexType indexType)
{
    ShellCommandListBase::BindIndexBuffer(buffer, offset, indexType);

    if (buffer == nullptr)
        return;

    vkCmdBindIndexBuffer(m_CommandBuffer, GetVkBufferFromBuffer(buffer), offset, ToVkIndexType(indexType));
}

void VulkanCommandList::SetViewport(float x, float y, float w, float h, float zmin, float zmax)
{
    ShellCommandListBase::SetViewport(x, y, w, h, zmin, zmax);

    VkViewport viewport{};
    viewport.x = x;
    viewport.y = y;
    viewport.width = w;
    viewport.height = h;
    viewport.minDepth = zmin;
    viewport.maxDepth = zmax;
    vkCmdSetViewport(m_CommandBuffer, 0, 1, &viewport);
}

void VulkanCommandList::SetScissor(int32_t x, int32_t y, uint32_t w, uint32_t h)
{
    ShellCommandListBase::SetScissor(x, y, w, h);

    VkRect2D scissor{};
    scissor.offset = {x, y};
    scissor.extent = {w, h};
    vkCmdSetScissor(m_CommandBuffer, 0, 1, &scissor);
}

void VulkanCommandList::Draw(uint32_t vertexCount, uint32_t firstVertex)
{
    ShellCommandListBase::Draw(vertexCount, firstVertex);
    RTRLAB_ASSERT_MSG(m_IsRendering, "Vulkan Draw requires an active rendering scope.");
    RTRLAB_ASSERT_MSG(m_GraphicsPipeline != nullptr, "Vulkan Draw requires a bound graphics pipeline.");
    vkCmdDraw(m_CommandBuffer, vertexCount, 1, firstVertex, 0);
}

void VulkanCommandList::DrawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset)
{
    ShellCommandListBase::DrawIndexed(indexCount, firstIndex, vertexOffset);
    RTRLAB_ASSERT_MSG(m_IsRendering, "Vulkan DrawIndexed requires an active rendering scope.");
    RTRLAB_ASSERT_MSG(m_GraphicsPipeline != nullptr, "Vulkan DrawIndexed requires a bound graphics pipeline.");
    RTRLAB_ASSERT_MSG(m_IndexBuffer != nullptr, "Vulkan DrawIndexed requires a bound index buffer.");
    vkCmdDrawIndexed(m_CommandBuffer, indexCount, 1, firstIndex, vertexOffset, 0);
}

VulkanSwapchain::VulkanSwapchain(VulkanDevice& device,
                                 const SwapchainDesc& desc,
                                 const NativeWindowHandle& nativeWindowHandle)
    : m_Device(device), m_Desc(RHIInternal::SanitizeSwapchainDesc(desc)), m_NativeWindowHandle(nativeWindowHandle)
{
    RTRLAB_ASSERT_MSG(RHIInternal::IsNativeWindowHandleValid(nativeWindowHandle),
                      "Native window handle is incomplete.");

    RecreateSwapchain();
}

VulkanSwapchain::~VulkanSwapchain()
{
    DestroySwapchain();
}

uint32_t VulkanSwapchain::AcquireNextImage()
{
    uint32_t imageIndex = 0;
    VkResult result = vkAcquireNextImageKHR(m_Device.GetVkDevice(),
                                            m_Swapchain,
                                            std::numeric_limits<uint64_t>::max(),
                                            m_Device.GetCurrentImageAvailableSemaphore(),
                                            VK_NULL_HANDLE,
                                            &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        RecreateSwapchain(m_Swapchain);
        result = vkAcquireNextImageKHR(m_Device.GetVkDevice(),
                                       m_Swapchain,
                                       std::numeric_limits<uint64_t>::max(),
                                       m_Device.GetCurrentImageAvailableSemaphore(),
                                       VK_NULL_HANDLE,
                                       &imageIndex);
    }

    if (result != VK_SUBOPTIMAL_KHR)
        CheckVk(result, "vkAcquireNextImageKHR");
    return imageIndex;
}

Texture* VulkanSwapchain::GetImage(uint32_t imageIndex) const
{
    RTRLAB_ASSERT_MSG(imageIndex < m_Images.size(), "Swapchain image index out of range.");
    return m_Images[imageIndex].get();
}

TextureView* VulkanSwapchain::GetImageView(uint32_t imageIndex) const
{
    RTRLAB_ASSERT_MSG(imageIndex < m_ImageViews.size(), "Swapchain image-view index out of range.");
    return m_ImageViews[imageIndex].get();
}

void VulkanSwapchain::Present(uint32_t imageIndex)
{
    RTRLAB_ASSERT_MSG(imageIndex < m_Images.size(), "Swapchain present index out of range.");

    VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    const VkSemaphore renderFinishedSemaphore = m_Device.GetCurrentRenderFinishedSemaphore();
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_Swapchain;
    presentInfo.pImageIndices = &imageIndex;

    VkResult result = vkQueuePresentKHR(m_Device.GetPresentQueue(), &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        RecreateSwapchain(m_Swapchain);
        m_Device.RecycleCurrentRenderFinishedSemaphore();
        m_Device.AdvanceFrameSync();
        return;
    }

    CheckVk(result, "vkQueuePresentKHR");
    m_Device.AdvanceFrameSync();
}

void VulkanSwapchain::Resize(uint32_t newWidth, uint32_t newHeight)
{
    if (newWidth == 0 || newHeight == 0)
        return;

    m_Desc.m_Width = std::max(newWidth, 1u);
    m_Desc.m_Height = std::max(newHeight, 1u);
    RecreateSwapchain(m_Swapchain);
}

TextureDesc VulkanSwapchain::BuildSwapchainImageDesc() const
{
    TextureDesc textureDesc;
    textureDesc.m_Type = TextureType::Tex2D;
    textureDesc.m_Format = m_Desc.m_Format;
    textureDesc.m_Extent = Extent3D{m_Desc.m_Width, m_Desc.m_Height, 1};
    textureDesc.m_MipLevels = 1;
    textureDesc.m_ArrayLayers = 1;
    textureDesc.m_UsageMask = TextureUsage::RenderTarget;
    textureDesc.m_DebugName = "VulkanSwapchainImage";
    return textureDesc;
}

void VulkanSwapchain::RecreateSwapchain(VkSwapchainKHR oldSwapchain)
{
    const VkPhysicalDevice physicalDevice = m_Device.GetVkPhysicalDevice();
    const VkSurfaceKHR surface = m_Device.GetVkSurface();
    const VkDevice device = m_Device.GetVkDevice();

    if (oldSwapchain != VK_NULL_HANDLE)
        vkDeviceWaitIdle(device);

    VkSurfaceCapabilitiesKHR capabilities{};
    CheckVk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities),
            "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

    const VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(physicalDevice, surface, m_Desc.m_Format);
    const VkPresentModeKHR presentMode = ChoosePresentMode(physicalDevice, surface, m_Desc.m_Vsync);
    const VkExtent2D extent = ChooseSwapchainExtent(capabilities, m_Desc.m_Width, m_Desc.m_Height);

    uint32_t desiredImageCount = std::max(m_Desc.m_ImageCount, capabilities.minImageCount);
    if (capabilities.maxImageCount > 0)
        desiredImageCount = std::min(desiredImageCount, capabilities.maxImageCount);

    VkSwapchainCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    createInfo.surface = surface;
    createInfo.minImageCount = desiredImageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    const uint32_t queueFamilyIndices[] = {m_Device.GetGraphicsQueueFamily(), m_Device.GetPresentQueueFamily()};

    if (m_Device.GetGraphicsQueueFamily() != m_Device.GetPresentQueueFamily())
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = oldSwapchain;

    VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
    CheckVk(vkCreateSwapchainKHR(device, &createInfo, nullptr, &newSwapchain), "vkCreateSwapchainKHR");

    uint32_t swapchainImageCount = 0;
    CheckVk(vkGetSwapchainImagesKHR(device, newSwapchain, &swapchainImageCount, nullptr),
            "vkGetSwapchainImagesKHR(count)");
    RTRLAB_ASSERT_MSG(swapchainImageCount > 0, "Vulkan swapchain reported zero images.");

    std::vector<VkImage> swapchainImages(swapchainImageCount);
    CheckVk(vkGetSwapchainImagesKHR(device, newSwapchain, &swapchainImageCount, swapchainImages.data()),
            "vkGetSwapchainImagesKHR(list)");

    std::vector<Scope<VulkanSwapchainTexture>> images;
    std::vector<Scope<VulkanSwapchainImageView>> imageViews;
    images.reserve(swapchainImageCount);
    imageViews.reserve(swapchainImageCount);

    m_Desc.m_Width = extent.width;
    m_Desc.m_Height = extent.height;
    m_Desc.m_ImageCount = swapchainImageCount;
    m_Desc.m_Format = ToRhiFormat(surfaceFormat.format);
    if (m_Desc.m_Format == Format::Unknown)
        m_Desc.m_Format = Format::BGRA8_UNORM;

    const TextureDesc imageDesc = BuildSwapchainImageDesc();

    for (VkImage image : swapchainImages)
    {
        auto swapchainTexture = CreateScope<VulkanSwapchainTexture>(image, imageDesc);
        SetVulkanDebugName(device, VK_OBJECT_TYPE_IMAGE, reinterpret_cast<uint64_t>(image), imageDesc.m_DebugName);

        VkImageViewCreateInfo viewCreateInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewCreateInfo.image = image;
        viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewCreateInfo.format = surfaceFormat.format;
        viewCreateInfo.components = {VK_COMPONENT_SWIZZLE_IDENTITY,
                                     VK_COMPONENT_SWIZZLE_IDENTITY,
                                     VK_COMPONENT_SWIZZLE_IDENTITY,
                                     VK_COMPONENT_SWIZZLE_IDENTITY};
        viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewCreateInfo.subresourceRange.baseMipLevel = 0;
        viewCreateInfo.subresourceRange.levelCount = 1;
        viewCreateInfo.subresourceRange.baseArrayLayer = 0;
        viewCreateInfo.subresourceRange.layerCount = 1;

        VkImageView imageView = VK_NULL_HANDLE;
        CheckVk(vkCreateImageView(device, &viewCreateInfo, nullptr, &imageView), "vkCreateImageView");
        SetVulkanDebugName(
            device, VK_OBJECT_TYPE_IMAGE_VIEW, reinterpret_cast<uint64_t>(imageView), "VulkanSwapchainImage.View");

        TextureViewDesc viewDesc;
        viewDesc.m_Type = TextureType::Tex2D;
        viewDesc.m_Format = m_Desc.m_Format;
        viewDesc.m_Aspect = TextureAspect::Color;

        imageViews.push_back(
            CreateScope<VulkanSwapchainImageView>(device, swapchainTexture.get(), imageView, viewDesc));
        images.push_back(std::move(swapchainTexture));
    }

    DestroySwapchain();

    m_Swapchain = newSwapchain;
    m_VkFormat = surfaceFormat.format;
    m_Images = std::move(images);
    m_ImageViews = std::move(imageViews);
}

void VulkanSwapchain::DestroySwapchain()
{
    m_ImageViews.clear();
    m_Images.clear();

    if (m_Swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(m_Device.GetVkDevice(), m_Swapchain, nullptr);
        m_Swapchain = VK_NULL_HANDLE;
    }
}

VulkanDevice::VulkanDevice()
{
    CheckVk(volkInitialize(), "volkInitialize");
    InitializeInstance();
}

VulkanDevice::~VulkanDevice()
{
    ShutdownPresentationObjects();
    ShutdownDeviceObjects();

    if (m_Instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(m_Instance, nullptr);
        m_Instance = VK_NULL_HANDLE;
    }
}

Scope<Swapchain> VulkanDevice::CreateSwapchain(const SwapchainDesc& desc, const NativeWindowHandle& nativeWindowHandle)
{
    InitializePresentationObjects(nativeWindowHandle);
    return CreateScope<VulkanSwapchain>(*this, desc, nativeWindowHandle);
}

Scope<Buffer> VulkanDevice::CreateBuffer(const BufferDesc& desc)
{
    // TRANSITIONAL(M3): Vulkan buffers still use raw vkCreateBuffer + vkAllocateMemory
    // in the bring-up path. Once the backend-private VMA layer lands, this becomes
    // vmaCreateBuffer(...); VulkanBuffer will hold a VmaAllocation instead of
    // VkDeviceMemory, and teardown will move to vmaDestroyBuffer(...).
    InitializeDeviceObjects();

    VkBufferCreateInfo createInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    createInfo.size = std::max<uint64_t>(desc.m_Size, 1);
    createInfo.usage = ToVkBufferUsage(desc.m_UsageMask);
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer = VK_NULL_HANDLE;
    CheckVk(vkCreateBuffer(m_Device, &createInfo, nullptr, &buffer), "vkCreateBuffer");

    VkMemoryRequirements memoryRequirements{};
    vkGetBufferMemoryRequirements(m_Device, buffer, &memoryRequirements);

    const VkMemoryPropertyFlags requiredProperties = GetRequiredMemoryProperties(desc.m_MemoryUsage);
    const VkMemoryPropertyFlags fallbackProperties = GetFallbackMemoryProperties(desc.m_MemoryUsage);

    VkMemoryAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocateInfo.allocationSize = memoryRequirements.size;
    allocateInfo.memoryTypeIndex =
        FindMemoryType(m_PhysicalDevice, memoryRequirements.memoryTypeBits, requiredProperties, fallbackProperties);

    VkDeviceMemory memory = VK_NULL_HANDLE;
    CheckVk(vkAllocateMemory(m_Device, &allocateInfo, nullptr, &memory), "vkAllocateMemory(buffer)");
    CheckVk(vkBindBufferMemory(m_Device, buffer, memory, 0), "vkBindBufferMemory");
    SetVulkanDebugName(m_Device, VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(buffer), desc.m_DebugName);

    return CreateScope<VulkanBuffer>(m_Device, buffer, memory, desc);
}

Scope<Texture> VulkanDevice::CreateTexture(const TextureDesc& desc)
{
    // TRANSITIONAL(M3): TextureDesc does not expose residency policy yet, so the
    // bring-up path always creates device-local optimal-tiled images. CPU uploads
    // will go through staging-buffer + vkCmdCopyBufferToImage in the next batch.
    // After the Vulkan memory layer lands, this becomes vmaCreateImage(...);
    // VulkanTexture will hold a VmaAllocation instead of VkDeviceMemory, while
    // still mapping TextureDesc to device-local image allocations by default.
    InitializeDeviceObjects();

    VkImageCreateInfo createInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    createInfo.imageType = ToVkImageType(desc.m_Type);
    createInfo.format = ToVkFormat(desc.m_Format);
    createInfo.extent = VkExtent3D{
        std::max(desc.m_Extent.m_Width, 1u), std::max(desc.m_Extent.m_Height, 1u), std::max(desc.m_Extent.m_Depth, 1u)};
    createInfo.mipLevels = std::max(desc.m_MipLevels, 1u);
    createInfo.arrayLayers = std::max(desc.m_ArrayLayers, 1u);
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    createInfo.usage = ToVkImageUsage(desc.m_UsageMask);
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (desc.m_Type == TextureType::Cube)
    {
        createInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        RTRLAB_ASSERT_MSG((createInfo.arrayLayers % 6u) == 0,
                          "Cube textures require arrayLayers to be a multiple of 6.");
    }

    VkImage image = VK_NULL_HANDLE;
    CheckVk(vkCreateImage(m_Device, &createInfo, nullptr, &image), "vkCreateImage");

    VkMemoryRequirements memoryRequirements{};
    vkGetImageMemoryRequirements(m_Device, image, &memoryRequirements);

    VkMemoryAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocateInfo.allocationSize = memoryRequirements.size;
    allocateInfo.memoryTypeIndex = FindMemoryType(m_PhysicalDevice,
                                                  memoryRequirements.memoryTypeBits,
                                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkDeviceMemory memory = VK_NULL_HANDLE;
    CheckVk(vkAllocateMemory(m_Device, &allocateInfo, nullptr, &memory), "vkAllocateMemory(texture)");
    CheckVk(vkBindImageMemory(m_Device, image, memory, 0), "vkBindImageMemory");
    SetVulkanDebugName(m_Device, VK_OBJECT_TYPE_IMAGE, reinterpret_cast<uint64_t>(image), desc.m_DebugName);

    return CreateScope<VulkanTexture>(m_Device, image, memory, desc);
}

Scope<TextureView> VulkanDevice::CreateTextureView(Texture* texture, const TextureViewDesc& desc)
{
    InitializeDeviceObjects();
    RTRLAB_ASSERT_MSG(texture != nullptr, "Vulkan CreateTextureView requires a valid texture.");

    const TextureDesc& textureDesc = texture->GetDesc();
    const Format viewFormat = desc.m_Format == Format::Unknown ? textureDesc.m_Format : desc.m_Format;
    const uint32_t mipLevelCount =
        desc.m_MipLevelCount == 0 ? std::max(textureDesc.m_MipLevels - desc.m_BaseMipLevel, 1u) : desc.m_MipLevelCount;
    const uint32_t arrayLayerCount = desc.m_ArrayLayerCount == 0
                                         ? std::max(textureDesc.m_ArrayLayers - desc.m_BaseArrayLayer, 1u)
                                         : desc.m_ArrayLayerCount;

    VkImageViewCreateInfo createInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    createInfo.image = GetVkImageFromOwnedTexture(texture);
    createInfo.viewType = ToVkImageViewType(desc.m_Type);
    createInfo.format = ToVkFormat(viewFormat);
    createInfo.subresourceRange.aspectMask = ToVkImageAspect(desc.m_Aspect, viewFormat);
    createInfo.subresourceRange.baseMipLevel = desc.m_BaseMipLevel;
    createInfo.subresourceRange.levelCount = mipLevelCount;
    createInfo.subresourceRange.baseArrayLayer = desc.m_BaseArrayLayer;
    createInfo.subresourceRange.layerCount = arrayLayerCount;

    VkImageView imageView = VK_NULL_HANDLE;
    CheckVk(vkCreateImageView(m_Device, &createInfo, nullptr, &imageView), "vkCreateImageView(texture)");
    const std::string debugName = MakeTextureViewDebugName(*texture);
    SetVulkanDebugName(m_Device, VK_OBJECT_TYPE_IMAGE_VIEW, reinterpret_cast<uint64_t>(imageView), debugName.c_str());

    TextureViewDesc resolvedDesc = desc;
    resolvedDesc.m_Format = viewFormat;
    resolvedDesc.m_MipLevelCount = mipLevelCount;
    resolvedDesc.m_ArrayLayerCount = arrayLayerCount;
    return CreateScope<VulkanTextureView>(m_Device, texture, imageView, resolvedDesc);
}

Scope<Sampler> VulkanDevice::CreateSampler(const SamplerDesc& desc)
{
    InitializeDeviceObjects();

    VkSamplerCreateInfo createInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    createInfo.magFilter = ToVkFilter(desc.m_MagFilter);
    createInfo.minFilter = ToVkFilter(desc.m_MinFilter);
    createInfo.mipmapMode = ToVkMipmapMode(desc.m_MipFilter);
    createInfo.addressModeU = ToVkAddressMode(desc.m_AddressU);
    createInfo.addressModeV = ToVkAddressMode(desc.m_AddressV);
    createInfo.addressModeW = ToVkAddressMode(desc.m_AddressW);
    createInfo.mipLodBias = desc.m_MipLodBias;
    createInfo.anisotropyEnable = desc.m_AnisotropyEnable ? VK_TRUE : VK_FALSE;
    createInfo.maxAnisotropy = std::max(desc.m_MaxAnisotropy, 1.0f);
    // TRANSITIONAL(M4): SamplerDesc does not yet expose compare-op or border-color
    // controls. Vulkan shadow samplers and explicit border colors will be wired
    // through the future shader-system sampler contract instead of this fixed
    // M3 bring-up state.
    createInfo.compareEnable = VK_FALSE;
    createInfo.minLod = desc.m_MinLod;
    createInfo.maxLod = desc.m_MaxLod;
    createInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    createInfo.unnormalizedCoordinates = VK_FALSE;

    VkSampler sampler = VK_NULL_HANDLE;
    CheckVk(vkCreateSampler(m_Device, &createInfo, nullptr, &sampler), "vkCreateSampler");
    return CreateScope<VulkanSampler>(m_Device, sampler, desc);
}

Scope<ShaderProgram> VulkanDevice::CreateShaderProgram(const CompiledShaderProgramDesc& desc)
{
    InitializeDeviceObjects();

    std::vector<VulkanShaderProgram::StageModule> modules;
    modules.reserve(desc.m_Blobs.size());

    // TRANSITIONAL(M4): CompiledShaderBlob does not yet carry per-stage entry-point
    // names, so the Vulkan bring-up path assumes "main" for every stage. Once the
    // shader system forwards real entry-point metadata, pipeline stage creation should
    // consume it instead of this fixed convention.
    for (const CompiledShaderBlob& blob : desc.m_Blobs)
    {
        if (blob.m_Backend != BackendType::Vulkan)
            continue;

        RTRLAB_ASSERT_MSG(!blob.m_Code.empty(), "Vulkan shader blobs must contain SPIR-V bytes.");
        RTRLAB_ASSERT_MSG((blob.m_Code.size() % sizeof(uint32_t)) == 0,
                          "Vulkan shader blobs must contain aligned SPIR-V words.");

        VkShaderModuleCreateInfo createInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        createInfo.codeSize = blob.m_Code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(blob.m_Code.data());

        VkShaderModule shaderModule = VK_NULL_HANDLE;
        CheckVk(vkCreateShaderModule(m_Device, &createInfo, nullptr, &shaderModule), "vkCreateShaderModule");
        modules.push_back({blob.m_Stage, shaderModule});
    }

    RTRLAB_ASSERT_MSG(!modules.empty(), "Vulkan CreateShaderProgram requires at least one Vulkan shader blob.");
    return CreateScope<VulkanShaderProgram>(m_Device, desc, std::move(modules));
}

Scope<VertexInputLayout> VulkanDevice::CreateVertexInputLayout(const VertexInputLayoutDesc& desc)
{
    std::vector<VkVertexInputBindingDescription> bindings;
    bindings.reserve(desc.m_Buffers.size());
    for (uint32_t bindingIndex = 0; bindingIndex < static_cast<uint32_t>(desc.m_Buffers.size()); ++bindingIndex)
    {
        const VertexBufferLayoutDesc& buffer = desc.m_Buffers[bindingIndex];
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = bindingIndex;
        bindingDescription.stride = buffer.m_Stride;
        bindingDescription.inputRate =
            buffer.m_PerInstance ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
        bindings.push_back(bindingDescription);
    }

    std::vector<VkVertexInputAttributeDescription> attributes;
    attributes.reserve(desc.m_Attributes.size());
    for (const VertexAttributeDesc& attribute : desc.m_Attributes)
    {
        RTRLAB_ASSERT_MSG(attribute.m_BufferSlot < desc.m_Buffers.size(),
                          "Vertex attribute buffer slot must reference an existing vertex buffer layout.");

        VkVertexInputAttributeDescription attributeDescription{};
        attributeDescription.location = attribute.m_Location;
        attributeDescription.binding = attribute.m_BufferSlot;
        attributeDescription.format = ToVkFormat(attribute.m_Format);
        attributeDescription.offset = attribute.m_Offset;
        attributes.push_back(attributeDescription);
    }

    return CreateScope<VulkanVertexInputLayout>(desc, std::move(bindings), std::move(attributes));
}

Scope<GraphicsPipeline> VulkanDevice::CreateGraphicsPipeline(const GraphicsPipelineDesc& desc)
{
    InitializeDeviceObjects();

    RTRLAB_ASSERT_MSG(desc.m_PipelineLayout != nullptr, "Vulkan graphics pipelines require a PipelineLayout.");
    RTRLAB_ASSERT_MSG(desc.m_ShaderProgram != nullptr, "Vulkan graphics pipelines require a ShaderProgram.");
    RTRLAB_ASSERT_MSG(desc.m_VertexInput != nullptr, "Vulkan graphics pipelines require a VertexInputLayout.");
    RTRLAB_ASSERT_MSG(!desc.m_ColorFormats.empty() || desc.m_DepthFormat != Format::Unknown,
                      "Vulkan graphics pipelines require at least one render-target format.");

    const VulkanShaderProgram& shaderProgram = GetVulkanShaderProgram(desc.m_ShaderProgram);
    const VulkanVertexInputLayout& vertexInput = GetVulkanVertexInputLayout(desc.m_VertexInput);

    const VulkanShaderProgram::StageModule* vertexStage = shaderProgram.FindStage(ShaderStage::Vertex);
    const VulkanShaderProgram::StageModule* fragmentStage = shaderProgram.FindStage(ShaderStage::Fragment);
    RTRLAB_ASSERT_MSG(vertexStage != nullptr, "Vulkan graphics pipelines require a vertex shader stage.");
    RTRLAB_ASSERT_MSG(fragmentStage != nullptr, "Vulkan graphics pipelines require a fragment shader stage.");

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{};
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vertexStage->m_Module;
    shaderStages[0].pName = kVulkanShaderEntryPoint;
    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = fragmentStage->m_Module;
    shaderStages[1].pName = kVulkanShaderEntryPoint;

    const PipelineLayoutDesc& pipelineLayoutDesc = desc.m_PipelineLayout->GetDesc();
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts =
        CreateVkDescriptorSetLayouts(m_Device, pipelineLayoutDesc);
    VkPipelineLayout pipelineLayout = CreateVkPipelineLayout(m_Device, pipelineLayoutDesc, descriptorSetLayouts);

    VkPipelineVertexInputStateCreateInfo vertexInputState{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInput.GetVkBindings().size());
    vertexInputState.pVertexBindingDescriptions = vertexInput.GetVkBindings().data();
    vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInput.GetVkAttributes().size());
    vertexInputState.pVertexAttributeDescriptions = vertexInput.GetVkAttributes().data();

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssemblyState.topology = ToVkPrimitiveTopology(desc.m_Topology);
    inputAssemblyState.primitiveRestartEnable =
        desc.m_Topology == PrimitiveTopology::TriangleStrip || desc.m_Topology == PrimitiveTopology::LineStrip;

    VkPipelineViewportStateCreateInfo viewportState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizationState{
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizationState.depthClampEnable = desc.m_RasterState.m_DepthClampEnable ? VK_TRUE : VK_FALSE;
    rasterizationState.rasterizerDiscardEnable = VK_FALSE;
    rasterizationState.polygonMode = ToVkPolygonMode(desc.m_RasterState.m_FillMode);
    rasterizationState.cullMode = ToVkCullMode(desc.m_RasterState.m_CullMode);
    rasterizationState.frontFace = ToVkFrontFace(desc.m_RasterState.m_FrontFace);
    rasterizationState.depthBiasEnable = desc.m_RasterState.m_DepthBiasEnable ? VK_TRUE : VK_FALSE;
    rasterizationState.depthBiasConstantFactor = desc.m_RasterState.m_DepthBiasConstant;
    rasterizationState.depthBiasSlopeFactor = desc.m_RasterState.m_DepthBiasSlopeFactor;
    rasterizationState.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampleState{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencilState{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depthStencilState.depthTestEnable = desc.m_DepthStencilState.m_DepthTestEnable ? VK_TRUE : VK_FALSE;
    depthStencilState.depthWriteEnable = desc.m_DepthStencilState.m_DepthWriteEnable ? VK_TRUE : VK_FALSE;
    depthStencilState.depthCompareOp = ToVkCompareOp(desc.m_DepthStencilState.m_DepthCompareOp);

    std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments(desc.m_ColorFormats.size());
    for (VkPipelineColorBlendAttachmentState& colorBlendAttachment : colorBlendAttachments)
    {
        colorBlendAttachment.blendEnable = desc.m_BlendState.m_BlendEnable ? VK_TRUE : VK_FALSE;
        colorBlendAttachment.srcColorBlendFactor = ToVkBlendFactor(desc.m_BlendState.m_SrcColorFactor);
        colorBlendAttachment.dstColorBlendFactor = ToVkBlendFactor(desc.m_BlendState.m_DstColorFactor);
        colorBlendAttachment.colorBlendOp = ToVkBlendOp(desc.m_BlendState.m_ColorBlendOp);
        colorBlendAttachment.srcAlphaBlendFactor = ToVkBlendFactor(desc.m_BlendState.m_SrcAlphaFactor);
        colorBlendAttachment.dstAlphaBlendFactor = ToVkBlendFactor(desc.m_BlendState.m_DstAlphaFactor);
        colorBlendAttachment.alphaBlendOp = ToVkBlendOp(desc.m_BlendState.m_AlphaBlendOp);
        colorBlendAttachment.colorWriteMask = ToVkColorWriteMask(desc.m_BlendState.m_ColorWriteMask);
    }

    VkPipelineColorBlendStateCreateInfo colorBlendState{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorBlendState.attachmentCount = static_cast<uint32_t>(colorBlendAttachments.size());
    colorBlendState.pAttachments = colorBlendAttachments.data();

    const std::array<VkDynamicState, 2> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    std::vector<VkFormat> colorAttachmentFormats;
    colorAttachmentFormats.reserve(desc.m_ColorFormats.size());
    for (Format colorFormat : desc.m_ColorFormats)
        colorAttachmentFormats.push_back(ToVkFormat(colorFormat));

    VkPipelineRenderingCreateInfo renderingInfo{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    renderingInfo.colorAttachmentCount = static_cast<uint32_t>(colorAttachmentFormats.size());
    renderingInfo.pColorAttachmentFormats = colorAttachmentFormats.data();
    renderingInfo.depthAttachmentFormat =
        desc.m_DepthFormat == Format::Unknown ? VK_FORMAT_UNDEFINED : ToVkFormat(desc.m_DepthFormat);
    renderingInfo.stencilAttachmentFormat =
        HasStencilComponent(desc.m_DepthFormat) ? ToVkFormat(desc.m_DepthFormat) : VK_FORMAT_UNDEFINED;

    VkGraphicsPipelineCreateInfo createInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    createInfo.pNext = &renderingInfo;
    createInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    createInfo.pStages = shaderStages.data();
    createInfo.pVertexInputState = &vertexInputState;
    createInfo.pInputAssemblyState = &inputAssemblyState;
    createInfo.pViewportState = &viewportState;
    createInfo.pRasterizationState = &rasterizationState;
    createInfo.pMultisampleState = &multisampleState;
    createInfo.pDepthStencilState = desc.m_DepthFormat == Format::Unknown ? nullptr : &depthStencilState;
    createInfo.pColorBlendState = &colorBlendState;
    createInfo.pDynamicState = &dynamicState;
    createInfo.layout = pipelineLayout;

    VkPipeline pipeline = VK_NULL_HANDLE;
    const VkResult result = vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &pipeline);
    if (result != VK_SUCCESS)
    {
        vkDestroyPipelineLayout(m_Device, pipelineLayout, nullptr);
        for (VkDescriptorSetLayout descriptorSetLayout : descriptorSetLayouts)
        {
            if (descriptorSetLayout != VK_NULL_HANDLE)
                vkDestroyDescriptorSetLayout(m_Device, descriptorSetLayout, nullptr);
        }
        CheckVk(result, "vkCreateGraphicsPipelines");
    }

    return CreateScope<VulkanGraphicsPipeline>(
        m_Device, desc, pipelineLayout, std::move(descriptorSetLayouts), pipeline);
}

void VulkanDevice::WriteBuffer(Buffer* buffer, uint64_t offset, const void* data, uint64_t size)
{
    InitializeDeviceObjects();

    if (size == 0)
        return;

    RTRLAB_ASSERT_MSG(buffer != nullptr, "Vulkan WriteBuffer requires a valid buffer.");
    RTRLAB_ASSERT_MSG(data != nullptr, "Vulkan WriteBuffer requires non-null source data.");

    VulkanBuffer& vulkanBuffer = GetVulkanBuffer(buffer);
    const BufferDesc& desc = vulkanBuffer.GetDesc();
    RTRLAB_ASSERT_MSG(desc.m_MemoryUsage == MemoryUsage::CpuToGpu,
                      "Vulkan WriteBuffer currently requires a CpuToGpu buffer.");
    RTRLAB_ASSERT_MSG(offset + size <= desc.m_Size, "Vulkan WriteBuffer range exceeds the buffer size.");

    void* mappedData = nullptr;
    CheckVk(vkMapMemory(m_Device, vulkanBuffer.GetVkMemory(), offset, size, 0, &mappedData), "vkMapMemory");
    std::memcpy(mappedData, data, static_cast<size_t>(size));
    vkUnmapMemory(m_Device, vulkanBuffer.GetVkMemory());
}

CommandList* VulkanDevice::BeginCommandList()
{
    RTRLAB_ASSERT_MSG(m_HasPresentationObjects,
                      "Vulkan presentation objects must be initialized before command recording.");
    RTRLAB_ASSERT_MSG(m_FrameInProgress, "Vulkan command recording requires an active frame.");
    RTRLAB_ASSERT_MSG(!m_FrameSubmitted, "Vulkan command recording must happen before queue submission.");

    VkCommandBuffer commandBuffer = m_CommandList.GetVkCommandBuffer();
    CheckVk(vkResetCommandBuffer(commandBuffer, 0), "vkResetCommandBuffer");

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    CheckVk(vkBeginCommandBuffer(commandBuffer, &beginInfo), "vkBeginCommandBuffer");

    return &m_CommandList;
}

void VulkanDevice::Submit(CommandList* commandList)
{
    RTRLAB_ASSERT_MSG(m_HasPresentationObjects,
                      "Vulkan presentation objects must be initialized before queue submission.");
    RTRLAB_ASSERT_MSG(m_FrameInProgress, "Vulkan Submit requires an active frame.");
    RTRLAB_ASSERT_MSG(!m_FrameSubmitted, "Vulkan Submit must only happen once per frame in the current bring-up path.");
    RTRLAB_ASSERT_MSG(commandList == &m_CommandList,
                      "VulkanDevice only accepts submissions from its backend command list.");
    RTRLAB_ASSERT_MSG(!m_CommandList.IsRenderingActive(),
                      "Vulkan Submit requires EndRendering before queue submission.");

    const VkCommandBuffer commandBuffer = m_CommandList.GetVkCommandBuffer();
    CheckVk(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer");

    const FrameSync& frameSync = GetCurrentFrameSync();
    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &frameSync.m_ImageAvailable;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &frameSync.m_RenderFinished;

    CheckVk(vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, frameSync.m_InFlightFence), "vkQueueSubmit");
    m_FrameSubmitted = true;
}

FrameContext* VulkanDevice::BeginFrame()
{
    RTRLAB_ASSERT_MSG(m_HasPresentationObjects,
                      "Vulkan presentation objects must be initialized before beginning a frame.");
    RTRLAB_ASSERT_MSG(!m_FrameInProgress, "VulkanDevice does not support nested frame lifetimes.");

    FrameSync& frameSync = GetCurrentFrameSync();
    CheckVk(vkWaitForFences(m_Device, 1, &frameSync.m_InFlightFence, VK_TRUE, std::numeric_limits<uint64_t>::max()),
            "vkWaitForFences");
    CheckVk(vkResetFences(m_Device, 1, &frameSync.m_InFlightFence), "vkResetFences");

    m_FrameInProgress = true;
    m_FrameSubmitted = false;
    return &m_FrameContext;
}

void VulkanDevice::EndFrame(FrameContext* frameContext)
{
    RTRLAB_ASSERT_MSG(frameContext == &m_FrameContext, "VulkanDevice only accepts its backend frame context.");
    RTRLAB_ASSERT_MSG(m_FrameInProgress, "Vulkan EndFrame requires an active frame.");
    RTRLAB_ASSERT_MSG(m_FrameSubmitted, "Vulkan EndFrame currently expects queue submission before the frame closes.");

    m_FrameInProgress = false;
}

VkSemaphore VulkanDevice::GetCurrentImageAvailableSemaphore() const
{
    RTRLAB_ASSERT_MSG(m_FrameInProgress, "Current Vulkan frame sync is only valid during an active frame.");
    return GetCurrentFrameSync().m_ImageAvailable;
}

VkSemaphore VulkanDevice::GetCurrentRenderFinishedSemaphore() const
{
    RTRLAB_ASSERT_MSG(m_FrameSubmitted, "Render-finished semaphore is only valid after queue submission.");
    return GetCurrentFrameSync().m_RenderFinished;
}

void VulkanDevice::AdvanceFrameSync()
{
    // The current bring-up path only advances after a submitted frame is either presented
    // or discarded during swapchain recreation. If future acquire-failure handling starts
    // skipping frames before submission, this invariant will need to be revisited.
    RTRLAB_ASSERT_MSG(!m_FrameInProgress, "Vulkan frame sync can only advance after EndFrame.");
    RTRLAB_ASSERT_MSG(m_FrameSubmitted, "Vulkan frame sync expects queue submission before presentation advances.");

    m_CurrentFrameSlot = (m_CurrentFrameSlot + 1) % static_cast<uint32_t>(m_FrameSyncObjects.size());
    m_FrameSubmitted = false;
}

VulkanDevice::FrameSync& VulkanDevice::GetCurrentFrameSync()
{
    return m_FrameSyncObjects[m_CurrentFrameSlot];
}

const VulkanDevice::FrameSync& VulkanDevice::GetCurrentFrameSync() const
{
    return m_FrameSyncObjects[m_CurrentFrameSlot];
}

void VulkanDevice::RecycleCurrentRenderFinishedSemaphore()
{
    FrameSync& frameSync = GetCurrentFrameSync();

    // Safety relies on the caller first making the queue/device idle. The current present
    // failure path does this via RecreateSwapchain() before recycling the semaphore.
    if (frameSync.m_RenderFinished != VK_NULL_HANDLE)
    {
        vkDestroySemaphore(m_Device, frameSync.m_RenderFinished, nullptr);
        frameSync.m_RenderFinished = VK_NULL_HANDLE;
    }

    VkSemaphoreCreateInfo semaphoreCreateInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    CheckVk(vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr, &frameSync.m_RenderFinished),
            "vkCreateSemaphore(renderFinished recycle)");
}

void VulkanDevice::InitializeInstance()
{
    if (m_Instance != VK_NULL_HANDLE)
        return;

    const std::vector<const char*> instanceExtensions = GetSupportedInstanceExtensions();

    VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.pApplicationName = "RTRLab";
    appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
    appInfo.pEngineName = "RTRLab";
    appInfo.engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
    createInfo.ppEnabledExtensionNames = instanceExtensions.data();

    CheckVk(vkCreateInstance(&createInfo, nullptr, &m_Instance), "vkCreateInstance");
    volkLoadInstance(m_Instance);
}

void VulkanDevice::InitializeDeviceObjects()
{
    if (m_HasDeviceObjects)
        return;

    InitializeInstance();

    uint32_t graphicsQueueFamily = std::numeric_limits<uint32_t>::max();
    m_PhysicalDevice = PickPhysicalDevice(m_Instance, graphicsQueueFamily);
    m_GraphicsQueueFamily = graphicsQueueFamily;
    m_PresentQueueFamily = graphicsQueueFamily;

    const float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo graphicsQueueCreateInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    graphicsQueueCreateInfo.queueFamilyIndex = m_GraphicsQueueFamily;
    graphicsQueueCreateInfo.queueCount = 1;
    graphicsQueueCreateInfo.pQueuePriorities = &queuePriority;

    const std::array<const char*, 1> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkPhysicalDeviceVulkan13Features vulkan13Features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    vulkan13Features.dynamicRendering = VK_TRUE;

    VkDeviceCreateInfo deviceCreateInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    deviceCreateInfo.pNext = &vulkan13Features;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &graphicsQueueCreateInfo;
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    CheckVk(vkCreateDevice(m_PhysicalDevice, &deviceCreateInfo, nullptr, &m_Device), "vkCreateDevice");
    volkLoadDevice(m_Device);

    vkGetDeviceQueue(m_Device, m_GraphicsQueueFamily, 0, &m_GraphicsQueue);
    m_PresentQueue = m_GraphicsQueue;

    VkCommandPoolCreateInfo commandPoolCreateInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolCreateInfo.queueFamilyIndex = m_GraphicsQueueFamily;
    CheckVk(vkCreateCommandPool(m_Device, &commandPoolCreateInfo, nullptr, &m_CommandPool), "vkCreateCommandPool");

    m_CommandList.Initialize(m_Device, m_CommandPool);
    m_CurrentFrameSlot = 0;
    m_FrameInProgress = false;
    m_FrameSubmitted = false;
    m_HasDeviceObjects = true;
}

void VulkanDevice::InitializeDeviceObjectsForSurface(VkSurfaceKHR surface)
{
    if (m_HasDeviceObjects)
    {
        RTRLAB_ASSERT_MSG(m_PhysicalDevice != VK_NULL_HANDLE, "Vulkan physical device must exist before presentation.");
        RTRLAB_ASSERT_MSG(SupportsPresentOnQueueFamily(m_PhysicalDevice, m_GraphicsQueueFamily, surface),
                          "This Vulkan device was initialized without a present-capable graphics queue for the "
                          "requested surface. Recreate the device from a swapchain-first path.");
        m_PresentQueueFamily = m_GraphicsQueueFamily;
        m_PresentQueue = m_GraphicsQueue;
        return;
    }

    InitializeInstance();

    QueueFamilySelection selection{};
    m_PhysicalDevice = PickPhysicalDevice(m_Instance, surface, selection);
    m_GraphicsQueueFamily = selection.m_GraphicsFamily;
    m_PresentQueueFamily = selection.m_PresentFamily;

    const float queuePriority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

    VkDeviceQueueCreateInfo graphicsQueueCreateInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    graphicsQueueCreateInfo.queueFamilyIndex = m_GraphicsQueueFamily;
    graphicsQueueCreateInfo.queueCount = 1;
    graphicsQueueCreateInfo.pQueuePriorities = &queuePriority;
    queueCreateInfos.push_back(graphicsQueueCreateInfo);

    if (m_PresentQueueFamily != m_GraphicsQueueFamily)
    {
        VkDeviceQueueCreateInfo presentQueueCreateInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        presentQueueCreateInfo.queueFamilyIndex = m_PresentQueueFamily;
        presentQueueCreateInfo.queueCount = 1;
        presentQueueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(presentQueueCreateInfo);
    }

    const std::array<const char*, 1> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkPhysicalDeviceVulkan13Features vulkan13Features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    vulkan13Features.dynamicRendering = VK_TRUE;

    VkDeviceCreateInfo deviceCreateInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    deviceCreateInfo.pNext = &vulkan13Features;
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    CheckVk(vkCreateDevice(m_PhysicalDevice, &deviceCreateInfo, nullptr, &m_Device), "vkCreateDevice");
    volkLoadDevice(m_Device);

    vkGetDeviceQueue(m_Device, m_GraphicsQueueFamily, 0, &m_GraphicsQueue);
    vkGetDeviceQueue(m_Device, m_PresentQueueFamily, 0, &m_PresentQueue);

    VkCommandPoolCreateInfo commandPoolCreateInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolCreateInfo.queueFamilyIndex = m_GraphicsQueueFamily;
    CheckVk(vkCreateCommandPool(m_Device, &commandPoolCreateInfo, nullptr, &m_CommandPool), "vkCreateCommandPool");

    m_CommandList.Initialize(m_Device, m_CommandPool);
    m_CurrentFrameSlot = 0;
    m_FrameInProgress = false;
    m_FrameSubmitted = false;
    m_HasDeviceObjects = true;
}

void VulkanDevice::InitializeFrameSyncObjects()
{
    VkSemaphoreCreateInfo semaphoreCreateInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

    VkFenceCreateInfo fenceCreateInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (FrameSync& frameSync : m_FrameSyncObjects)
    {
        CheckVk(vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr, &frameSync.m_ImageAvailable),
                "vkCreateSemaphore(imageAvailable)");
        CheckVk(vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr, &frameSync.m_RenderFinished),
                "vkCreateSemaphore(renderFinished)");
        CheckVk(vkCreateFence(m_Device, &fenceCreateInfo, nullptr, &frameSync.m_InFlightFence),
                "vkCreateFence(inFlight)");
    }
}

void VulkanDevice::ShutdownFrameSyncObjects()
{
    for (FrameSync& frameSync : m_FrameSyncObjects)
    {
        if (frameSync.m_ImageAvailable != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(m_Device, frameSync.m_ImageAvailable, nullptr);
            frameSync.m_ImageAvailable = VK_NULL_HANDLE;
        }

        if (frameSync.m_RenderFinished != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(m_Device, frameSync.m_RenderFinished, nullptr);
            frameSync.m_RenderFinished = VK_NULL_HANDLE;
        }

        if (frameSync.m_InFlightFence != VK_NULL_HANDLE)
        {
            vkDestroyFence(m_Device, frameSync.m_InFlightFence, nullptr);
            frameSync.m_InFlightFence = VK_NULL_HANDLE;
        }
    }
}

void VulkanDevice::InitializePresentationObjects(const NativeWindowHandle& nativeWindowHandle)
{
    if (m_HasPresentationObjects)
    {
        RTRLAB_ASSERT_MSG(m_NativeWindowHandle.m_System == nativeWindowHandle.m_System &&
                              m_NativeWindowHandle.m_Window == nativeWindowHandle.m_Window &&
                              m_NativeWindowHandle.m_Display == nativeWindowHandle.m_Display &&
                              m_NativeWindowHandle.m_Layer == nativeWindowHandle.m_Layer,
                          "VulkanDevice currently supports a single presentation target per device.");
        return;
    }

    InitializeInstance();
    m_Surface = CreateSurface(m_Instance, nativeWindowHandle);
    InitializeDeviceObjectsForSurface(m_Surface);
    InitializeFrameSyncObjects();
    m_NativeWindowHandle = nativeWindowHandle;
    m_CurrentFrameSlot = 0;
    m_FrameInProgress = false;
    m_FrameSubmitted = false;
    m_HasPresentationObjects = true;
}

void VulkanDevice::ShutdownPresentationObjects()
{
    if (!m_HasPresentationObjects)
        return;

    if (m_Device != VK_NULL_HANDLE)
        vkDeviceWaitIdle(m_Device);

    ShutdownFrameSyncObjects();

    if (m_Surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
        m_Surface = VK_NULL_HANDLE;
    }

    m_PresentQueue = m_GraphicsQueue;
    m_PresentQueueFamily = m_GraphicsQueueFamily;
    m_NativeWindowHandle = {};
    m_HasPresentationObjects = false;
}

void VulkanDevice::ShutdownDeviceObjects()
{
    if (!m_HasDeviceObjects)
        return;

    if (m_Device != VK_NULL_HANDLE)
        vkDeviceWaitIdle(m_Device);

    m_CommandList.Shutdown();

    if (m_CommandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
        m_CommandPool = VK_NULL_HANDLE;
    }

    if (m_Device != VK_NULL_HANDLE)
    {
        vkDestroyDevice(m_Device, nullptr);
        m_Device = VK_NULL_HANDLE;
    }

    m_PhysicalDevice = VK_NULL_HANDLE;
    m_GraphicsQueue = VK_NULL_HANDLE;
    m_PresentQueue = VK_NULL_HANDLE;
    m_GraphicsQueueFamily = std::numeric_limits<uint32_t>::max();
    m_PresentQueueFamily = std::numeric_limits<uint32_t>::max();
    m_CurrentFrameSlot = 0;
    m_FrameInProgress = false;
    m_FrameSubmitted = false;
    m_HasDeviceObjects = false;
}
