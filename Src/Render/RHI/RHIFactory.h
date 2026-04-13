#pragma once

/// @file RHIFactory.h
/// @brief Public backend-selection and RHI device factory helpers.

#include "Render/RHI/RHIDevice.h"

/// Return the backend selected by the current build configuration.
BackendType getDefaultBackendType();

/// Human-readable backend name used for diagnostics and logging.
const char *getBackendName(BackendType backend);

/// Create a backend-specific RHI device shell for the requested backend.
Scope<Device> createDevice(BackendType backend);

/// Convenience wrapper that creates the device matching the active build backend.
inline Scope<Device> createDefaultDevice()
{
    return createDevice(getDefaultBackendType());
}

