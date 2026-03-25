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
///   at buffer index (kUniformBaseSlot + binding), independent of reflection.
///
/// Uses pImpl — no Metal types visible in this header.

#include <cstdint>
#include <memory>
#include <string>

#include <glm/glm.hpp>

#include "core/Base.h"
#include "graphics/RenderTypes.h"
#include "graphics/interface/IShader.h"

/// Buffer index offset for SetUniformBlock uploads.
/// Vertex buffer slots 0..N-1 are used by vertex data.
/// Uniform blocks start at this index to avoid conflicts.
static constexpr uint32_t kUniformBaseSlot = 16;

class MetalShader : public IShader
{
public:
	~MetalShader() override;

	MetalShader(const MetalShader &) = delete;
	MetalShader &operator=(const MetalShader &) = delete;

	// --- IShader ---
	void Bind()   const override;
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
	/// @param colorPixelFormat  MTLPixelFormat cast to uint32_t
	/// @param depthPixelFormat  MTLPixelFormat cast to uint32_t
	/// @param ps                immutable pipeline state (blend, depth, cull)
	/// @return id<MTLRenderPipelineState> as void*, or nullptr on error
	void *GetOrCreatePSO(void *mtlDevice, void *mtlVertDescriptor,
	                     uint32_t colorPixelFormat, uint32_t depthPixelFormat,
	                     const PipelineState &ps);

	/// Upload staging buffers and explicit uniform blocks to the current encoder.
	/// @param mtlEncoder id<MTLRenderCommandEncoder> as void*
	void FlushUniforms(void *mtlEncoder);

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
