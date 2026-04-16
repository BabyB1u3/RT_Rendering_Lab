#include "Render/RHI/Backends/Common/RHIShellCommon.h"

#include <algorithm>
#include <cstring>

#include "Core/Diagnostics/Assert/Assert.h"

namespace
{
ResourceKind MapReflectedResourceKind(ReflectedTypeKind typeKind)
{
    switch (typeKind)
    {
        case ReflectedTypeKind::Buffer:
            return ResourceKind::StorageBuffer;
        case ReflectedTypeKind::Texture:
            return ResourceKind::SampledTexture;
        case ReflectedTypeKind::Sampler:
            return ResourceKind::Sampler;
        case ReflectedTypeKind::ConstantData:
        case ReflectedTypeKind::ParameterBlock:
        case ReflectedTypeKind::Struct:
            return ResourceKind::UniformBuffer;
    }

    return ResourceKind::UniformBuffer;
}
} // namespace

namespace RHIInternal
{
bool IsNativeWindowHandleValid(const NativeWindowHandle& nativeWindowHandle)
{
    switch (nativeWindowHandle.m_System)
    {
        case NativeWindowSystem::Win32:
            return nativeWindowHandle.m_Window != 0;
        case NativeWindowSystem::Cocoa:
            return nativeWindowHandle.m_Window != 0 && nativeWindowHandle.m_Layer != nullptr;
        case NativeWindowSystem::Xlib:
        case NativeWindowSystem::Xcb:
        case NativeWindowSystem::Wayland:
            return nativeWindowHandle.m_Window != 0 && nativeWindowHandle.m_Display != nullptr;
    }

    return false;
}

SwapchainDesc SanitizeSwapchainDesc(const SwapchainDesc& desc)
{
    SwapchainDesc sanitized = desc;
    sanitized.m_Width = std::max(desc.m_Width, 1u);
    sanitized.m_Height = std::max(desc.m_Height, 1u);
    sanitized.m_ImageCount = std::max(desc.m_ImageCount, 1u);
    if (sanitized.m_Format == Format::Unknown)
        sanitized.m_Format = Format::BGRA8_UNORM;
    return sanitized;
}

PipelineLayoutDesc BuildPipelineLayoutDescFromReflection(const ShaderReflectionData& reflection)
{
    PipelineLayoutDesc layoutDesc;

    for (const ReflectedField& field : reflection.m_Globals)
    {
        const bool isBindableResource =
            field.m_TypeKind == ReflectedTypeKind::Texture || field.m_TypeKind == ReflectedTypeKind::Sampler ||
            field.m_TypeKind == ReflectedTypeKind::Buffer || field.m_TypeKind == ReflectedTypeKind::ParameterBlock ||
            field.m_TypeKind == ReflectedTypeKind::ConstantData;

        if (!isBindableResource)
            continue;

        BindingInfo bindingInfo;
        bindingInfo.m_Name = field.m_Name;
        bindingInfo.m_SetIndex = field.m_SetIndex;
        bindingInfo.m_Binding = field.m_Binding;
        bindingInfo.m_Kind = MapReflectedResourceKind(field.m_TypeKind);
        bindingInfo.m_ArrayCount = field.m_ArrayCount;
        bindingInfo.m_StageMask = field.m_StageMask;
        layoutDesc.m_Bindings.push_back(std::move(bindingInfo));
    }

    layoutDesc.m_PushConstants = reflection.m_PushConstants;
    return layoutDesc;
}

PipelineLayoutDesc ShellShaderProgram::DerivePipelineLayoutDesc() const
{
    return BuildPipelineLayoutDescFromReflection(m_Reflection);
}

ShellResourceSet::ShellResourceSet(PipelineLayout* layout, uint32_t setIndex) : m_Layout(layout), m_SetIndex(setIndex)
{
}

void ShellResourceSet::SetBuffer(uint32_t binding, const BufferBinding& bufferBinding)
{
    m_BufferBindings[binding] = bufferBinding;
    ++m_Version;
}

void ShellResourceSet::SetTexture(uint32_t binding, const TextureBinding& textureBinding)
{
    m_TextureBindings[binding] = textureBinding;
    ++m_Version;
}

void ShellResourceSet::SetSampler(uint32_t binding, const SamplerBinding& samplerBinding)
{
    m_SamplerBindings[binding] = samplerBinding;
    ++m_Version;
}

void ShellCommandListBase::BeginRendering(const RenderingInfo& renderingInfo)
{
    m_RenderingInfo = renderingInfo;
    m_IsRendering = true;
}

void ShellCommandListBase::EndRendering()
{
    m_IsRendering = false;
}

void ShellCommandListBase::BindGraphicsPipeline(GraphicsPipeline* pipeline)
{
    m_GraphicsPipeline = pipeline;
}

void ShellCommandListBase::BindComputePipeline(ComputePipeline* pipeline)
{
    m_ComputePipeline = pipeline;
}

void ShellCommandListBase::BindResourceSet(uint32_t setIndex, ResourceSet* resourceSet)
{
    m_ResourceSets[setIndex] = resourceSet;
}

void ShellCommandListBase::PushConstants(ShaderStage, uint32_t offset, uint32_t size, const void* data)
{
    if (size == 0 || data == nullptr)
        return;

    if (offset + size > m_PushConstants.size())
        m_PushConstants.resize(offset + size);

    std::memcpy(m_PushConstants.data() + offset, data, size);
}

void ShellCommandListBase::BindMesh(const MeshBinding& meshBinding, const uint64_t* vertexOffsets)
{
    m_MeshBinding = meshBinding;
    m_VertexOffsets.clear();

    if (vertexOffsets != nullptr)
    {
        m_VertexOffsets.assign(vertexOffsets, vertexOffsets + meshBinding.m_VertexBuffers.size());
    }
}

void ShellCommandListBase::BindVertexBuffers(uint32_t firstSlot,
                                             Buffer* const* buffers,
                                             uint32_t count,
                                             const uint64_t* offsets)
{
    if (m_MeshBinding.m_VertexBuffers.size() < firstSlot + count)
        m_MeshBinding.m_VertexBuffers.resize(firstSlot + count);
    if (m_VertexOffsets.size() < firstSlot + count)
        m_VertexOffsets.resize(firstSlot + count, 0);

    for (uint32_t index = 0; index < count; ++index)
    {
        m_MeshBinding.m_VertexBuffers[firstSlot + index] = buffers[index];
        m_VertexOffsets[firstSlot + index] = offsets != nullptr ? offsets[index] : 0;
    }
}

void ShellCommandListBase::BindIndexBuffer(Buffer* buffer, uint64_t offset, IndexType indexType)
{
    m_IndexBuffer = buffer;
    m_IndexOffset = offset;
    m_IndexType = indexType;
}

void ShellCommandListBase::SetViewport(float x, float y, float w, float h, float zmin, float zmax)
{
    m_Viewport[0] = x;
    m_Viewport[1] = y;
    m_Viewport[2] = w;
    m_Viewport[3] = h;
    m_Viewport[4] = zmin;
    m_Viewport[5] = zmax;
}

void ShellCommandListBase::SetScissor(int32_t x, int32_t y, uint32_t w, uint32_t h)
{
    m_Scissor = Rect2D{x, y, w, h};
}

void ShellCommandListBase::Draw(uint32_t vertexCount, uint32_t firstVertex)
{
    m_LastDrawVertexCount = vertexCount;
    m_LastDrawFirstVertex = firstVertex;
}

void ShellCommandListBase::DrawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset)
{
    m_LastDrawIndexedCount = indexCount;
    m_LastDrawFirstIndex = firstIndex;
    m_LastDrawVertexOffset = vertexOffset;
}

void ShellCommandListBase::Dispatch(uint32_t groupX, uint32_t groupY, uint32_t groupZ)
{
    m_LastDispatchX = groupX;
    m_LastDispatchY = groupY;
    m_LastDispatchZ = groupZ;
}

void ShellCommandListBase::TextureBarrier(Texture*, TextureState, TextureState, ShaderStage, ShaderStage) {}

void ShellCommandListBase::BufferBarrier(Buffer*, BufferState, BufferState, ShaderStage, ShaderStage) {}

ShellSwapchainBase::ShellSwapchainBase(const SwapchainDesc& desc, const NativeWindowHandle& nativeWindowHandle)
    : m_Desc(SanitizeSwapchainDesc(desc)), m_NativeWindowHandle(nativeWindowHandle)
{
    RTRLAB_ASSERT_MSG(IsNativeWindowHandleValid(nativeWindowHandle), "Native window handle is incomplete.");
    RebuildImages();
}

uint32_t ShellSwapchainBase::AcquireNextImage()
{
    const uint32_t currentImageIndex = m_NextImageIndex;
    m_NextImageIndex = (m_NextImageIndex + 1) % GetImageCount();
    return currentImageIndex;
}

Texture* ShellSwapchainBase::GetImage(uint32_t imageIndex) const
{
    RTRLAB_ASSERT_MSG(imageIndex < m_Images.size(), "Swapchain image index out of range.");
    return m_Images[imageIndex].get();
}

TextureView* ShellSwapchainBase::GetImageView(uint32_t imageIndex) const
{
    RTRLAB_ASSERT_MSG(imageIndex < m_ImageViews.size(), "Swapchain image-view index out of range.");
    return m_ImageViews[imageIndex].get();
}

void ShellSwapchainBase::Present(uint32_t imageIndex)
{
    RTRLAB_ASSERT_MSG(imageIndex < m_Images.size(), "Swapchain present index out of range.");
}

void ShellSwapchainBase::Resize(uint32_t newWidth, uint32_t newHeight)
{
    m_Desc.m_Width = std::max(newWidth, 1u);
    m_Desc.m_Height = std::max(newHeight, 1u);
    RebuildImages();
}

TextureDesc ShellSwapchainBase::BuildSwapchainImageDesc() const
{
    TextureDesc textureDesc;
    textureDesc.m_Type = TextureType::Tex2D;
    textureDesc.m_Format = m_Desc.m_Format;
    textureDesc.m_Extent = Extent3D{m_Desc.m_Width, m_Desc.m_Height, 1};
    textureDesc.m_MipLevels = 1;
    textureDesc.m_ArrayLayers = 1;
    textureDesc.m_UsageMask = TextureUsage::RenderTarget | TextureUsage::CopySrc;
    textureDesc.m_DebugName = "SwapchainImage";
    return textureDesc;
}

void ShellSwapchainBase::RebuildImages()
{
    m_Images.clear();
    m_ImageViews.clear();
    m_Images.reserve(m_Desc.m_ImageCount);
    m_ImageViews.reserve(m_Desc.m_ImageCount);

    const TextureDesc imageDesc = BuildSwapchainImageDesc();

    for (uint32_t imageIndex = 0; imageIndex < m_Desc.m_ImageCount; ++imageIndex)
    {
        auto image = CreateScope<ShellTexture>(imageDesc);

        TextureViewDesc viewDesc;
        viewDesc.m_Type = TextureType::Tex2D;
        viewDesc.m_Format = imageDesc.m_Format;
        viewDesc.m_Aspect = TextureAspect::Color;

        auto view = CreateScope<ShellTextureView>(image.get(), viewDesc);
        m_ImageViews.push_back(std::move(view));
        m_Images.push_back(std::move(image));
    }

    m_NextImageIndex = 0;
}

Scope<Buffer> ShellDeviceBase::CreateBuffer(const BufferDesc& desc)
{
    return CreateScope<ShellBuffer>(desc);
}

Scope<Texture> ShellDeviceBase::CreateTexture(const TextureDesc& desc)
{
    return CreateScope<ShellTexture>(desc);
}

Scope<TextureView> ShellDeviceBase::CreateTextureView(Texture* texture, const TextureViewDesc& desc)
{
    return CreateScope<ShellTextureView>(texture, desc);
}

Scope<Sampler> ShellDeviceBase::CreateSampler(const SamplerDesc& desc)
{
    return CreateScope<ShellSampler>(desc);
}

Scope<ShaderProgram> ShellDeviceBase::CreateShaderProgram(const CompiledShaderProgramDesc& desc)
{
    return CreateScope<ShellShaderProgram>(desc);
}

Scope<PipelineLayout> ShellDeviceBase::CreatePipelineLayout(const PipelineLayoutDesc& desc)
{
    return CreateScope<ShellPipelineLayout>(desc);
}

Scope<ResourceSet> ShellDeviceBase::CreateResourceSet(PipelineLayout* layout, uint32_t setIndex)
{
    return CreateScope<ShellResourceSet>(layout, setIndex);
}

Scope<VertexInputLayout> ShellDeviceBase::CreateVertexInputLayout(const VertexInputLayoutDesc& desc)
{
    return CreateScope<ShellVertexInputLayout>(desc);
}

Scope<GraphicsPipeline> ShellDeviceBase::CreateGraphicsPipeline(const GraphicsPipelineDesc& desc)
{
    return CreateScope<ShellGraphicsPipeline>(desc);
}

Scope<ComputePipeline> ShellDeviceBase::CreateComputePipeline(const ComputePipelineDesc& desc)
{
    return CreateScope<ShellComputePipeline>(desc);
}

void ShellDeviceBase::Submit(CommandList*) {}

void ShellDeviceBase::EndFrame(FrameContext*) {}
} // namespace RHIInternal
