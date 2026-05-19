#include "Render/RHI/RHIFactory.h"

#include "Core/Diagnostics/Logging/LogCategories.h"
#include "Core/Diagnostics/Logging/LogMacros.h"
#include "Render/RHI/Backends/Vulkan/Device/VulkanDevice.h"

BackendType GetDefaultBackendType()
{
    return BackendType::Vulkan;
}

const char* GetBackendName(BackendType backend)
{
    switch (backend)
    {
        case BackendType::Vulkan:
            return "Vulkan";
    }

    return "Unknown";
}

Scope<Device> CreateDevice()
{
    LOG_INFO_CAT(LogCategory::k_Graphics, "Creating {} RHI device shell", GetBackendName(GetDefaultBackendType()));
    return CreateScope<VulkanDevice>();
}
