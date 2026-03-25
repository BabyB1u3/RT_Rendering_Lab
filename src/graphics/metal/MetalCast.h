#pragma once

/// @file MetalCast.h
/// @brief Safe downcast helper for Metal backend types.
///
/// AsMetal<T>() is the Metal equivalent of AsGL<T>().
/// Use within .mm files wherever an interface pointer must be narrowed to its
/// concrete Metal type to access the native MTL handle.

#include <cassert>

#include "core/Base.h"

template <typename MetalType, typename IType>
MetalType *AsMetal(const Ref<IType> &ptr)
{
	assert(ptr && "AsMetal: null pointer");
#ifndef NDEBUG
	assert(dynamic_cast<MetalType *>(ptr.get()) && "AsMetal: type mismatch – wrong backend?");
#endif
	return static_cast<MetalType *>(ptr.get());
}

template <typename MetalType, typename IType>
MetalType *AsMetal(IType *ptr)
{
	assert(ptr && "AsMetal: null pointer");
#ifndef NDEBUG
	assert(dynamic_cast<MetalType *>(ptr) && "AsMetal: type mismatch – wrong backend?");
#endif
	return static_cast<MetalType *>(ptr);
}
