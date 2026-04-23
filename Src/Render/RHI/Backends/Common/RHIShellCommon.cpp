#include "Render/RHI/Backends/Common/RHIShellCommon.h"
#include "Render/Shader/ShaderReflection.h"

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
        case ReflectedTypeKind::StorageTexture:
            return ResourceKind::StorageTexture;
        case ReflectedTypeKind::Sampler:
            return ResourceKind::Sampler;
        case ReflectedTypeKind::ConstantData:
        case ReflectedTypeKind::ParameterBlock:
        case ReflectedTypeKind::Struct:
            return ResourceKind::UniformBuffer;
    }

    return ResourceKind::UniformBuffer;
}

void AddOrMergeBindingInfo(std::vector<BindingInfo>& bindings, const BindingInfo& candidate)
{
    auto it = std::find_if(bindings.begin(),
                           bindings.end(),
                           [&candidate](const BindingInfo& existing)
                           {
                               return existing.m_SetIndex == candidate.m_SetIndex &&
                                      existing.m_Binding == candidate.m_Binding && existing.m_Kind == candidate.m_Kind;
                           });

    if (it == bindings.end())
    {
        bindings.push_back(candidate);
        return;
    }

    if (it->m_Name.empty())
        it->m_Name = candidate.m_Name;
    RTRLAB_ASSERT_MSG(it->m_ArrayCount == candidate.m_ArrayCount,
                      "Reflected bindings that share set/binding/kind must also share the same array count.");
    RTRLAB_ASSERT_MSG(it->m_ByteSize == candidate.m_ByteSize,
                      "Reflected bindings that share set/binding/kind must also share the same byte size.");
    it->m_StageMask |= candidate.m_StageMask;
}

void CollectPipelineBindings(const ReflectedField& field,
                             uint32_t currentSetIndex,
                             bool hasSetIndex,
                             std::vector<BindingInfo>& bindings)
{
    uint32_t resolvedSetIndex = hasSetIndex ? currentSetIndex : field.m_SetIndex;
    bool childHasSetIndex = hasSetIndex;
    uint32_t childSetIndex = currentSetIndex;

    if (field.m_TypeKind == ReflectedTypeKind::ParameterBlock)
    {
        resolvedSetIndex = field.m_SetIndex;
        childHasSetIndex = true;
        childSetIndex = field.m_SetIndex;
    }

    if (IsPipelineBindableReflectedType(field.m_TypeKind))
    {
        BindingInfo bindingInfo;
        bindingInfo.m_Name = field.m_Name;
        bindingInfo.m_SetIndex = resolvedSetIndex;
        bindingInfo.m_Binding = field.m_Binding;
        bindingInfo.m_Kind = MapReflectedResourceKind(field.m_TypeKind);
        bindingInfo.m_ByteSize = field.m_TypeKind == ReflectedTypeKind::ParameterBlock ? field.m_Size : 0;
        bindingInfo.m_ArrayCount = field.m_ArrayCount;
        bindingInfo.m_StageMask = field.m_StageMask;
        AddOrMergeBindingInfo(bindings, bindingInfo);
    }

    for (const ReflectedField& child : field.m_Children)
        CollectPipelineBindings(child, childSetIndex, childHasSetIndex, bindings);
}
} // namespace

namespace RHIInternal
{
std::vector<const BindingInfo*> CollectBindingInfosForSet(const PipelineLayoutDesc& desc, uint32_t setIndex)
{
    std::vector<const BindingInfo*> bindings;

    for (const BindingInfo& binding : desc.m_Bindings)
    {
        if (binding.m_SetIndex == setIndex)
            bindings.push_back(&binding);
    }

    return bindings;
}

const BindingInfo*
FindBindingInfo(const PipelineLayoutDesc& desc, uint32_t setIndex, uint32_t binding, ResourceKind kind)
{
    const auto it = std::find_if(
        desc.m_Bindings.begin(),
        desc.m_Bindings.end(),
        [setIndex, binding, kind](const BindingInfo& candidate)
        { return candidate.m_SetIndex == setIndex && candidate.m_Binding == binding && candidate.m_Kind == kind; });
    return it != desc.m_Bindings.end() ? &(*it) : nullptr;
}

const BindingInfo* FindFirstBindingInfoForSet(const PipelineLayoutDesc& desc, uint32_t setIndex, ResourceKind kind)
{
    const auto it = std::find_if(desc.m_Bindings.begin(),
                                 desc.m_Bindings.end(),
                                 [setIndex, kind](const BindingInfo& candidate)
                                 { return candidate.m_SetIndex == setIndex && candidate.m_Kind == kind; });
    return it != desc.m_Bindings.end() ? &(*it) : nullptr;
}

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

BufferDesc SanitizeBufferDesc(const BufferDesc& desc)
{
    BufferDesc sanitized = desc;
    sanitized.m_Size = std::max<uint64_t>(desc.m_Size, 1);
    return sanitized;
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

TextureDesc SanitizeTextureDesc(const TextureDesc& desc)
{
    TextureDesc sanitized = desc;
    sanitized.m_Extent.m_Width = std::max(desc.m_Extent.m_Width, 1u);
    sanitized.m_Extent.m_Height = std::max(desc.m_Extent.m_Height, 1u);
    sanitized.m_Extent.m_Depth = std::max(desc.m_Extent.m_Depth, 1u);
    sanitized.m_MipLevels = std::max(desc.m_MipLevels, 1u);
    sanitized.m_ArrayLayers = std::max(desc.m_ArrayLayers, 1u);

    if (sanitized.m_Type == TextureType::Cube)
    {
        RTRLAB_ASSERT_MSG((sanitized.m_ArrayLayers % 6u) == 0,
                          "Cube textures require arrayLayers to be a multiple of 6.");
    }

    return sanitized;
}

PipelineLayoutDesc BuildPipelineLayoutDescFromReflection(const ShaderReflectionData& reflection)
{
    std::string validationError;
    RTRLAB_ASSERT_MSG(ValidateShaderReflectionData(reflection, &validationError), validationError.c_str());

    PipelineLayoutDesc layoutDesc;

    for (const ReflectedField& field : reflection.m_Globals)
        CollectPipelineBindings(field, 0, false, layoutDesc.m_Bindings);

    std::sort(layoutDesc.m_Bindings.begin(),
              layoutDesc.m_Bindings.end(),
              [](const BindingInfo& lhs, const BindingInfo& rhs)
              {
                  if (lhs.m_SetIndex != rhs.m_SetIndex)
                      return lhs.m_SetIndex < rhs.m_SetIndex;
                  if (lhs.m_Binding != rhs.m_Binding)
                      return lhs.m_Binding < rhs.m_Binding;
                  if (lhs.m_Kind != rhs.m_Kind)
                      return static_cast<uint32_t>(lhs.m_Kind) < static_cast<uint32_t>(rhs.m_Kind);
                  return lhs.m_Name < rhs.m_Name;
              });

    layoutDesc.m_PushConstants = reflection.m_PushConstants;
    return layoutDesc;
}

PipelineLayoutDesc ShellShaderProgram::DerivePipelineLayoutDesc() const
{
    return BuildPipelineLayoutDescFromReflection(m_Reflection);
}

ShellResourceSet::ShellResourceSet(PipelineLayout* layout, uint32_t setIndex) : m_Layout(layout), m_SetIndex(setIndex)
{
    RTRLAB_ASSERT_MSG(m_Layout != nullptr, "ResourceSet creation requires a valid PipelineLayout.");

    const std::vector<const BindingInfo*> setBindings = CollectBindingInfosForSet(m_Layout->GetDesc(), m_SetIndex);
    RTRLAB_ASSERTF(
        !setBindings.empty(), "ResourceSet set {} does not exist in the provided PipelineLayout.", m_SetIndex);

    if (const BindingInfo* constantBindingInfo =
            FindFirstBindingInfoForSet(m_Layout->GetDesc(), m_SetIndex, ResourceKind::UniformBuffer);
        constantBindingInfo != nullptr && constantBindingInfo->m_ByteSize > 0)
    {
        m_Constants.Resize(constantBindingInfo->m_ByteSize);
    }
}

void ShellResourceSet::SetConstantDataRaw(uint32_t offset, const void* data, size_t size)
{
    if (size == 0)
        return;

    const BindingInfo& bindingInfo = ValidateConstantBindingExists();
    RTRLAB_ASSERT_MSG(offset + size <= bindingInfo.m_ByteSize,
                      "ResourceSet constant write exceeds the declared UniformBuffer size.");
    m_Constants.SetRaw(offset, data, size);
    ++m_Version;
}

void ShellResourceSet::SetBufferArray(uint32_t binding, std::span<const BufferBinding> bufferBindings)
{
    const BindingInfo& bindingInfo = RequireBindingInfo(binding, ResourceKind::StorageBuffer);
    (void)ValidateBindingArrayCount(bindingInfo, bufferBindings.size(), "buffer");
    m_BufferBindings[binding] = std::vector<BufferBinding>(bufferBindings.begin(), bufferBindings.end());
    ++m_Version;
}

void ShellResourceSet::SetTextureArray(uint32_t binding, std::span<const TextureBinding> textureBindings)
{
    const BindingInfo* bindingInfo =
        FindBindingInfo(m_Layout->GetDesc(), m_SetIndex, binding, ResourceKind::SampledTexture);
    if (bindingInfo == nullptr)
        bindingInfo = FindBindingInfo(m_Layout->GetDesc(), m_SetIndex, binding, ResourceKind::StorageTexture);
    RTRLAB_ASSERTF(bindingInfo != nullptr,
                   "ResourceSet set {} has no texture binding {} in its PipelineLayout.",
                   m_SetIndex,
                   binding);
    (void)ValidateBindingArrayCount(*bindingInfo, textureBindings.size(), "texture");
    m_TextureBindings[binding] = std::vector<TextureBinding>(textureBindings.begin(), textureBindings.end());
    ++m_Version;
}

void ShellResourceSet::SetSamplerArray(uint32_t binding, std::span<const SamplerBinding> samplerBindings)
{
    const BindingInfo& bindingInfo = RequireBindingInfo(binding, ResourceKind::Sampler);
    (void)ValidateBindingArrayCount(bindingInfo, samplerBindings.size(), "sampler");
    m_SamplerBindings[binding] = std::vector<SamplerBinding>(samplerBindings.begin(), samplerBindings.end());
    ++m_Version;
}

const BindingInfo& ShellResourceSet::RequireBindingInfo(uint32_t binding, ResourceKind kind) const
{
    RTRLAB_ASSERT_MSG(m_Layout != nullptr, "ResourceSet binding validation requires a valid PipelineLayout.");
    const BindingInfo* bindingInfo = FindBindingInfo(m_Layout->GetDesc(), m_SetIndex, binding, kind);
    RTRLAB_ASSERTF(bindingInfo != nullptr,
                   "ResourceSet set {} has no binding {} of expected kind {} in its PipelineLayout.",
                   m_SetIndex,
                   binding,
                   static_cast<uint32_t>(kind));
    return *bindingInfo;
}

const BindingInfo& ShellResourceSet::ValidateBindingArrayCount(const BindingInfo& bindingInfo,
                                                               size_t providedCount,
                                                               std::string_view resourceKind) const
{
    RTRLAB_ASSERTF(providedCount == bindingInfo.m_ArrayCount,
                   "ResourceSet set {} binding {} expects exactly {} {} descriptor(s), but received {}.",
                   m_SetIndex,
                   bindingInfo.m_Binding,
                   bindingInfo.m_ArrayCount,
                   resourceKind,
                   providedCount);
    return bindingInfo;
}

const BindingInfo& ShellResourceSet::ValidateConstantBindingExists() const
{
    RTRLAB_ASSERT_MSG(m_Layout != nullptr, "ResourceSet constant validation requires a valid PipelineLayout.");
    const BindingInfo* bindingInfo =
        FindFirstBindingInfoForSet(m_Layout->GetDesc(), m_SetIndex, ResourceKind::UniformBuffer);
    RTRLAB_ASSERTF(
        bindingInfo != nullptr, "ResourceSet set {} has no UniformBuffer binding in its PipelineLayout.", m_SetIndex);
    RTRLAB_ASSERT_MSG(bindingInfo->m_ArrayCount <= 1,
                      "ResourceSet constant writes currently only support non-array UniformBuffer bindings.");
    return *bindingInfo;
}

void ShellCommandListBase::BeginRendering(const RenderingInfo& renderingInfo)
{
    RTRLAB_ASSERT_MSG(!renderingInfo.m_ColorAttachments.empty() || renderingInfo.m_DepthAttachment.m_View != nullptr,
                      "BeginRendering requires at least one color or depth attachment.");
    RTRLAB_ASSERT_MSG(renderingInfo.m_RenderArea.m_Width > 0 && renderingInfo.m_RenderArea.m_Height > 0,
                      "BeginRendering requires a non-zero render area.");

    for (const ColorAttachmentInfo& colorAttachment : renderingInfo.m_ColorAttachments)
    {
        RTRLAB_ASSERT_MSG(colorAttachment.m_View != nullptr,
                          "BeginRendering requires non-null color attachment views.");
    }

    m_RenderingInfo = renderingInfo;
    m_IsRendering = true;
}

void ShellCommandListBase::EndRendering()
{
    m_IsRendering = false;
}

void ShellCommandListBase::ResetState()
{
    m_RenderingInfo = {};
    m_IsRendering = false;
    m_GraphicsPipeline = nullptr;
    m_ComputePipeline = nullptr;
    m_ResourceSets.clear();
    m_MeshBinding = {};
    m_VertexOffsets.clear();
    m_IndexBuffer = nullptr;
    m_IndexOffset = 0;
    m_IndexType = IndexType::UInt32;
    m_Scissor = {};
    m_Viewport[0] = 0.0f;
    m_Viewport[1] = 0.0f;
    m_Viewport[2] = 0.0f;
    m_Viewport[3] = 0.0f;
    m_Viewport[4] = 0.0f;
    m_Viewport[5] = 1.0f;
    m_PushConstants.clear();
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

void ShellCommandListBase::CopyBuffer(Buffer*, Buffer*, std::span<const BufferCopyRegion>)
{
    RTRLAB_ASSERT_MSG(false,
                      "CopyBuffer is not implemented for the shell backend. Use a backend with a real copy path.");
}

void ShellCommandListBase::CopyBufferToTexture(Buffer*, Texture*, std::span<const BufferTextureCopyRegion>)
{
    RTRLAB_ASSERT_MSG(false,
                      "CopyBufferToTexture is not implemented for the shell backend. Use a backend with a real "
                      "copy/upload path.");
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
    return CreateScope<ShellBuffer>(SanitizeBufferDesc(desc));
}

Scope<Texture> ShellDeviceBase::CreateTexture(const TextureDesc& desc)
{
    return CreateScope<ShellTexture>(SanitizeTextureDesc(desc));
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

void ShellDeviceBase::WriteBuffer(Buffer*, uint64_t, const void*, uint64_t)
{
    RTRLAB_ASSERT_MSG(
        false, "WriteBuffer is not implemented for this backend. Use a backend with an explicit M3 upload path.");
}

void ShellDeviceBase::Submit(CommandList*) {}

void ShellDeviceBase::EndFrame(FrameContext*) {}
} // namespace RHIInternal
