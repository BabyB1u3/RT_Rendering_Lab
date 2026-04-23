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
};

enum class ShaderStage : uint32_t
{
    None = 0,
    Vertex = BIT(0),
    Fragment = BIT(1),
    Compute = BIT(2),
    All = BIT(0) | BIT(1) | BIT(2),
};

constexpr ShaderStage operator|(ShaderStage lhs, ShaderStage rhs)
{
    return static_cast<ShaderStage>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

constexpr ShaderStage operator&(ShaderStage lhs, ShaderStage rhs)
{
    return static_cast<ShaderStage>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}

constexpr ShaderStage& operator|=(ShaderStage& lhs, ShaderStage rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

struct ShaderEntryPointDesc
{
    std::string m_ModuleName;
    std::string m_EntryName;
    ShaderStage m_Stage = ShaderStage::None;
};

struct ShaderSourceDesc
{
    std::vector<ShaderEntryPointDesc> m_Entries;
    std::vector<std::string> m_Defines;
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
    StorageTexture,
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
    std::string m_Name;
    ReflectedTypeKind m_TypeKind = ReflectedTypeKind::Struct;

    uint32_t m_Offset = 0;
    uint32_t m_Size = 0;
    uint32_t m_Alignment = 0;
    uint32_t m_ArrayStride = 0;
    uint32_t m_MatrixStride = 0;
    LayoutConvention m_LayoutConvention = LayoutConvention::Std430;

    uint32_t m_SetIndex = 0;
    uint32_t m_Binding = 0;
    uint32_t m_ArrayCount = 1;
    ShaderStage m_StageMask = ShaderStage::None;

    std::vector<ReflectedField> m_Children;
};

struct PushConstantRangeDesc
{
    ShaderStage m_StageMask = ShaderStage::None;
    uint32_t m_Offset = 0;
    uint32_t m_Size = 0;
};

struct ShaderReflectionData
{
    std::vector<ReflectedField> m_Globals;
    std::vector<PushConstantRangeDesc> m_PushConstants;
};

struct CompiledShaderBlob
{
    BackendType m_Backend = BackendType::Vulkan;
    ShaderStage m_Stage = ShaderStage::None;
    std::string m_EntryPoint;
    std::vector<uint8_t> m_Code;
    MetalCodeFormat m_MetalCodeFormat = MetalCodeFormat::MslSource;
};

struct CompiledShaderProgramDesc
{
    std::vector<CompiledShaderBlob> m_Blobs;
    ShaderReflectionData m_Reflection;
};
