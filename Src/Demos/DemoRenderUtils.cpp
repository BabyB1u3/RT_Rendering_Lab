#include "Demos/DemoRenderUtils.h"

#include <algorithm>

#include <stb_image.h>

#include "Core/App/Application.h"
#include "Core/Diagnostics/Assert/Assert.h"
#include "Core/Resource/FileSystem.h"

namespace DemoRenderUtils
{
Math::Mat4 BuildOrbitViewProjection(uint32_t framebufferWidth, uint32_t framebufferHeight)
{
    const float aspect =
        framebufferHeight == 0 ? 1.0f : static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight);
    const Math::Mat4 projection = Math::PerspectiveRH_ZO(Math::Radians(55.0f), aspect, 0.1f, 100.0f);
    const Math::Mat4 view =
        Math::LookAt(Math::Vec3(0.0f, 0.0f, 3.1f), Math::Vec3(0.0f, 0.0f, 0.0f), Math::Vec3(0.0f, 1.0f, 0.0f));
    return projection * view;
}

DemoViewport ComputeCenteredSquareViewport(uint32_t framebufferWidth, uint32_t framebufferHeight)
{
    RTRLAB_ASSERT_MSG(framebufferWidth > 0 && framebufferHeight > 0,
                      "Demo viewport calculation requires a non-zero framebuffer size.");

    const float squareExtent = static_cast<float>(std::min(framebufferWidth, framebufferHeight));

    DemoViewport viewport;
    viewport.m_Width = squareExtent;
    viewport.m_Height = squareExtent;
    viewport.m_X = (static_cast<float>(framebufferWidth) - squareExtent) * 0.5f;
    viewport.m_Y = (static_cast<float>(framebufferHeight) - squareExtent) * 0.5f;
    return viewport;
}

uint32_t
FindRequiredSetIndex(const PipelineLayoutDesc& layoutDesc, std::string_view bindingName, std::string_view debugOwner)
{
    const auto it = std::find_if(layoutDesc.m_Bindings.begin(),
                                 layoutDesc.m_Bindings.end(),
                                 [bindingName](const BindingInfo& binding) { return binding.m_Name == bindingName; });
    RTRLAB_ASSERTF(it != layoutDesc.m_Bindings.end(),
                   "{} failed to find reflected binding '{}' in the PipelineLayout.",
                   debugOwner,
                   bindingName);
    return it->m_SetIndex;
}

ShaderCompileRequest BuildGraphicsShaderCompileRequest(const std::filesystem::path& shaderPath)
{
    ShaderCompileRequest request;

#if defined(GLAB_BACKEND_VULKAN)
    request.m_Targets.push_back({BackendType::Vulkan, MetalCodeFormat::MslSource});
#elif defined(GLAB_BACKEND_METAL)
    request.m_Targets.push_back({BackendType::Metal, MetalCodeFormat::MslSource});
#endif

    const std::string shaderModule = shaderPath.generic_string();
    request.m_Source.m_Entries.push_back({shaderModule, "main_vertex", ShaderStage::Vertex});
    request.m_Source.m_Entries.push_back({shaderModule, "main_fragment", ShaderStage::Fragment});
    return request;
}

CompiledShaderProgramDesc CompileShaderProgramDesc(const ShaderCompileRequest& request, std::string_view debugOwner)
{
    Scope<ShaderCompiler> shaderCompiler = CreateShaderCompiler();
    RTRLAB_ASSERTF(shaderCompiler != nullptr, "{} requires a valid ShaderCompiler instance.", debugOwner);

    ShaderCompileResult compileResult = shaderCompiler->CompileProgram(request);
    RTRLAB_ASSERTF(compileResult.m_Succeeded,
                   "{} failed to compile its Slang shader program: {}",
                   debugOwner,
                   compileResult.m_ErrorMessage);
    return std::move(compileResult.m_Program);
}

void CreateStaticBufferPair(Device& device,
                            const void* data,
                            uint64_t size,
                            BufferUsage targetUsage,
                            const char* targetDebugName,
                            const char* uploadDebugName,
                            Scope<Buffer>& targetBuffer,
                            Scope<Buffer>& uploadBuffer)
{
    RTRLAB_ASSERT_MSG(data != nullptr, "Static demo buffer creation requires non-null source data.");
    RTRLAB_ASSERT_MSG(size > 0, "Static demo buffer creation requires a non-zero byte size.");

    BufferDesc targetDesc;
    targetDesc.m_Size = size;
    targetDesc.m_UsageMask = targetUsage | BufferUsage::CopyDst;
    targetDesc.m_MemoryUsage = MemoryUsage::GpuOnly;
    targetDesc.m_DebugName = targetDebugName;
    targetBuffer = device.CreateBuffer(targetDesc);

    BufferDesc uploadDesc;
    uploadDesc.m_Size = size;
    uploadDesc.m_UsageMask = BufferUsage::CopySrc;
    uploadDesc.m_MemoryUsage = MemoryUsage::CpuToGpu;
    uploadDesc.m_DebugName = uploadDebugName;
    uploadBuffer = device.CreateBuffer(uploadDesc);
    device.WriteBuffer(uploadBuffer.get(), 0, data, size);
}

void UploadStaticBufferPair(CommandList& commandList,
                            ResourceStateTracker& resourceStateTracker,
                            Buffer* uploadBuffer,
                            Buffer* targetBuffer,
                            uint64_t size,
                            BufferState finalState)
{
    RTRLAB_ASSERT_MSG(uploadBuffer != nullptr && targetBuffer != nullptr,
                      "Static demo buffer upload requires both upload and target buffers.");
    RTRLAB_ASSERT_MSG(size > 0, "Static demo buffer upload requires a non-zero byte size.");

    resourceStateTracker.Transition(uploadBuffer, BufferState::CopySource);
    resourceStateTracker.Transition(targetBuffer, BufferState::CopyDest);
    resourceStateTracker.FlushBarriers(&commandList);

    const BufferCopyRegion copyRegion{0, 0, size};
    commandList.CopyBuffer(uploadBuffer, targetBuffer, std::span<const BufferCopyRegion>(&copyRegion, 1));

    resourceStateTracker.Transition(targetBuffer, finalState);
    resourceStateTracker.FlushBarriers(&commandList);
}

LoadedImage LoadTextureFileRGBA8(const std::filesystem::path& texturePath, std::string_view debugOwner)
{
    int width = 0;
    int height = 0;
    int sourceChannels = 0;
    stbi_uc* pixels = stbi_load(texturePath.string().c_str(), &width, &height, &sourceChannels, 4);
    RTRLAB_ASSERTF(pixels != nullptr,
                   "{} failed to load texture '{}': {}",
                   debugOwner,
                   texturePath.string(),
                   stbi_failure_reason() != nullptr ? stbi_failure_reason() : "unknown stb_image error");
    RTRLAB_ASSERTF(width > 0 && height > 0, "{} loaded an invalid texture extent.", debugOwner);

    const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
    LoadedImage image;
    image.m_Width = static_cast<uint32_t>(width);
    image.m_Height = static_cast<uint32_t>(height);
    image.m_Pixels.assign(pixels, pixels + pixelCount);
    stbi_image_free(pixels);
    return image;
}
} // namespace DemoRenderUtils
