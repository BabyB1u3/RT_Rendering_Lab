#pragma once

/// @file IVertexArray.h
/// @brief Abstract interface for vertex array objects. Backend-agnostic.
///
/// Metal/Vulkan have no native VAO concept. Implementations on those backends
/// store buffer bindings and apply them at draw time.

#include "core/Base.h"

class IVertexBuffer;
class IIndexBuffer;

class IVertexArray
{
public:
	virtual ~IVertexArray() = default;

	virtual void Bind() const = 0;
	virtual void Unbind() const = 0;

	virtual void AddVertexBuffer(const Ref<IVertexBuffer> &vb) = 0;
	virtual void SetIndexBuffer(const Ref<IIndexBuffer> &ib) = 0;
	virtual const Ref<IIndexBuffer> &GetIndexBuffer() const = 0;
};
