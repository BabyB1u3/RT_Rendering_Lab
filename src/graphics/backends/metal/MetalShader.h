#pragma once

/// @file MetalShader.h
/// @brief Metal implementation of IShader.
///
/// Loads pre-compiled MSL from the shader directory (Slang → MSL pipeline),
/// creates MTLLibrary + MTLFunction objects, and manages a pipeline state
/// object (PSO) cache keyed by vertex layout, attachment formats, and blend state.
///
/// Uniform data strategy (Phase 1):
///   Named setters (SetMat4, etc.) write into per-stage CPU staging buffers.
///   FlushUniforms() uploads them via setVertexBytes / setFragmentBytes before
///   each draw call. Offset mapping is loaded from a JSON sidecar produced by
///   slangc -dump-reflection; if absent, named setters are silent no-ops.
///
///   SetUniformBlock(binding, data, size) uploads a raw block to both stages
///   at buffer index (kUniformBaseSlot + binding) = binding, matching [[buffer(N)]].
///
/// Binding index convention - see kUniformBaseSlot / kMetalVertexBufferBase below.
///
/// Uses pImpl - no Metal types visible in this header.

#include <cstdint>
#include <array>
#include <memory>
#include <string>

#include <glm/glm.hpp>

#include "core/Base.h"
#include "graphics/RenderTypes.h"
#include "graphics/interfaces/IShader.h"

/// Binding index convention - Vulkan descriptor model as canonical abstraction.
///
/// SetUniformBlock(N) / SetTexture(N) use the [vk::binding(N, 0)] index directly,
/// consistent across all three backends:
///   OpenGL  : UBO → glBindBufferBase(GL_UNIFORM_BUFFER, N)
///             Tex → glBindTextureUnit(N, ...)
///   Metal   : CB  → setVertexBytes / setFragmentBytes at index N  ([[buffer(N)]])
///             Tex → setFragmentTexture at index N                  ([[texture(N)]])
///   Vulkan  : descriptor set 0, binding N
///
/// Metal buffer index partition (vertex stage shares buffer indices with CBs):
///   [0, kMetalVertexBufferBase)   → constant / uniform buffers ([[buffer(N)]])
///   [kMetalVertexBufferBase, ...) → vertex data (MTLVertexDescriptor + setVertexBuffer)
static constexpr uint32_t kUniformBaseSlot = 0;
static constexpr uint32_t kMetalVertexBufferBase = 8;

class MetalShader : public IShader
{
public:
	~MetalShader() override;

	MetalShader(const MetalShader &) = delete;
	MetalShader &operator=(const MetalShader &) = delete;

	// --- IShader ---
	void Bind() const override;
	void Unbind() const override {}

	const std::string &GetName() const override;

	void SetInt(const std::string &name, int value) override;
	void SetIntArray(const std::string &name, const int *values, uint32_t count) override;
	void SetBool(const std::string &name, bool value) override;
	void SetFloat(const std::string &name, float value) override;
	void SetFloat2(const std::string &name, const glm::vec2 &value) override;
	void SetFloat3(const std::string &name, const glm::vec3 &value) override;
	void SetFloat4(const std::string &name, const glm::vec4 &value) override;
	void SetMat3(const std::string &name, const glm::mat3 &value) override;
	void SetMat4(const std::string &name, const glm::mat4 &value) override;
	void SetUniformBlock(uint32_t binding, const void *data, uint32_t size) override;

	// --- Metal-internal (call only from .mm files) ---

	/// Look up or create a cached MTLRenderPipelineState.
	/// @param mtlDevice         id<MTLDevice>        as void*
	/// @param mtlVertDescriptor MTLVertexDescriptor* as void*
	/// @param colorPixelFormats Up to 4 MTLPixelFormats cast to uint32_t
	/// @param colorAttachmentCount Number of active color attachments
	/// @param depthPixelFormat  MTLPixelFormat cast to uint32_t
	/// @param ps                immutable pipeline state (blend, depth, cull)
	/// @return id<MTLRenderPipelineState> as void*, or nullptr on error
	void *GetOrCreatePSO(void *mtlDevice, void *mtlVertDescriptor,
						 const std::array<uint32_t, 4> &colorPixelFormats,
						 uint32_t colorAttachmentCount, uint32_t depthPixelFormat,
						 const PipelineState &ps);

	/// Upload staging buffers and explicit uniform blocks to the current encoder.
	/// @param mtlEncoder id<MTLRenderCommandEncoder> as void*
	void FlushUniforms(void *mtlEncoder);

	/// Number of UBO bindings that precede textures in the GLSL/Vulkan binding layout.
	/// Slang MSL assigns texture indices 0-based independently, so the Metal backend
	/// subtracts this offset when translating C++ SetTexture(slot) → [[texture(N)]]:
	///   Metal texture index = slot - GetTextureBindingBase()
	/// Default: 1 (one GlobalParams CB at binding 0; textures start at binding 1).
	/// Overridable via the reflection sidecar ("textureBindingBase" field).
	uint32_t GetTextureBindingBase() const;

	// Factories (used by MetalGraphicsDevice)
	static Ref<MetalShader> CreateFromCompiledMSL(const std::string &name);
	static Ref<MetalShader> CreateFromMSLSource(const std::string &name,
												const std::string &source);

private:
	MetalShader() = default;

	void WriteToStagingBuffer(const std::string &name, const void *data, uint32_t size);

	struct Impl;
	std::unique_ptr<Impl> m_Impl;
};
