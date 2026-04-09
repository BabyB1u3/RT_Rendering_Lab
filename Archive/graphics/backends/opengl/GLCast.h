#pragma once

/// @file GLCast.h
/// @brief Safe downcast helper for GL backend types.
///
/// Within the OpenGL backend, interface pointers (e.g. Ref<IVertexBuffer>) often
/// need to be downcast to their GL-specific type to access the native GL handle.
/// AsGL<T>() centralizes this pattern with an always-on null check and a
/// debug-only dynamic_cast validation.
///
/// Requires: IType has a virtual destructor (all I* interfaces do).
/// Requires: RTTI enabled (default for MSVC/GCC/Clang; needed for debug dynamic_cast).

#include "core/Base.h"
#include "core/diagnostics/Assert.h"

template <typename GLType, typename IType>
GLType *AsGL(const Ref<IType> &ptr)
{
	RTRLAB_ASSERT_MSG(ptr, "AsGL: null pointer");
#ifndef NDEBUG
	RTRLAB_ASSERT_MSG(dynamic_cast<GLType *>(ptr.get()), "AsGL: type mismatch - wrong backend?");
#endif
	return static_cast<GLType *>(ptr.get());
}
