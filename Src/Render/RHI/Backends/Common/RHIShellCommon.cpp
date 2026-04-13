#include "Render/RHI/Backends/Common/RHIShellCommon.h"

#include <algorithm>
#include <cstring>

#include "Core/Diagnostics/Assert/Assert.h"

namespace
{
    ResourceKind mapReflectedResourceKind(ReflectedTypeKind typeKind)
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
}

namespace RHIInternal
{
    bool isNativeWindowHandleValid(const NativeWindowHandle &nativeWindowHandle)
    {
        switch (nativeWindowHandle.system)
        {
        case NativeWindowSystem::Win32:
            return nativeWindowHandle.window != 0;
        case NativeWindowSystem::Cocoa:
            return nativeWindowHandle.window != 0 && nativeWindowHandle.layer != nullptr;
        case NativeWindowSystem::Xlib:
        case NativeWindowSystem::Xcb:
        case NativeWindowSystem::Wayland:
            return nativeWindowHandle.window != 0 && nativeWindowHandle.display != nullptr;
        }

        return false;
    }

    SwapchainDesc sanitizeSwapchainDesc(const SwapchainDesc &desc)
    {
        SwapchainDesc sanitized = desc;
        sanitized.width = std::max(desc.width, 1u);
        sanitized.height = std::max(desc.height, 1u);
        sanitized.imageCount = std::max(desc.imageCount, 1u);
        if (sanitized.format == Format::Unknown)
            sanitized.format = Format::BGRA8_UNORM;
        return sanitized;
    }

    PipelineLayoutDesc buildPipelineLayoutDescFromReflection(const ShaderReflectionData &reflection)
    {
        PipelineLayoutDesc layoutDesc;

        for (const ReflectedField &field : reflection.globals)
        {
            const bool isBindableResource =
                field.typeKind == ReflectedTypeKind::Texture ||
                field.typeKind == ReflectedTypeKind::Sampler ||
                field.typeKind == ReflectedTypeKind::Buffer ||
                field.typeKind == ReflectedTypeKind::ParameterBlock ||
                field.typeKind == ReflectedTypeKind::ConstantData;

            if (!isBindableResource)
                continue;

            BindingInfo bindingInfo;
            bindingInfo.name = field.name;
            bindingInfo.setIndex = field.setIndex;
            bindingInfo.binding = field.binding;
            bindingInfo.kind = mapReflectedResourceKind(field.typeKind);
            bindingInfo.arrayCount = field.arrayCount;
            bindingInfo.stageMask = field.stageMask;
            layoutDesc.bindings.push_back(std::move(bindingInfo));
        }

        layoutDesc.pushConstants = reflection.pushConstants;
        return layoutDesc;
    }

    PipelineLayoutDesc ShellShaderProgram::derivePipelineLayoutDesc() const
    {
        return buildPipelineLayoutDescFromReflection(m_Reflection);
    }

    ShellResourceSet::ShellResourceSet(PipelineLayout *layout, uint32_t setIndex)
        : m_Layout(layout), m_SetIndex(setIndex)
    {
    }

    void ShellResourceSet::setBuffer(uint32_t binding, const BufferBinding &bufferBinding)
    {
        m_BufferBindings[binding] = bufferBinding;
        ++m_Version;
    }

    void ShellResourceSet::setTexture(uint32_t binding, const TextureBinding &textureBinding)
    {
        m_TextureBindings[binding] = textureBinding;
        ++m_Version;
    }

    void ShellResourceSet::setSampler(uint32_t binding, const SamplerBinding &samplerBinding)
    {
        m_SamplerBindings[binding] = samplerBinding;
        ++m_Version;
    }

    void ShellCommandListBase::beginRendering(const RenderingInfo &renderingInfo)
    {
        m_RenderingInfo = renderingInfo;
        m_IsRendering = true;
    }

    void ShellCommandListBase::endRendering()
    {
        m_IsRendering = false;
    }

    void ShellCommandListBase::bindGraphicsPipeline(GraphicsPipeline *pipeline)
    {
        m_GraphicsPipeline = pipeline;
    }

    void ShellCommandListBase::bindComputePipeline(ComputePipeline *pipeline)
    {
        m_ComputePipeline = pipeline;
    }

    void ShellCommandListBase::bindResourceSet(uint32_t setIndex, ResourceSet *resourceSet)
    {
        m_ResourceSets[setIndex] = resourceSet;
    }

    void ShellCommandListBase::pushConstants(ShaderStage, uint32_t offset, uint32_t size, const void *data)
    {
        if (size == 0 || data == nullptr)
            return;

        if (offset + size > m_PushConstants.size())
            m_PushConstants.resize(offset + size);

        std::memcpy(m_PushConstants.data() + offset, data, size);
    }

    void ShellCommandListBase::bindMesh(const MeshBinding &meshBinding, const uint64_t *vertexOffsets)
    {
        m_MeshBinding = meshBinding;
        m_VertexOffsets.clear();

        if (vertexOffsets != nullptr)
        {
            m_VertexOffsets.assign(vertexOffsets, vertexOffsets + meshBinding.vertexBuffers.size());
        }
    }

    void ShellCommandListBase::bindVertexBuffers(uint32_t firstSlot, Buffer *const *buffers, uint32_t count, const uint64_t *offsets)
    {
        if (m_MeshBinding.vertexBuffers.size() < firstSlot + count)
            m_MeshBinding.vertexBuffers.resize(firstSlot + count);
        if (m_VertexOffsets.size() < firstSlot + count)
            m_VertexOffsets.resize(firstSlot + count, 0);

        for (uint32_t index = 0; index < count; ++index)
        {
            m_MeshBinding.vertexBuffers[firstSlot + index] = buffers[index];
            m_VertexOffsets[firstSlot + index] = offsets != nullptr ? offsets[index] : 0;
        }
    }

    void ShellCommandListBase::bindIndexBuffer(Buffer *buffer, uint64_t offset, IndexType indexType)
    {
        m_IndexBuffer = buffer;
        m_IndexOffset = offset;
        m_IndexType = indexType;
    }

    void ShellCommandListBase::setViewport(float x, float y, float w, float h, float zmin, float zmax)
    {
        m_Viewport[0] = x;
        m_Viewport[1] = y;
        m_Viewport[2] = w;
        m_Viewport[3] = h;
        m_Viewport[4] = zmin;
        m_Viewport[5] = zmax;
    }

    void ShellCommandListBase::setScissor(int32_t x, int32_t y, uint32_t w, uint32_t h)
    {
        m_Scissor = Rect2D{x, y, w, h};
    }

    void ShellCommandListBase::draw(uint32_t vertexCount, uint32_t firstVertex)
    {
        m_LastDrawVertexCount = vertexCount;
        m_LastDrawFirstVertex = firstVertex;
    }

    void ShellCommandListBase::drawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset)
    {
        m_LastDrawIndexedCount = indexCount;
        m_LastDrawFirstIndex = firstIndex;
        m_LastDrawVertexOffset = vertexOffset;
    }

    void ShellCommandListBase::dispatch(uint32_t groupX, uint32_t groupY, uint32_t groupZ)
    {
        m_LastDispatchX = groupX;
        m_LastDispatchY = groupY;
        m_LastDispatchZ = groupZ;
    }

    void ShellCommandListBase::textureBarrier(Texture *, TextureState, TextureState, ShaderStage, ShaderStage)
    {
    }

    void ShellCommandListBase::bufferBarrier(Buffer *, BufferState, BufferState, ShaderStage, ShaderStage)
    {
    }

    ShellSwapchainBase::ShellSwapchainBase(const SwapchainDesc &desc, const NativeWindowHandle &nativeWindowHandle)
        : m_Desc(sanitizeSwapchainDesc(desc)), m_NativeWindowHandle(nativeWindowHandle)
    {
        RTRLAB_ASSERT_MSG(isNativeWindowHandleValid(nativeWindowHandle), "Native window handle is incomplete.");
        rebuildImages();
    }

    uint32_t ShellSwapchainBase::acquireNextImage()
    {
        const uint32_t currentImageIndex = m_NextImageIndex;
        m_NextImageIndex = (m_NextImageIndex + 1) % imageCount();
        return currentImageIndex;
    }

    Texture *ShellSwapchainBase::getImage(uint32_t imageIndex) const
    {
        RTRLAB_ASSERT_MSG(imageIndex < m_Images.size(), "Swapchain image index out of range.");
        return m_Images[imageIndex].get();
    }

    TextureView *ShellSwapchainBase::getImageView(uint32_t imageIndex) const
    {
        RTRLAB_ASSERT_MSG(imageIndex < m_ImageViews.size(), "Swapchain image-view index out of range.");
        return m_ImageViews[imageIndex].get();
    }

    void ShellSwapchainBase::present(uint32_t imageIndex)
    {
        RTRLAB_ASSERT_MSG(imageIndex < m_Images.size(), "Swapchain present index out of range.");
    }

    void ShellSwapchainBase::resize(uint32_t newWidth, uint32_t newHeight)
    {
        m_Desc.width = std::max(newWidth, 1u);
        m_Desc.height = std::max(newHeight, 1u);
        rebuildImages();
    }

    TextureDesc ShellSwapchainBase::buildSwapchainImageDesc() const
    {
        TextureDesc textureDesc;
        textureDesc.type = TextureType::Tex2D;
        textureDesc.format = m_Desc.format;
        textureDesc.extent = Extent3D{m_Desc.width, m_Desc.height, 1};
        textureDesc.mipLevels = 1;
        textureDesc.arrayLayers = 1;
        textureDesc.usageMask = TextureUsage::RenderTarget | TextureUsage::CopySrc;
        textureDesc.debugName = "SwapchainImage";
        return textureDesc;
    }

    void ShellSwapchainBase::rebuildImages()
    {
        m_Images.clear();
        m_ImageViews.clear();
        m_Images.reserve(m_Desc.imageCount);
        m_ImageViews.reserve(m_Desc.imageCount);

        const TextureDesc imageDesc = buildSwapchainImageDesc();

        for (uint32_t imageIndex = 0; imageIndex < m_Desc.imageCount; ++imageIndex)
        {
            auto image = CreateScope<ShellTexture>(imageDesc);

            TextureViewDesc viewDesc;
            viewDesc.type = TextureType::Tex2D;
            viewDesc.format = imageDesc.format;
            viewDesc.aspect = TextureAspect::Color;

            auto view = CreateScope<ShellTextureView>(image.get(), viewDesc);
            m_ImageViews.push_back(std::move(view));
            m_Images.push_back(std::move(image));
        }

        m_NextImageIndex = 0;
    }

    Scope<Buffer> ShellDeviceBase::createBuffer(const BufferDesc &desc)
    {
        return CreateScope<ShellBuffer>(desc);
    }

    Scope<Texture> ShellDeviceBase::createTexture(const TextureDesc &desc)
    {
        return CreateScope<ShellTexture>(desc);
    }

    Scope<TextureView> ShellDeviceBase::createTextureView(Texture *texture, const TextureViewDesc &desc)
    {
        return CreateScope<ShellTextureView>(texture, desc);
    }

    Scope<Sampler> ShellDeviceBase::createSampler(const SamplerDesc &desc)
    {
        return CreateScope<ShellSampler>(desc);
    }

    Scope<ShaderProgram> ShellDeviceBase::createShaderProgram(const CompiledShaderProgramDesc &desc)
    {
        return CreateScope<ShellShaderProgram>(desc);
    }

    Scope<PipelineLayout> ShellDeviceBase::createPipelineLayout(const PipelineLayoutDesc &desc)
    {
        return CreateScope<ShellPipelineLayout>(desc);
    }

    Scope<ResourceSet> ShellDeviceBase::createResourceSet(PipelineLayout *layout, uint32_t setIndex)
    {
        return CreateScope<ShellResourceSet>(layout, setIndex);
    }

    Scope<VertexInputLayout> ShellDeviceBase::createVertexInputLayout(const VertexInputLayoutDesc &desc)
    {
        return CreateScope<ShellVertexInputLayout>(desc);
    }

    Scope<GraphicsPipeline> ShellDeviceBase::createGraphicsPipeline(const GraphicsPipelineDesc &desc)
    {
        return CreateScope<ShellGraphicsPipeline>(desc);
    }

    Scope<ComputePipeline> ShellDeviceBase::createComputePipeline(const ComputePipelineDesc &desc)
    {
        return CreateScope<ShellComputePipeline>(desc);
    }

    void ShellDeviceBase::submit(CommandList *)
    {
    }

    void ShellDeviceBase::endFrame(FrameContext *)
    {
    }
} // namespace RHIInternal
