#include "Render/RHI/RHIFactory.h"

#include "Core/Diagnostics/Assert/Assert.h"
#include "Core/Diagnostics/Logging/LogCategories.h"
#include "Core/Diagnostics/Logging/LogMacros.h"
#if defined(GLAB_BACKEND_METAL)
#include "Render/RHI/Backends/Metal/MetalDevice.h"
#endif
#if defined(GLAB_BACKEND_OPENGL)
#include "Render/RHI/Backends/OpenGL/OpenGLDevice.h"
#endif
#if defined(GLAB_BACKEND_VULKAN)
#include "Render/RHI/Backends/Vulkan/VulkanDevice.h"
#endif

BackendType GetDefaultBackendType()
{
#if defined(GLAB_BACKEND_OPENGL)
    return BackendType::OpenGL;
#elif defined(GLAB_BACKEND_METAL)
    return BackendType::Metal;
#elif defined(GLAB_BACKEND_VULKAN)
    return BackendType::Vulkan;
#else
#error No RHI backend selected for this build configuration.
#endif
}

const char* GetBackendName(BackendType backend)
{
    switch (backend)
    {
        case BackendType::Vulkan:
            return "Vulkan";
        case BackendType::Metal:
            return "Metal";
        case BackendType::OpenGL:
            return "OpenGL";
    }

    return "Unknown";
}

Scope<Device> CreateDevice(BackendType backend)
{
    LOG_INFO_CAT(LogCategory::Graphics, "Creating {} RHI device shell", GetBackendName(backend));

    switch (backend)
    {
        case BackendType::Vulkan:
#if defined(GLAB_BACKEND_VULKAN)
            return CreateScope<VulkanDevice>();
#else
            break;
#endif
        case BackendType::Metal:
#if defined(GLAB_BACKEND_METAL)
            return CreateScope<MetalDevice>();
#else
            break;
#endif
        case BackendType::OpenGL:
#if defined(GLAB_BACKEND_OPENGL)
            return CreateScope<OpenGLDevice>();
#else
            break;
#endif
    }

    RTRLAB_ASSERT_MSG(false, "Requested RHI backend is not compiled into this build.");
    return {};
}
