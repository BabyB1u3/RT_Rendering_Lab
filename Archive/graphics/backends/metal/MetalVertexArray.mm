#include "MetalVertexArray.h"

#import <Metal/Metal.h>

#include "core/diagnostics/LogCategories.h"
#include "core/diagnostics/LogMacros.h"
#include "graphics/GraphicsDevice.h"
#include "graphics/Buffers.h"
#include "graphics/backends/metal/MetalCast.h"
#include "graphics/backends/metal/MetalGraphicsDevice.h"
#include "graphics/backends/metal/MetalRenderCommand.h"
#include "graphics/backends/metal/MetalShader.h"
#include "graphics/backends/metal/MetalTypes.h"
#include "graphics/backends/metal/MetalVertexBuffer.h"

// ─── Impl ─────────────────────────────────────────────────────────────────────

struct MetalVertexArray::Impl
{
	MTLVertexDescriptor *vertexDescriptor = nil;
	uint32_t             attribIndex      = 0; // running attribute slot counter
};

// ─── Construction ─────────────────────────────────────────────────────────────

MetalVertexArray::MetalVertexArray()
	: m_Impl(std::make_unique<Impl>())
{
	m_Impl->vertexDescriptor = [MTLVertexDescriptor new];
}

MetalVertexArray::~MetalVertexArray() = default;

// ─── IVertexArray ─────────────────────────────────────────────────────────────

void MetalVertexArray::Bind() const
{
	// Register this VAO as the "current" on the Metal render command so that
	// the upcoming DrawIndexed / DrawArrays call can reference our descriptor.
	auto *dev = static_cast<MetalGraphicsDevice *>(GetDevice().get());
	dev->GetMetalRenderCommand()->SetCurrentVAO(const_cast<MetalVertexArray *>(this));
}

void MetalVertexArray::AddVertexBuffer(const Ref<IVertexBuffer> &vb)
{
	const BufferLayout &layout = vb->GetLayout();
	if (layout.GetElements().empty())
	{
		LOG_WARN_CAT(LogCategory::Graphics, "MetalVertexArray::AddVertexBuffer: vertex buffer has no layout");
		return;
	}

	// Vertex buffers are placed at buffer indices [kMetalVertexBufferBase, ...) to avoid
	// conflicting with constant buffers at [0, kMetalVertexBufferBase).
	uint32_t slot       = static_cast<uint32_t>(m_VertexBuffers.size());
	uint32_t bufferSlot = kMetalVertexBufferBase + slot;

	// Per-buffer layout (stride, step function)
	MTLVertexBufferLayoutDescriptor *layoutDesc = m_Impl->vertexDescriptor.layouts[bufferSlot];
	layoutDesc.stride       = layout.GetStride();
	layoutDesc.stepFunction = MTLVertexStepFunctionPerVertex;
	layoutDesc.stepRate     = 1;

	// Per-attribute descriptions
	for (const auto &element : layout)
	{
		if (element.Type == ShaderDataType::Mat3 || element.Type == ShaderDataType::Mat4)
		{
			// Matrices expand into multiple consecutive vector attributes.
			uint32_t vecCount = (element.Type == ShaderDataType::Mat3) ? 3 : 4;
			uint32_t vecSize  = (element.Type == ShaderDataType::Mat3)
			                      ? sizeof(float) * 3
			                      : sizeof(float) * 4;
			MTLVertexFormat fmt = VertexFormatFromShaderDataType(element.Type);

			for (uint32_t r = 0; r < vecCount; ++r)
			{
				auto *attr   = m_Impl->vertexDescriptor.attributes[m_Impl->attribIndex++];
				attr.format      = fmt;
				attr.offset      = element.Offset + r * vecSize;
				attr.bufferIndex = bufferSlot;
			}
		}
		else
		{
			auto *attr   = m_Impl->vertexDescriptor.attributes[m_Impl->attribIndex++];
			attr.format      = VertexFormatFromShaderDataType(element.Type);
			attr.offset      = element.Offset;
			attr.bufferIndex = bufferSlot;
		}
	}

	m_VertexBuffers.push_back(vb);
}

void MetalVertexArray::SetIndexBuffer(const Ref<IIndexBuffer> &ib)
{
	m_IndexBuffer = ib;
}

// ─── Metal-internal ───────────────────────────────────────────────────────────

void *MetalVertexArray::GetMTLVertexDescriptor() const
{
	return (__bridge void *)m_Impl->vertexDescriptor;
}
