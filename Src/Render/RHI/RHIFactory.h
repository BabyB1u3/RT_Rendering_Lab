#pragma once

/// @file RHIFactory.h
/// @brief Public backend-selection and RHI device factory helpers.

#include "Render/RHI/RHIDevice.h"

/// Return the backend selected by the current build configuration.
BackendType GetDefaultBackendType();

/// Human-readable backend name used for diagnostics and logging.
const char* GetBackendName(BackendType backend);

/// Create a backend-specific RHI device shell for the requested backend.
Scope<Device> CreateDevice(BackendType backend);

/// Convenience wrapper that creates the device matching the active build backend.
inline Scope<Device> CreateDefaultDevice()
{
    return CreateDevice(GetDefaultBackendType());
}
