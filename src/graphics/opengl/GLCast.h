#pragma once

/// @file GLCast.h
/// @brief Safe downcast helper for GL backend types.
///
/// Within the OpenGL backend, interface pointers (e.g. Ref<IVertexBuffer>) often
/// need to be downcast to their GL-specific type to access the native GL handle.
/// AsGL<T>() centralizes this pattern with a debug-mode dynamic_cast check.
///
/// Requires: IType has a virtual destructor (all I* interfaces do).
/// Requires: RTTI enabled (default for MSVC/GCC/Clang; needed for debug dynamic_cast).

#include <cassert>

#include "core/Base.h"

template <typename GLType, typename IType>
GLType *AsGL(const Ref<IType> &ptr)
{
	assert(ptr && "AsGL: null pointer");
#ifndef NDEBUG
	assert(dynamic_cast<GLType *>(ptr.get()) && "AsGL: type mismatch - wrong backend?");
#endif
	return static_cast<GLType *>(ptr.get());
}
