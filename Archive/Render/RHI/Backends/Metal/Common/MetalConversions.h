#pragma once

/// @file MetalConversions.h
/// @brief Metal enum/flag conversions, debug-name helpers, and shader argument-
/// buffer planning utilities shared across the backend.

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Render/RHI/Backends/Metal/Common/MetalCommon.h"
#include "Render/RHI/RHICommandList.h"
#include "Render/RHI/RHIPipeline.h"
#include "Render/RHI/RHIResources.h"
#include "Render/Shader/ShaderTypes.h"

namespace MetalRHI
{
MTLStorageMode ToMetalStorageMode(MemoryUsage memoryUsage);
MTLResourceOptions ToMetalBufferResourceOptions(MemoryUsage memoryUsage);
MTLPixelFormat ToMetalPixelFormat(Format format);
MTLTextureType ToMetalTextureType(TextureType type);
MTLSamplerMinMagFilter ToMetalFilter(FilterMode mode);
MTLSamplerMipFilter ToMetalMipFilter(MipFilterMode mode);
MTLSamplerAddressMode ToMetalAddressMode(AddressMode mode);
MTLLoadAction ToMetalLoadAction(LoadOp loadOp);
MTLStoreAction ToMetalStoreAction(StoreOp storeOp);
MTLCompareFunction ToMetalCompareFunction(CompareOp compareOp);
bool IsDepthFormat(Format format);
bool HasStencilComponent(Format format);
uint32_t GetFormatBytesPerPixel(Format format);
bool HasDebugName(const char* debugName);
NSString* MakeNSString(const char* debugName);
NSString* MakeNSString(const std::vector<uint8_t>& utf8Bytes);
void SetMetalDebugLabel(id<MTLResource> resource, const char* debugName);
std::string MakeTextureViewDebugName(const Texture& texture);
MTLVertexFormat ToMetalVertexFormat(Format format);
MTLPrimitiveType ToMetalPrimitiveType(PrimitiveTopology topology);
MTLIndexType ToMetalIndexType(IndexType indexType);
MTLWinding ToMetalWinding(FrontFace frontFace);
MTLCullMode ToMetalCullMode(CullMode cullMode);
MTLTriangleFillMode ToMetalTriangleFillMode(FillMode fillMode);

struct MetalBindingPlanEntry
{
    std::string m_Name;
    uint32_t m_Binding = 0;
    uint32_t m_ArgumentIndex = 0;
    uint32_t m_ArrayCount = 1;
    ResourceKind m_Kind = ResourceKind::UniformBuffer;
};

struct MetalStageBindingPlan
{
    ShaderStage m_Stage = ShaderStage::None;
    uint32_t m_ArgumentBufferSlot = 0;
    std::vector<MetalBindingPlanEntry> m_Entries;
};

struct MetalSetBindingPlan
{
    uint32_t m_SetIndex = 0;
    std::vector<MetalStageBindingPlan> m_StagePlans;
};

std::string RewriteMetalShaderSourceForArgumentBuffers(const CompiledShaderProgramDesc& desc,
                                                       const CompiledShaderBlob& blob,
                                                       std::string_view sourceText,
                                                       std::vector<MetalSetBindingPlan>* outUsedSetPlans = nullptr);
void MergeUsedMetalSetBindingPlans(std::vector<MetalSetBindingPlan>* destination,
                                   std::vector<MetalSetBindingPlan> source);
std::vector<MetalSetBindingPlan> BuildMetalSetBindingPlans(const PipelineLayoutDesc& desc);
uint32_t ComputeVertexBufferSlotBase(const std::vector<MetalSetBindingPlan>& plans);
} // namespace MetalRHI
