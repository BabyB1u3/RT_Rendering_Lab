#pragma once

/// @file MetalVertexBuffer.h
/// @brief Metal implementation of IVertexBuffer — wraps a MTLBuffer.
///
/// StaticDraw  → MTLResourceStorageModeManaged (fast GPU reads, one-time upload)
/// DynamicDraw → MTLResourceStorageModeShared  (CPU-writable every frame)
/// StreamDraw  → same as DynamicDraw

#include <cstdint>
#include <memory>

#include "core/Base.h"
#include "graphics/Buffers.h"
#include "graphics/interfaces/IVertexBuffer.h"

class MetalVertexBuffer : public IVertexBuffer
{
public:
	/// Allocate an empty buffer (DynamicDraw / StreamDraw).
	MetalVertexBuffer(uint32_t size, BufferUsage usage);
	/// Allocate and upload initial data (StaticDraw common).
	MetalVertexBuffer(const void *data, uint32_t size, BufferUsage usage);
	~MetalVertexBuffer() override;

	MetalVertexBuffer(const MetalVertexBuffer &) = delete;
	MetalVertexBuffer &operator=(const MetalVertexBuffer &) = delete;

	// --- IVertexBuffer ---
	void SetData(const void *data, uint32_t size, uint32_t offset = 0) override;
	void SetLayout(const BufferLayout &layout) override { m_Layout = layout; }
	const BufferLayout &GetLayout() const override { return m_Layout; }

	// --- Metal-internal (call only from .mm files; bridges to id<MTLBuffer>) ---
	void *GetMTLBuffer() const;
	uint32_t GetCurrentOffset() const { return 0; } // triple-buffering offset (Phase 2)
	uint32_t GetAllocatedSize() const { return m_AllocatedSize; }

private:
	struct Impl;
	std::unique_ptr<Impl> m_Impl;

	BufferLayout m_Layout;
	BufferUsage  m_Usage          = BufferUsage::StaticDraw;
	uint32_t     m_AllocatedSize  = 0;
};
