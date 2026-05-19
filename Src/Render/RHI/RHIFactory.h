#pragma once

/// @file RHIFactory.h
/// @brief Public backend-selection and RHI device factory helpers.

#include "Render/RHI/RHIDevice.h"

/// Return the backend selected by the current build configuration.
BackendType GetDefaultBackendType();

/// Human-readable backend name used for diagnostics and logging.
const char* GetBackendName(BackendType backend);

/// Create the Vulkan RHI device shell used by the mainline renderer.
Scope<Device> CreateDevice();

/// Convenience wrapper kept for call sites that spell out the default intent.
inline Scope<Device> CreateDefaultDevice()
{
    return CreateDevice();
}
