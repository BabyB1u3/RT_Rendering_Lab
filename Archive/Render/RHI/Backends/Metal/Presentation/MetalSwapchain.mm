#include "Render/RHI/Backends/Metal/Presentation/MetalSwapchain.h"

#include <algorithm>

#include "Core/Diagnostics/Assert/Assert.h"
#include "Render/RHI/Backends/Common/RHIShellCommon.h"
#include "Render/RHI/Backends/Metal/Common/MetalCommon.h"
#include "Render/RHI/Backends/Metal/Common/MetalConversions.h"
#include "Render/RHI/Backends/Metal/Device/MetalDevice.h"
#include "Render/RHI/Backends/Metal/Resources/MetalTexture.h"

using namespace MetalRHI;

MetalSwapchain::MetalSwapchain(MetalDevice& device,
                               const SwapchainDesc& desc,
                               const NativeWindowHandle& nativeWindowHandle)
    : m_Device(device),
      m_Desc(RHIInternal::SanitizeSwapchainDesc(desc)),
      m_NativeWindowHandle(nativeWindowHandle),
      m_Data(new MetalSwapchainData())
{
    RTRLAB_ASSERT_MSG(RHIInternal::IsNativeWindowHandleValid(nativeWindowHandle),
                      "Native window handle is incomplete.");
    RTRLAB_ASSERT_MSG(nativeWindowHandle.m_Layer != nullptr, "Metal swapchain creation requires a valid CAMetalLayer.");

    m_Data->m_Layer = [(__bridge CAMetalLayer*)nativeWindowHandle.m_Layer retain];
    m_Data->m_Layer.device = m_Device.GetData()->m_Device;
    m_Data->m_Layer.pixelFormat = ToMetalPixelFormat(m_Desc.m_Format);
    m_Data->m_Layer.framebufferOnly = YES;
    UpdateDrawableSize();
}

MetalSwapchain::~MetalSwapchain()
{
    m_ImageView.reset();
    m_Image.reset();

    if (m_Data != nullptr)
    {
        if (m_Data->m_CurrentDrawable != nil)
        {
            [m_Data->m_CurrentDrawable release];
            m_Data->m_CurrentDrawable = nil;
        }

        if (m_Data->m_Layer != nil)
        {
            [m_Data->m_Layer release];
            m_Data->m_Layer = nil;
        }

        delete m_Data;
        m_Data = nullptr;
    }
}

uint32_t MetalSwapchain::AcquireNextImage()
{
    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->m_Layer != nil,
                      "Metal swapchain layer must be valid before acquiring a drawable.");

    SyncDrawableSizeToLayer();

    id<CAMetalDrawable> drawable = [m_Data->m_Layer nextDrawable];
    RTRLAB_ASSERT_MSG(drawable != nil,
                      "Failed to acquire a CAMetalDrawable from the Metal layer. Window minimized or drawable "
                      "acquisition timed out?");

    if (m_Data->m_CurrentDrawable != nil)
        [m_Data->m_CurrentDrawable release];
    m_Data->m_CurrentDrawable = [drawable retain];

    TextureDesc imageDesc;
    imageDesc.m_Type = TextureType::Tex2D;
    imageDesc.m_Format = m_Desc.m_Format;
    imageDesc.m_Extent = Extent3D{m_Desc.m_Width, m_Desc.m_Height, 1};
    imageDesc.m_MipLevels = 1;
    imageDesc.m_ArrayLayers = 1;
    imageDesc.m_UsageMask = TextureUsage::RenderTarget;
    imageDesc.m_DebugName = "MetalSwapchainImage";
    SetMetalDebugLabel(drawable.texture, imageDesc.m_DebugName);

    auto image = CreateScope<MetalSwapchainTexture>(drawable.texture, imageDesc);

    TextureViewDesc viewDesc;
    viewDesc.m_Type = TextureType::Tex2D;
    viewDesc.m_Format = m_Desc.m_Format;
    viewDesc.m_Aspect = TextureAspect::Color;

    m_ImageView = CreateScope<MetalSwapchainImageView>(image.get(), viewDesc);
    m_Image = std::move(image);
    return 0;
}

Texture* MetalSwapchain::GetImage(uint32_t imageIndex) const
{
    RTRLAB_ASSERT_MSG(imageIndex == 0, "Early Metal bring-up only exposes a single acquired drawable at a time.");
    return m_Image.get();
}

TextureView* MetalSwapchain::GetImageView(uint32_t imageIndex) const
{
    RTRLAB_ASSERT_MSG(imageIndex == 0, "Early Metal bring-up only exposes a single acquired drawable at a time.");
    return m_ImageView.get();
}

void MetalSwapchain::Present(uint32_t imageIndex)
{
    RTRLAB_ASSERT_MSG(imageIndex == 0, "Early Metal bring-up only exposes a single acquired drawable at a time.");
    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->m_CurrentDrawable != nil,
                      "Metal presentation requires an acquired drawable.");

    m_Device.PresentDrawable((__bridge void*)m_Data->m_CurrentDrawable);
    m_ImageView.reset();
    m_Image.reset();
    [m_Data->m_CurrentDrawable release];
    m_Data->m_CurrentDrawable = nil;
}

void MetalSwapchain::Resize(uint32_t newWidth, uint32_t newHeight)
{
    if (newWidth == 0 || newHeight == 0)
        return;

    m_Desc.m_Width = newWidth;
    m_Desc.m_Height = newHeight;
    UpdateDrawableSize();
}

void MetalSwapchain::UpdateDrawableSize()
{
    if (m_Data == nullptr || m_Data->m_Layer == nil)
        return;

    m_Data->m_Layer.drawableSize =
        CGSizeMake(static_cast<CGFloat>(m_Desc.m_Width), static_cast<CGFloat>(m_Desc.m_Height));
}

void MetalSwapchain::SyncDrawableSizeToLayer()
{
    if (m_Data == nullptr || m_Data->m_Layer == nil)
        return;

    const CGFloat scale = m_Data->m_Layer.contentsScale > 0.0 ? m_Data->m_Layer.contentsScale : 1.0;
    CGSize drawableSize = m_Data->m_Layer.bounds.size;
    drawableSize.width *= scale;
    drawableSize.height *= scale;

    const uint32_t width = static_cast<uint32_t>(std::max<CGFloat>(drawableSize.width, 1.0));
    const uint32_t height = static_cast<uint32_t>(std::max<CGFloat>(drawableSize.height, 1.0));

    if (width == m_Desc.m_Width && height == m_Desc.m_Height)
        return;

    m_Desc.m_Width = width;
    m_Desc.m_Height = height;
    UpdateDrawableSize();
}
