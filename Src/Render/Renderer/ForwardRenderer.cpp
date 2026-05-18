#include "Render/Renderer/ForwardRenderer.h"

#include <algorithm>
#include <string>
#include <utility>

#include "Core/Diagnostics/Assert/Assert.h"
#include "Render/Shader/ShaderCompiler.h"

namespace Renderer
{
namespace
{
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
} // namespace

void ForwardRenderer::Initialize(Device& device, const ForwardRendererDesc& desc)
{
    RTRLAB_ASSERTF(!desc.m_ShaderPath.empty(), "{} requires a shader path.", desc.m_DebugName);
    RTRLAB_ASSERTF(desc.m_ColorFormat != Format::Unknown, "{} requires a color target format.", desc.m_DebugName);
    RTRLAB_ASSERTF(desc.m_VertexStride > 0, "{} requires a non-zero vertex stride.", desc.m_DebugName);

    Reset();

    m_ShaderProgram = device.CreateShaderProgram(
        CompileShaderProgramDesc(BuildGraphicsShaderCompileRequest(desc.m_ShaderPath), desc.m_DebugName));
    m_PipelineLayout = device.CreatePipelineLayout(m_ShaderProgram->DerivePipelineLayoutDesc());

    const PipelineLayoutDesc& pipelineLayoutDesc = m_PipelineLayout->GetDesc();
    m_SetIndices.m_FrameSet = FindRequiredSetIndex(pipelineLayoutDesc, "gFrame", desc.m_DebugName);
    m_SetIndices.m_MaterialSet = FindRequiredSetIndex(pipelineLayoutDesc, "gMaterial", desc.m_DebugName);
    m_SetIndices.m_ObjectSet = FindRequiredSetIndex(pipelineLayoutDesc, "gObject", desc.m_DebugName);

    m_FrameSet = device.CreateResourceSet(m_PipelineLayout.get(), m_SetIndices.m_FrameSet);

    VertexInputLayoutDesc vertexInputLayoutDesc;
    vertexInputLayoutDesc.m_Buffers = {{desc.m_VertexStride, false}};
    vertexInputLayoutDesc.m_Attributes = {
        {0u, Format::RGBA32F, desc.m_PositionOffset, 0u},
        {1u, Format::RG32F, desc.m_UVOffset, 0u},
    };
    m_VertexInputLayout = device.CreateVertexInputLayout(vertexInputLayoutDesc);

    GraphicsPipelineDesc pipelineDesc;
    pipelineDesc.m_PipelineLayout = m_PipelineLayout.get();
    pipelineDesc.m_ShaderProgram = m_ShaderProgram.get();
    pipelineDesc.m_VertexInput = m_VertexInputLayout.get();
    pipelineDesc.m_RasterState.m_CullMode = CullMode::None;
    if (desc.m_DepthFormat != Format::Unknown)
    {
        pipelineDesc.m_DepthStencilState.m_DepthTestEnable = true;
        pipelineDesc.m_DepthStencilState.m_DepthWriteEnable = true;
        pipelineDesc.m_DepthStencilState.m_DepthCompareOp = CompareOp::Less;
        pipelineDesc.m_DepthFormat = desc.m_DepthFormat;
    }
    pipelineDesc.m_ColorFormats = {desc.m_ColorFormat};
    m_GraphicsPipeline = device.CreateGraphicsPipeline(pipelineDesc);
}

bool ForwardRenderer::IsInitialized() const
{
    return m_ShaderProgram != nullptr && m_PipelineLayout != nullptr && m_FrameSet != nullptr &&
           m_VertexInputLayout != nullptr && m_GraphicsPipeline != nullptr;
}

void ForwardRenderer::Reset()
{
    m_GraphicsPipeline.reset();
    m_VertexInputLayout.reset();
    m_FrameSet.reset();
    m_PipelineLayout.reset();
    m_ShaderProgram.reset();
    m_SetIndices = {};
}
} // namespace Renderer
