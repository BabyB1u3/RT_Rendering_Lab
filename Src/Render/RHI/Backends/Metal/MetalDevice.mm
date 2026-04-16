#include "Render/RHI/Backends/Metal/MetalDevice.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include "Core/Diagnostics/Assert/Assert.h"

namespace
{
MTLPixelFormat ToMetalPixelFormat(Format format)
{
    switch (format)
    {
        case Format::BGRA8_UNORM:
            return MTLPixelFormatBGRA8Unorm;
        case Format::BGRA8_SRGB:
            return MTLPixelFormatBGRA8Unorm_sRGB;
        case Format::RGBA8_UNORM:
            return MTLPixelFormatRGBA8Unorm;
        case Format::RGBA8_SRGB:
            return MTLPixelFormatRGBA8Unorm_sRGB;
        default:
            return MTLPixelFormatBGRA8Unorm;
    }
}

MTLLoadAction ToMetalLoadAction(LoadOp loadOp)
{
    switch (loadOp)
    {
        case LoadOp::Clear:
            return MTLLoadActionClear;
        case LoadOp::DontCare:
            return MTLLoadActionDontCare;
        case LoadOp::Load:
        default:
            return MTLLoadActionLoad;
    }
}

MTLStoreAction ToMetalStoreAction(StoreOp storeOp)
{
    switch (storeOp)
    {
        case StoreOp::DontCare:
            return MTLStoreActionDontCare;
        case StoreOp::Store:
        default:
            return MTLStoreActionStore;
    }
}
} // namespace

class MetalSwapchainTexture final : public Texture
{
public:
    MetalSwapchainTexture(id<MTLTexture> texture, const TextureDesc& desc) : m_Texture(texture), m_Desc(desc) {}

    const TextureDesc& GetDesc() const override { return m_Desc; }
    id<MTLTexture> GetMetalTexture() const { return m_Texture; }

private:
    id<MTLTexture> m_Texture = nil;
    TextureDesc m_Desc;
};

class MetalSwapchainImageView final : public TextureView
{
public:
    MetalSwapchainImageView(Texture* texture, const TextureViewDesc& desc) : m_Texture(texture), m_Desc(desc) {}

    Texture* GetTexture() const override { return m_Texture; }
    const TextureViewDesc& GetDesc() const override { return m_Desc; }

private:
    Texture* m_Texture = nullptr;
    TextureViewDesc m_Desc;
};

struct MetalDeviceData
{
    id<MTLDevice> m_Device = nil;
    id<MTLCommandQueue> m_CommandQueue = nil;
    id<MTLCommandBuffer> m_CurrentCommandBuffer = nil;
    NSAutoreleasePool* m_FrameAutoreleasePool = nil;
    bool m_FrameInProgress = false;
    bool m_FrameSubmitted = false;
};

struct MetalCommandListData
{
    MetalDeviceData* m_DeviceData = nullptr;
    id<MTLRenderCommandEncoder> m_RenderEncoder = nil;
};

struct MetalSwapchainData
{
    CAMetalLayer* m_Layer = nil;
    id<CAMetalDrawable> m_CurrentDrawable = nil;
};

MetalCommandList::MetalCommandList() : m_Data(new MetalCommandListData()) {}

MetalCommandList::~MetalCommandList()
{
    Shutdown();
    delete m_Data;
    m_Data = nullptr;
}

void MetalCommandList::Initialize(MetalDeviceData* deviceData)
{
    Shutdown();
    m_Data->m_DeviceData = deviceData;
}

void MetalCommandList::Shutdown()
{
    if (m_Data == nullptr)
        return;

    if (m_Data->m_RenderEncoder != nil)
    {
        [m_Data->m_RenderEncoder endEncoding];
        [m_Data->m_RenderEncoder release];
        m_Data->m_RenderEncoder = nil;
    }

    m_Data->m_DeviceData = nullptr;
}

void MetalCommandList::BeginRendering(const RenderingInfo& renderingInfo)
{
    ShellCommandListBase::BeginRendering(renderingInfo);

    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->m_DeviceData != nullptr,
                      "Metal command list must be initialized before rendering.");
    RTRLAB_ASSERT_MSG(m_Data->m_DeviceData->m_CurrentCommandBuffer != nil,
                      "Metal rendering requires an active command buffer.");
    RTRLAB_ASSERT_MSG(renderingInfo.m_ColorAttachments.size() == 1,
                      "Early Metal bring-up currently supports exactly one color attachment.");
    RTRLAB_ASSERT_MSG(renderingInfo.m_DepthAttachment.m_View == nullptr,
                      "Early Metal bring-up does not support depth attachments yet.");

    const ColorAttachmentInfo& colorAttachment = renderingInfo.m_ColorAttachments.front();
    auto* imageView = dynamic_cast<MetalSwapchainImageView*>(colorAttachment.m_View);
    RTRLAB_ASSERT_MSG(imageView != nullptr, "Metal BeginRendering currently expects a swapchain image view.");

    auto* texture = dynamic_cast<MetalSwapchainTexture*>(imageView->GetTexture());
    RTRLAB_ASSERT_MSG(texture != nullptr, "Metal BeginRendering currently expects a swapchain texture.");

    MTLRenderPassDescriptor* renderPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
    renderPassDescriptor.colorAttachments[0].texture = texture->GetMetalTexture();
    renderPassDescriptor.colorAttachments[0].loadAction = ToMetalLoadAction(colorAttachment.m_LoadOp);
    renderPassDescriptor.colorAttachments[0].storeAction = ToMetalStoreAction(colorAttachment.m_StoreOp);
    renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(colorAttachment.m_ClearValue.m_R,
                                                                            colorAttachment.m_ClearValue.m_G,
                                                                            colorAttachment.m_ClearValue.m_B,
                                                                            colorAttachment.m_ClearValue.m_A);

    m_Data->m_RenderEncoder =
        [[m_Data->m_DeviceData->m_CurrentCommandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor] retain];
    RTRLAB_ASSERT_MSG(m_Data->m_RenderEncoder != nil, "Failed to create the Metal render command encoder.");
}

void MetalCommandList::EndRendering()
{
    RTRLAB_ASSERT_MSG(m_IsRendering, "Metal EndRendering requires an active rendering scope.");
    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->m_RenderEncoder != nil,
                      "Metal EndRendering requires an active render encoder.");

    [m_Data->m_RenderEncoder endEncoding];
    [m_Data->m_RenderEncoder release];
    m_Data->m_RenderEncoder = nil;

    ShellCommandListBase::EndRendering();
}

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
    // Window/platform code only supplies the presentation surface. Metal-specific layer
    // configuration is owned here by the backend swapchain.
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

MetalDevice::MetalDevice() : m_Data(new MetalDeviceData())
{
    m_Data->m_Device = MTLCreateSystemDefaultDevice();
    RTRLAB_ASSERT_MSG(m_Data->m_Device != nil, "Failed to create the system Metal device.");

    m_Data->m_CommandQueue = [m_Data->m_Device newCommandQueue];
    RTRLAB_ASSERT_MSG(m_Data->m_CommandQueue != nil, "Failed to create the Metal command queue.");

    m_CommandList.Initialize(m_Data);
}

MetalDevice::~MetalDevice()
{
    m_CommandList.Shutdown();

    if (m_Data != nullptr)
    {
        if (m_Data->m_FrameAutoreleasePool != nil)
        {
            [m_Data->m_FrameAutoreleasePool release];
            m_Data->m_FrameAutoreleasePool = nil;
        }

        if (m_Data->m_CurrentCommandBuffer != nil)
        {
            [m_Data->m_CurrentCommandBuffer release];
            m_Data->m_CurrentCommandBuffer = nil;
        }

        if (m_Data->m_CommandQueue != nil)
        {
            [m_Data->m_CommandQueue release];
            m_Data->m_CommandQueue = nil;
        }

        if (m_Data->m_Device != nil)
        {
            [m_Data->m_Device release];
            m_Data->m_Device = nil;
        }
    }

    delete m_Data;
    m_Data = nullptr;
}

Scope<Swapchain> MetalDevice::CreateSwapchain(const SwapchainDesc& desc, const NativeWindowHandle& nativeWindowHandle)
{
    return CreateScope<MetalSwapchain>(*this, desc, nativeWindowHandle);
}

CommandList* MetalDevice::BeginCommandList()
{
    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->m_FrameInProgress,
                      "Metal command recording requires an active frame.");
    RTRLAB_ASSERT_MSG(!m_Data->m_FrameSubmitted, "Metal command recording must happen before submission.");
    return &m_CommandList;
}

void MetalDevice::Submit(CommandList* commandList)
{
    RTRLAB_ASSERT_MSG(commandList == &m_CommandList,
                      "MetalDevice only accepts submissions from its backend command list.");
    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->m_FrameInProgress, "Metal submission requires an active frame.");
    RTRLAB_ASSERT_MSG(!m_Data->m_FrameSubmitted,
                      "Metal Submit must only happen once per frame in the current bring-up path.");
    RTRLAB_ASSERT_MSG(!m_CommandList.IsRenderingActive(), "Metal Submit requires EndRendering before submission.");

    // Metal keeps the public Submit/present split by deferring commit until Swapchain::present().
    m_Data->m_FrameSubmitted = true;
}

FrameContext* MetalDevice::BeginFrame()
{
    RTRLAB_ASSERT_MSG(m_Data != nullptr, "Metal device data must be initialized before beginning a frame.");
    RTRLAB_ASSERT_MSG(!m_Data->m_FrameInProgress, "MetalDevice does not support nested frame lifetimes.");
    RTRLAB_ASSERT_MSG(m_Data->m_FrameAutoreleasePool == nil,
                      "Metal frame autorelease pool must be drained before the next frame begins.");

    m_Data->m_FrameAutoreleasePool = [[NSAutoreleasePool alloc] init];

    m_Data->m_CurrentCommandBuffer = [[m_Data->m_CommandQueue commandBuffer] retain];
    RTRLAB_ASSERT_MSG(m_Data->m_CurrentCommandBuffer != nil, "Failed to allocate the Metal command buffer.");

    m_Data->m_FrameInProgress = true;
    m_Data->m_FrameSubmitted = false;
    return &m_FrameContext;
}

void MetalDevice::EndFrame(FrameContext* frameContext)
{
    RTRLAB_ASSERT_MSG(frameContext == &m_FrameContext, "MetalDevice only accepts its backend frame context.");
    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->m_FrameInProgress, "Metal EndFrame requires an active frame.");
    RTRLAB_ASSERT_MSG(m_Data->m_FrameSubmitted, "Metal EndFrame currently expects submission before the frame closes.");

    m_Data->m_FrameInProgress = false;
}

void MetalDevice::PresentDrawable(void* drawable)
{
    RTRLAB_ASSERT_MSG(m_Data != nullptr, "Metal device data must remain valid until presentation.");
    RTRLAB_ASSERT_MSG(!m_Data->m_FrameInProgress, "Metal drawable presentation requires EndFrame first.");
    RTRLAB_ASSERT_MSG(m_Data->m_FrameSubmitted, "Metal drawable presentation requires a submitted frame.");
    RTRLAB_ASSERT_MSG(m_Data->m_CurrentCommandBuffer != nil,
                      "Metal drawable presentation requires an active command buffer.");

    id<CAMetalDrawable> metalDrawable = (__bridge id<CAMetalDrawable>)drawable;
    RTRLAB_ASSERT_MSG(metalDrawable != nil, "Metal drawable presentation requires a valid CAMetalDrawable.");

    [m_Data->m_CurrentCommandBuffer presentDrawable:metalDrawable];
    [m_Data->m_CurrentCommandBuffer commit];
    [m_Data->m_CurrentCommandBuffer release];
    m_Data->m_CurrentCommandBuffer = nil;

    if (m_Data->m_FrameAutoreleasePool != nil)
    {
        [m_Data->m_FrameAutoreleasePool release];
        m_Data->m_FrameAutoreleasePool = nil;
    }

    m_Data->m_FrameSubmitted = false;
}
