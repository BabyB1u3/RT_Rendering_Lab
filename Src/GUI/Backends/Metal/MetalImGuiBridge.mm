#include "GUI/Backends/Metal/MetalImGuiBridge.h"

#include <imgui.h>
#include <imgui_impl_metal.h>

#import <Metal/Metal.h>

namespace
{
MTLRenderPassDescriptor* CreateImGuiRenderPassDescriptor(id<MTLTexture> drawableTexture)
{
    if (!drawableTexture)
        return nil;

    MTLRenderPassDescriptor* renderPassDescriptor = [MTLRenderPassDescriptor new];
    renderPassDescriptor.colorAttachments[0].texture = drawableTexture;
    renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionLoad;
    renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
    return renderPassDescriptor;
}
} // namespace

namespace MetalImGuiBridge
{
void Init(void* mtlDevice)
{
    ImGui_ImplMetal_Init((__bridge id<MTLDevice>)mtlDevice);
}

void Shutdown()
{
    ImGui_ImplMetal_Shutdown();
}

void NewFrame(void* drawableTexture)
{
    MTLRenderPassDescriptor* renderPassDescriptor =
        CreateImGuiRenderPassDescriptor((__bridge id<MTLTexture>)drawableTexture);
    if (!renderPassDescriptor)
        return;

    ImGui_ImplMetal_NewFrame(renderPassDescriptor);
}

void RenderDrawData(void* drawData, void* commandBuffer, void* drawableTexture)
{
    if (!drawData || !commandBuffer || !drawableTexture)
        return;

    id<MTLCommandBuffer> metalCommandBuffer = (__bridge id<MTLCommandBuffer>)commandBuffer;
    MTLRenderPassDescriptor* renderPassDescriptor =
        CreateImGuiRenderPassDescriptor((__bridge id<MTLTexture>)drawableTexture);
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
