#include "GUI/Backends/Metal/MetalImGuiBridge.h"

#include <imgui.h>
#include <imgui_impl_metal.h>

#include "Core/Diagnostics/Assert/Assert.h"
#include "Render/RHI/Backends/Metal/Device/MetalDevice.h"
#include "Render/RHI/Backends/Metal/Resources/MetalTexture.h"

#import <Metal/Metal.h>

namespace
{
MTLRenderPassDescriptor* CreateImGuiRenderPassDescriptor(id<MTLTexture> drawableTexture)
{
    if (!drawableTexture)
        return nil;

    MTLRenderPassDescriptor* renderPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
    renderPassDescriptor.colorAttachments[0].texture = drawableTexture;
    renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionLoad;
    renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
    return renderPassDescriptor;
}

MetalDevice& GetMetalDevice(Device& device)
{
    auto* metalDevice = dynamic_cast<MetalDevice*>(&device);
    RTRLAB_ASSERT_MSG(metalDevice != nullptr, "Metal ImGui bridge requires a MetalDevice.");
    return *metalDevice;
}
} // namespace

namespace MetalImGuiBridge
{
void Init(Device& device)
{
    MetalDevice& metalDevice = GetMetalDevice(device);
    RTRLAB_ASSERT_MSG(metalDevice.GetData() != nullptr && metalDevice.GetData()->m_Device != nil,
                      "Metal ImGui bridge requires an initialized Metal device.");
    ImGui_ImplMetal_Init(metalDevice.GetData()->m_Device);
}

void Shutdown()
{
    ImGui_ImplMetal_Shutdown();
}

void NewFrame(Texture* drawableTexture)
{
    MTLRenderPassDescriptor* renderPassDescriptor =
        CreateImGuiRenderPassDescriptor(GetMetalTextureFromTexture(drawableTexture));
    if (!renderPassDescriptor)
        return;

    ImGui_ImplMetal_NewFrame(renderPassDescriptor);
}

void RenderDrawData(void* drawData, Device& device, Texture* drawableTexture)
{
    if (!drawData || !drawableTexture)
        return;

    MetalDevice& metalDevice = GetMetalDevice(device);
    RTRLAB_ASSERT_MSG(metalDevice.GetData() != nullptr && metalDevice.GetData()->m_CurrentCommandBuffer != nil,
                      "Metal ImGui bridge requires an active Metal command buffer.");

    id<MTLCommandBuffer> metalCommandBuffer = metalDevice.GetData()->m_CurrentCommandBuffer;
    MTLRenderPassDescriptor* renderPassDescriptor =
        CreateImGuiRenderPassDescriptor(GetMetalTextureFromTexture(drawableTexture));
    if (!renderPassDescriptor)
        return;

    id<MTLRenderCommandEncoder> renderEncoder =
        [metalCommandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
    if (!renderEncoder)
        return;

    [renderEncoder pushDebugGroup:@"Dear ImGui"];
    ImGui_ImplMetal_RenderDrawData(static_cast<ImDrawData*>(drawData), metalCommandBuffer, renderEncoder);
    [renderEncoder popDebugGroup];
    [renderEncoder endEncoding];
}
} // namespace MetalImGuiBridge
