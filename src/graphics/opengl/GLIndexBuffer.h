#pragma once

/// @file GLIndexBuffer.h
/// @brief OpenGL implementation of IIndexBuffer using DSA.

#include <cstdint>

#include "graphics/interface/IIndexBuffer.h"

class GLIndexBuffer : public IIndexBuffer
{
public:
	GLIndexBuffer(const uint32_t *indices, uint32_t count);
	~GLIndexBuffer() override;

	GLIndexBuffer(const GLIndexBuffer &) = delete;
	GLIndexBuffer &operator=(const GLIndexBuffer &) = delete;

	GLIndexBuffer(GLIndexBuffer &&other) noexcept;
	GLIndexBuffer &operator=(GLIndexBuffer &&other) noexcept;

	// --- IIndexBuffer interface ---
	uint32_t GetCount() const override { return m_Count; }

	// --- GL-specific (non-virtual) ---
	uint32_t GetRendererID() const { return m_RendererID; }

private:
	uint32_t m_RendererID = 0;
	uint32_t m_Count = 0;
};
