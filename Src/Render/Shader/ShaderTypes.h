#pragma once

/// @file ShaderTypes.h
/// @brief Public shader-system boundary types shared with the RHI.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "Core/Util/Base.h"

enum class BackendType
{
    Vulkan,
    Metal,
    OpenGL,
};

enum class ShaderStage : uint32_t
{
    None     = 0,
    Vertex   = BIT(0),
    Fragment = BIT(1),
    Compute  = BIT(2),
    All      = BIT(0) | BIT(1) | BIT(2),
};

constexpr ShaderStage operator|(ShaderStage lhs, ShaderStage rhs)
{
    return static_cast<ShaderStage>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

constexpr ShaderStage operator&(ShaderStage lhs, ShaderStage rhs)
{
    return static_cast<ShaderStage>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}

constexpr ShaderStage &operator|=(ShaderStage &lhs, ShaderStage rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

struct ShaderEntryPointDesc
{
    std::string moduleName;
    std::string entryName;
    ShaderStage stage = ShaderStage::None;
};

struct ShaderSourceDesc
{
    std::vector<ShaderEntryPointDesc> entries;
    std::vector<std::string> defines;
};

enum class MetalCodeFormat
{
    MslSource,
    Metallib,
};

enum class ReflectedTypeKind
{
    Struct,
    ConstantData,
    Texture,
    Sampler,
    Buffer,
    ParameterBlock,
};

enum class LayoutConvention
{
    Std430,
    Std140,
    Scalar,
};

struct ReflectedField
{
    std::string name;
    ReflectedTypeKind typeKind = ReflectedTypeKind::Struct;

    uint32_t offset = 0;
    uint32_t size = 0;
    uint32_t alignment = 0;
    uint32_t arrayStride = 0;
    uint32_t matrixStride = 0;
    LayoutConvention layoutConvention = LayoutConvention::Std430;

    uint32_t setIndex = 0;
    uint32_t binding = 0;
    uint32_t arrayCount = 1;
    ShaderStage stageMask = ShaderStage::None;

    std::vector<ReflectedField> children;
};

struct PushConstantRangeDesc
{
    ShaderStage stageMask = ShaderStage::None;
    uint32_t offset = 0;
    uint32_t size = 0;
};

struct ShaderReflectionData
{
    std::vector<ReflectedField> globals;
    std::vector<PushConstantRangeDesc> pushConstants;
};

struct CompiledShaderBlob
{
    BackendType backend = BackendType::Vulkan;
    ShaderStage stage = ShaderStage::None;
    std::vector<uint8_t> code;
    MetalCodeFormat metalCodeFormat = MetalCodeFormat::MslSource;
};

struct CompiledShaderProgramDesc
{
    std::vector<CompiledShaderBlob> blobs;
    ShaderReflectionData reflection;
};
