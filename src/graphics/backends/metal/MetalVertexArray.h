#pragma once

/// @file MetalVertexArray.h
/// @brief Metal implementation of IVertexArray.
///
/// Metal has no native VAO concept. This class is a software-side descriptor
/// that builds a MTLVertexDescriptor from the added BufferLayouts and stores
/// references to the bound vertex/index buffers.
///
/// Bind() registers this object as the "current VAO" on MetalRenderCommand so
/// that the next DrawIndexed/DrawArrays call can use its descriptor and buffers.

#include <cstdint>
#include <memory>
#include <vector>

#include "core/Base.h"
#include "graphics/interfaces/IVertexArray.h"

class MetalVertexArray : public IVertexArray
{
public:
	MetalVertexArray();
	~MetalVertexArray() override;

	MetalVertexArray(const MetalVertexArray &) = delete;
	MetalVertexArray &operator=(const MetalVertexArray &) = delete;

	// --- IVertexArray ---
	void Bind() const override;
	void Unbind() const override {}

	void AddVertexBuffer(const Ref<IVertexBuffer> &vb) override;
	void SetIndexBuffer(const Ref<IIndexBuffer> &ib) override;
	const Ref<IIndexBuffer> &GetIndexBuffer() const override { return m_IndexBuffer; }

	// --- Metal-internal (call only from .mm files) ---
	/// Returns MTLVertexDescriptor* as void*.
	void *GetMTLVertexDescriptor() const;
	const std::vector<Ref<IVertexBuffer>> &GetVertexBuffers() const { return m_VertexBuffers; }

private:
	struct Impl;
	std::unique_ptr<Impl> m_Impl;

	std::vector<Ref<IVertexBuffer>> m_VertexBuffers;
	Ref<IIndexBuffer>               m_IndexBuffer;
};
