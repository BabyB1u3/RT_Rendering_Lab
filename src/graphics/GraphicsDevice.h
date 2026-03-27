#pragma once

/// @file GraphicsDevice.h
/// @brief Global accessor for the active graphics device.
///
/// SetDevice() replaces the global Ref<IGraphicsDevice> only - it does NOT perform
/// any rendering state initialization. Backend bring-up (e.g.
/// GetDevice()->GetRenderCommand()->Init()) must be called separately by the caller.
///
/// SetDevice() may be called multiple times (for test teardown/re-init).
/// GetDevice() asserts non-null - crash-early if called before init.

#include "core/Base.h"
#include "graphics/interfaces/IGraphicsDevice.h"

/// Set the active graphics device. Does not initialize rendering state.
void SetDevice(Ref<IGraphicsDevice> device);

/// Get the active graphics device. Asserts non-null.
Ref<IGraphicsDevice> GetDevice();
