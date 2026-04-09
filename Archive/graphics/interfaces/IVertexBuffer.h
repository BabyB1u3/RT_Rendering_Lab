#pragma once

/// @file IVertexBuffer.h
/// @brief Abstract interface for GPU vertex buffers. Backend-agnostic.

#include <cstdint>

#include "graphics/Buffers.h" // BufferLayout

class IVertexBuffer
{
public:
	virtual ~IVertexBuffer() = default;

	virtual void SetData(const void *data, uint32_t size, uint32_t offset = 0) = 0;

	virtual void SetLayout(const BufferLayout &layout) = 0;
	virtual const BufferLayout &GetLayout() const = 0;
};
