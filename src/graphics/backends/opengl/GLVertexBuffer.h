#pragma once

/// @file GLVertexBuffer.h
/// @brief OpenGL implementation of IVertexBuffer using DSA.

#include <cstdint>

#include "core/Base.h"
#include "graphics/Buffers.h"
#include "graphics/interfaces/IVertexBuffer.h"

class GLVertexBuffer : public IVertexBuffer
{
public:
	GLVertexBuffer(uint32_t size, BufferUsage usage = BufferUsage::DynamicDraw);
	GLVertexBuffer(const void *data, uint32_t size, BufferUsage usage = BufferUsage::StaticDraw);
	~GLVertexBuffer() override;

	GLVertexBuffer(const GLVertexBuffer &) = delete;
	GLVertexBuffer &operator=(const GLVertexBuffer &) = delete;

	GLVertexBuffer(GLVertexBuffer &&other) noexcept;
	GLVertexBuffer &operator=(GLVertexBuffer &&other) noexcept;

	// --- IVertexBuffer interface ---
	void SetData(const void *data, uint32_t size, uint32_t offset = 0) override;
	void SetLayout(const BufferLayout &layout) override { m_Layout = layout; }
	const BufferLayout &GetLayout() const override { return m_Layout; }

	// --- GL-specific (non-virtual) ---
	uint32_t GetRendererID() const { return m_RendererID; }

private:
	uint32_t m_RendererID = 0;
	BufferLayout m_Layout;
	BufferUsage m_Usage = BufferUsage::StaticDraw;
};
