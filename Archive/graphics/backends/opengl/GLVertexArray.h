#pragma once

/// @file GLVertexArray.h
/// @brief OpenGL Vertex Array Object (VAO) implementation of IVertexArray using DSA.

#include <cstdint>
#include <vector>

#include "core/Base.h"
#include "graphics/interfaces/IVertexArray.h"

class GLVertexArray : public IVertexArray
{
public:
	GLVertexArray();
	~GLVertexArray() override;

	GLVertexArray(const GLVertexArray &) = delete;
	GLVertexArray &operator=(const GLVertexArray &) = delete;

	GLVertexArray(GLVertexArray &&other) noexcept;
	GLVertexArray &operator=(GLVertexArray &&other) noexcept;

	// --- IVertexArray interface ---
	void Bind() const override;
	void Unbind() const override;

	void AddVertexBuffer(const Ref<IVertexBuffer> &vb) override;
	void SetIndexBuffer(const Ref<IIndexBuffer> &ib) override;
	const Ref<IIndexBuffer> &GetIndexBuffer() const override { return m_IndexBuffer; }

	// --- GL-specific (non-virtual) ---
	uint32_t GetRendererID() const { return m_RendererID; }

private:
	uint32_t m_RendererID = 0;
	uint32_t m_VertexAttribIndex = 0;

	std::vector<Ref<IVertexBuffer>> m_VertexBuffers;
	Ref<IIndexBuffer> m_IndexBuffer;
};
