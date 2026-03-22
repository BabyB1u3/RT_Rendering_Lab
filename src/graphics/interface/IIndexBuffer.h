#pragma once

/// @file IIndexBuffer.h
/// @brief Abstract interface for GPU index buffers. Backend-agnostic.

#include <cstdint>

class IIndexBuffer
{
public:
	virtual ~IIndexBuffer() = default;

	virtual uint32_t GetCount() const = 0;
};
