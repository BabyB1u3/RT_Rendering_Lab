#pragma once

/// @file MetalIndexBuffer.h
/// @brief Metal implementation of IIndexBuffer — wraps a static MTLBuffer of uint32_t indices.

#include <cstdint>
#include <memory>

#include "core/Base.h"
#include "graphics/interfaces/IIndexBuffer.h"

class MetalIndexBuffer : public IIndexBuffer
{
public:
	MetalIndexBuffer(const uint32_t *indices, uint32_t count);
	~MetalIndexBuffer() override;

	MetalIndexBuffer(const MetalIndexBuffer &) = delete;
	MetalIndexBuffer &operator=(const MetalIndexBuffer &) = delete;

	// --- IIndexBuffer ---
	uint32_t GetCount() const override { return m_Count; }

	// --- Metal-internal (call only from .mm files; bridges to id<MTLBuffer>) ---
	void *GetMTLBuffer() const;

private:
	struct Impl;
	std::unique_ptr<Impl> m_Impl;

	uint32_t m_Count = 0;
};
