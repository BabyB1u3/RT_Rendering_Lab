#pragma once

/// @file RHIPipeline.h
/// @brief Public RHI pipeline-layout, vertex-input, and pipeline object types.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "Render/RHI/RHIResources.h"
#include "Render/Shader/ShaderTypes.h"

enum class ResourceKind
{
    UniformBuffer,
    StorageBuffer,
    SampledTexture,
    StorageTexture,
    Sampler,
};

struct BindingInfo
{
    std::string m_Name;
    uint32_t m_SetIndex = 0;
    uint32_t m_Binding = 0;
    ResourceKind m_Kind = ResourceKind::UniformBuffer;
    uint32_t m_ArrayCount = 1;
    ShaderStage m_StageMask = ShaderStage::None;
};

struct PipelineLayoutDesc
{
    std::vector<BindingInfo> m_Bindings;
    std::vector<PushConstantRangeDesc> m_PushConstants;
};

class PipelineLayout
{
public:
    virtual ~PipelineLayout() = default;

    virtual const PipelineLayoutDesc& GetDesc() const = 0;
};

struct VertexAttributeDesc
{
    uint32_t m_Location = 0;
    Format m_Format = Format::Unknown;
    uint32_t m_Offset = 0;
    uint32_t m_BufferSlot = 0;
};

struct VertexBufferLayoutDesc
{
    uint32_t m_Stride = 0;
    bool m_PerInstance = false;
};

struct VertexInputLayoutDesc
{
    std::vector<VertexAttributeDesc> m_Attributes;
    std::vector<VertexBufferLayoutDesc> m_Buffers;
};

class VertexInputLayout
{
public:
    virtual ~VertexInputLayout() = default;

    virtual const VertexInputLayoutDesc& GetDesc() const = 0;
};

enum class IndexType
{
    UInt16,
    UInt32,
};

struct MeshBinding
{
    std::vector<Buffer*> m_VertexBuffers;
    Buffer* m_IndexBuffer = nullptr;
    IndexType m_IndexType = IndexType::UInt32;
};

enum class PrimitiveTopology
{
    TriangleList,
    TriangleStrip,
    LineList,
    LineStrip,
    PointList,
};

enum class CullMode
{
    None,
    Front,
    Back,
};

enum class FrontFace
{
    // Logical front-face selection in the project's public rasterization
    // policy. Backends may translate this to native winding state if viewport
    // transforms alter framebuffer-space winding.
    CW,
    CCW,
};

enum class FillMode
{
    Solid,
    Wireframe,
};

struct RasterState
{
    CullMode m_CullMode = CullMode::Back;
    // Public contract: this is evaluated in the project's logical clip-space
    // policy, not as a promise that every backend will use the same native
    // winding enum value.
    FrontFace m_FrontFace = FrontFace::CCW;
    FillMode m_FillMode = FillMode::Solid;
    bool m_DepthClampEnable = false;
    bool m_DepthBiasEnable = false;
    float m_DepthBiasConstant = 0.0f;
    float m_DepthBiasSlopeFactor = 0.0f;
};

enum class CompareOp
{
    Never,
    Less,
    Equal,
    LessEqual,
    Greater,
    NotEqual,
    GreaterEqual,
    Always,
};

struct DepthStencilState
{
    bool m_DepthTestEnable = false;
    bool m_DepthWriteEnable = false;
    CompareOp m_DepthCompareOp = CompareOp::Less;
};

enum class BlendFactor
{
    Zero,
    One,
    SrcColor,
    OneMinusSrcColor,
    DstColor,
    OneMinusDstColor,
    SrcAlpha,
    OneMinusSrcAlpha,
    DstAlpha,
    OneMinusDstAlpha,
};

enum class BlendOp
{
    Add,
    Subtract,
    ReverseSubtract,
    Min,
    Max,
};

struct BlendState
{
    bool m_BlendEnable = false;
    BlendFactor m_SrcColorFactor = BlendFactor::One;
    BlendFactor m_DstColorFactor = BlendFactor::Zero;
    BlendOp m_ColorBlendOp = BlendOp::Add;
    BlendFactor m_SrcAlphaFactor = BlendFactor::One;
    BlendFactor m_DstAlphaFactor = BlendFactor::Zero;
    BlendOp m_AlphaBlendOp = BlendOp::Add;
    uint8_t m_ColorWriteMask = 0xF;
};

struct GraphicsPipelineDesc
{
    PipelineLayout* m_PipelineLayout = nullptr;
    class ShaderProgram* m_ShaderProgram = nullptr;
    VertexInputLayout* m_VertexInput = nullptr;

    BlendState m_BlendState;
    DepthStencilState m_DepthStencilState;
    RasterState m_RasterState;
    PrimitiveTopology m_Topology = PrimitiveTopology::TriangleList;

    std::vector<Format> m_ColorFormats;
    Format m_DepthFormat = Format::Unknown;
};

struct ComputePipelineDesc
{
    PipelineLayout* m_PipelineLayout = nullptr;
    class ShaderProgram* m_ShaderProgram = nullptr;
};

class ShaderProgram
{
public:
    virtual ~ShaderProgram() = default;

    virtual const ShaderReflectionData& GetReflection() const = 0;
    virtual PipelineLayoutDesc DerivePipelineLayoutDesc() const = 0;
};

class GraphicsPipeline
{
public:
    virtual ~GraphicsPipeline() = default;

    virtual const GraphicsPipelineDesc& GetDesc() const = 0;
};

class ComputePipeline
{
public:
    virtual ~ComputePipeline() = default;

    virtual const ComputePipelineDesc& GetDesc() const = 0;
};
