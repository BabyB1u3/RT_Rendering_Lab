#include "GUI/Backends/Vulkan/VulkanImGuiBridge.h"

#include <algorithm>

#include <imgui.h>
#include <imgui_impl_vulkan.h>

#include "Core/Diagnostics/Assert/Assert.h"
#include "Render/RHI/Backends/Vulkan/Command/VulkanCommandList.h"
#include "Render/RHI/Backends/Vulkan/Common/VulkanConversions.h"
#include "Render/RHI/Backends/Vulkan/Device/VulkanDevice.h"
#include "Render/RHI/RHICommandList.h"
#include "Render/RHI/RHIResources.h"

using namespace VulkanRHI;

namespace
{
VulkanDevice& GetVulkanDevice(Device& device)
{
    auto* vulkanDevice = dynamic_cast<VulkanDevice*>(&device);
    RTRLAB_ASSERT_MSG(vulkanDevice != nullptr, "Vulkan ImGui bridge requires a VulkanDevice.");
    return *vulkanDevice;
}

VulkanCommandList& GetVulkanCommandList(CommandList* commandList)
{
    auto* vulkanCommandList = dynamic_cast<VulkanCommandList*>(commandList);
    RTRLAB_ASSERT_MSG(vulkanCommandList != nullptr, "Vulkan ImGui bridge requires a VulkanCommandList.");
    return *vulkanCommandList;
}

void CheckImGuiVkResult(VkResult result)
{
    CheckVk(result, "Dear ImGui Vulkan backend");
}
} // namespace

namespace VulkanImGuiBridge
{
void Init(Device& device, Swapchain& swapchain)
{
    VulkanDevice& vulkanDevice = GetVulkanDevice(device);
    const VkFormat colorFormat = ToVkFormat(swapchain.GetFormat());

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.ApiVersion = VK_API_VERSION_1_3;
    initInfo.Instance = vulkanDevice.GetVkInstance();
    initInfo.PhysicalDevice = vulkanDevice.GetVkPhysicalDevice();
    initInfo.Device = vulkanDevice.GetVkDevice();
    initInfo.QueueFamily = vulkanDevice.GetGraphicsQueueFamily();
    initInfo.Queue = vulkanDevice.GetGraphicsQueue();
    initInfo.DescriptorPoolSize = 1024;
    initInfo.MinImageCount = std::max(swapchain.GetImageCount(), 2u);
    initInfo.ImageCount = std::max(swapchain.GetImageCount(), initInfo.MinImageCount);
    initInfo.UseDynamicRendering = true;
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &colorFormat;
    initInfo.CheckVkResultFn = CheckImGuiVkResult;

    RTRLAB_ASSERT_MSG(ImGui_ImplVulkan_Init(&initInfo), "Failed to initialize Dear ImGui Vulkan backend.");
}

void Shutdown()
{
    ImGui_ImplVulkan_Shutdown();
}

void NewFrame()
{
    ImGui_ImplVulkan_NewFrame();
}

void RenderDrawData(void* drawData, CommandList* commandList, TextureView* targetView)
{
    if (!drawData)
        return;

    RTRLAB_ASSERT_MSG(targetView != nullptr, "Vulkan ImGui rendering requires a target texture view.");
    VulkanCommandList& vulkanCommandList = GetVulkanCommandList(commandList);

    Texture* targetTexture = targetView->GetTexture();
    RTRLAB_ASSERT_MSG(targetTexture != nullptr, "Vulkan ImGui target view must reference a texture.");

    RenderingInfo renderingInfo;
    renderingInfo.m_ColorAttachments.push_back(ColorAttachmentInfo{
        targetView,
        LoadOp::Load,
        StoreOp::Store,
        {},
    });
    renderingInfo.m_RenderArea = Rect2D{
        0,
        0,
        targetTexture->GetDesc().m_Extent.m_Width,
        targetTexture->GetDesc().m_Extent.m_Height,
    };

    vulkanCommandList.BeginRendering(renderingInfo);
    ImGui_ImplVulkan_RenderDrawData(static_cast<ImDrawData*>(drawData), vulkanCommandList.GetVkCommandBuffer());
    vulkanCommandList.EndRendering();
}
} // namespace VulkanImGuiBridge
