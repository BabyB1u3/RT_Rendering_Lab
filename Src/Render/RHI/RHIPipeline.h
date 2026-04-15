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
    std::string name;
    uint32_t setIndex = 0;
    uint32_t binding = 0;
    ResourceKind kind = ResourceKind::UniformBuffer;
    uint32_t arrayCount = 1;
    ShaderStage stageMask = ShaderStage::None;
};

struct PipelineLayoutDesc
{
    std::vector<BindingInfo> bindings;
    std::vector<PushConstantRangeDesc> pushConstants;
};

class PipelineLayout
{
public:
    virtual ~PipelineLayout() = default;

    virtual const PipelineLayoutDesc& getDesc() const = 0;
};

struct VertexAttributeDesc
{
    uint32_t location = 0;
    Format format = Format::Unknown;
    uint32_t offset = 0;
    uint32_t bufferSlot = 0;
};

struct VertexBufferLayoutDesc
{
    uint32_t stride = 0;
    bool perInstance = false;
};

struct VertexInputLayoutDesc
{
    std::vector<VertexAttributeDesc> attributes;
    std::vector<VertexBufferLayoutDesc> buffers;
};

class VertexInputLayout
{
public:
    virtual ~VertexInputLayout() = default;

    virtual const VertexInputLayoutDesc& getDesc() const = 0;
};

enum class IndexType
{
    UInt16,
    UInt32,
};

struct MeshBinding
{
    std::vector<Buffer*> vertexBuffers;
    Buffer* indexBuffer = nullptr;
    IndexType indexType = IndexType::UInt32;
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
    CullMode cullMode = CullMode::Back;
    FrontFace frontFace = FrontFace::CCW;
    FillMode fillMode = FillMode::Solid;
    bool depthClampEnable = false;
    bool depthBiasEnable = false;
    float depthBiasConstant = 0.0f;
    float depthBiasSlopeFactor = 0.0f;
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
    bool depthTestEnable = false;
    bool depthWriteEnable = false;
    CompareOp depthCompareOp = CompareOp::Less;
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
    bool blendEnable = false;
    BlendFactor srcColorFactor = BlendFactor::One;
    BlendFactor dstColorFactor = BlendFactor::Zero;
    BlendOp colorBlendOp = BlendOp::Add;
    BlendFactor srcAlphaFactor = BlendFactor::One;
    BlendFactor dstAlphaFactor = BlendFactor::Zero;
    BlendOp alphaBlendOp = BlendOp::Add;
    uint8_t colorWriteMask = 0xF;
};

struct GraphicsPipelineDesc
{
    PipelineLayout* pipelineLayout = nullptr;
    class ShaderProgram* shaderProgram = nullptr;
    VertexInputLayout* vertexInput = nullptr;

    BlendState blendState;
    DepthStencilState depthStencilState;
    RasterState rasterState;
    PrimitiveTopology topology = PrimitiveTopology::TriangleList;

    std::vector<Format> colorFormats;
    Format depthFormat = Format::Unknown;
};

struct ComputePipelineDesc
{
    PipelineLayout* pipelineLayout = nullptr;
    class ShaderProgram* shaderProgram = nullptr;
};

class ShaderProgram
{
public:
    virtual ~ShaderProgram() = default;

    virtual const ShaderReflectionData& getReflection() const = 0;
    virtual PipelineLayoutDesc derivePipelineLayoutDesc() const = 0;
};

class GraphicsPipeline
{
public:
    virtual ~GraphicsPipeline() = default;

    virtual const GraphicsPipelineDesc& getDesc() const = 0;
};

class ComputePipeline
{
public:
    virtual ~ComputePipeline() = default;

    virtual const ComputePipelineDesc& getDesc() const = 0;
};
