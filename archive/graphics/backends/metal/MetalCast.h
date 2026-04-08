#pragma once

/// @file MetalCast.h
/// @brief Safe downcast helper for Metal backend types.
///
/// AsMetal<T>() is the Metal equivalent of AsGL<T>().
/// Use within .mm files wherever an interface pointer must be narrowed to its
/// concrete Metal type to access the native MTL handle.

#include "core/Base.h"
#include "core/diagnostics/Assert.h"

template <typename MetalType, typename IType>
MetalType *AsMetal(const Ref<IType> &ptr)
{
    RTRLAB_ASSERT_MSG(ptr, "AsMetal: null pointer");
#ifndef NDEBUG
    RTRLAB_ASSERT_MSG(dynamic_cast<MetalType *>(ptr.get()), "AsMetal: type mismatch - wrong backend?");
#endif
    return static_cast<MetalType *>(ptr.get());
}

template <typename MetalType, typename IType>
MetalType *AsMetal(IType *ptr)
{
    RTRLAB_ASSERT_MSG(ptr, "AsMetal: null pointer");
#ifndef NDEBUG
    RTRLAB_ASSERT_MSG(dynamic_cast<MetalType *>(ptr), "AsMetal: type mismatch - wrong backend?");
#endif
    return static_cast<MetalType *>(ptr);
}
