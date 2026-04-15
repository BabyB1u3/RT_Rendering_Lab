#include "Render/RHI/Backends/Metal/MetalDevice.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include "Core/Diagnostics/Assert/Assert.h"

namespace
{
MTLPixelFormat toMetalPixelFormat(Format format)
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

MTLLoadAction toMetalLoadAction(LoadOp loadOp)
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

MTLStoreAction toMetalStoreAction(StoreOp storeOp)
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

    const TextureDesc& getDesc() const override { return m_Desc; }
    id<MTLTexture> getMetalTexture() const { return m_Texture; }

private:
    id<MTLTexture> m_Texture = nil;
    TextureDesc m_Desc;
};

class MetalSwapchainImageView final : public TextureView
{
public:
    MetalSwapchainImageView(Texture* texture, const TextureViewDesc& desc) : m_Texture(texture), m_Desc(desc) {}

    Texture* getTexture() const override { return m_Texture; }
    const TextureViewDesc& getDesc() const override { return m_Desc; }

private:
    Texture* m_Texture = nullptr;
    TextureViewDesc m_Desc;
};

struct MetalDeviceData
{
    id<MTLDevice> device = nil;
    id<MTLCommandQueue> commandQueue = nil;
    id<MTLCommandBuffer> currentCommandBuffer = nil;
    NSAutoreleasePool* frameAutoreleasePool = nil;
    bool frameInProgress = false;
    bool frameSubmitted = false;
};

struct MetalCommandListData
{
    MetalDeviceData* deviceData = nullptr;
    id<MTLRenderCommandEncoder> renderEncoder = nil;
};

struct MetalSwapchainData
{
    CAMetalLayer* layer = nil;
    id<CAMetalDrawable> currentDrawable = nil;
};

MetalCommandList::MetalCommandList() : m_Data(new MetalCommandListData()) {}

MetalCommandList::~MetalCommandList()
{
    shutdown();
    delete m_Data;
    m_Data = nullptr;
}

void MetalCommandList::initialize(MetalDeviceData* deviceData)
{
    shutdown();
    m_Data->deviceData = deviceData;
}

void MetalCommandList::shutdown()
{
    if (m_Data == nullptr)
        return;

    if (m_Data->renderEncoder != nil)
    {
        [m_Data->renderEncoder endEncoding];
        [m_Data->renderEncoder release];
        m_Data->renderEncoder = nil;
    }

    m_Data->deviceData = nullptr;
}

void MetalCommandList::beginRendering(const RenderingInfo& renderingInfo)
{
    ShellCommandListBase::beginRendering(renderingInfo);

    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->deviceData != nullptr,
                      "Metal command list must be initialized before rendering.");
    RTRLAB_ASSERT_MSG(m_Data->deviceData->currentCommandBuffer != nil,
                      "Metal rendering requires an active command buffer.");
    RTRLAB_ASSERT_MSG(renderingInfo.colorAttachments.size() == 1,
                      "Early Metal bring-up currently supports exactly one color attachment.");
    RTRLAB_ASSERT_MSG(renderingInfo.depthAttachment.view == nullptr,
                      "Early Metal bring-up does not support depth attachments yet.");

    const ColorAttachmentInfo& colorAttachment = renderingInfo.colorAttachments.front();
    auto* imageView = dynamic_cast<MetalSwapchainImageView*>(colorAttachment.view);
    RTRLAB_ASSERT_MSG(imageView != nullptr, "Metal beginRendering currently expects a swapchain image view.");

    auto* texture = dynamic_cast<MetalSwapchainTexture*>(imageView->getTexture());
    RTRLAB_ASSERT_MSG(texture != nullptr, "Metal beginRendering currently expects a swapchain texture.");

    MTLRenderPassDescriptor* renderPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
    renderPassDescriptor.colorAttachments[0].texture = texture->getMetalTexture();
    renderPassDescriptor.colorAttachments[0].loadAction = toMetalLoadAction(colorAttachment.loadOp);
    renderPassDescriptor.colorAttachments[0].storeAction = toMetalStoreAction(colorAttachment.storeOp);
    renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(colorAttachment.clearValue.r,
                                                                            colorAttachment.clearValue.g,
                                                                            colorAttachment.clearValue.b,
                                                                            colorAttachment.clearValue.a);

    m_Data->renderEncoder =
        [[m_Data->deviceData->currentCommandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor] retain];
    RTRLAB_ASSERT_MSG(m_Data->renderEncoder != nil, "Failed to create the Metal render command encoder.");
}

void MetalCommandList::endRendering()
{
    RTRLAB_ASSERT_MSG(m_IsRendering, "Metal endRendering requires an active rendering scope.");
    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->renderEncoder != nil,
                      "Metal endRendering requires an active render encoder.");

    [m_Data->renderEncoder endEncoding];
    [m_Data->renderEncoder release];
    m_Data->renderEncoder = nil;

    ShellCommandListBase::endRendering();
}

MetalSwapchain::MetalSwapchain(MetalDevice& device,
                               const SwapchainDesc& desc,
                               const NativeWindowHandle& nativeWindowHandle)
    : m_Device(device),
      m_Desc(RHIInternal::sanitizeSwapchainDesc(desc)),
      m_NativeWindowHandle(nativeWindowHandle),
      m_Data(new MetalSwapchainData())
{
    RTRLAB_ASSERT_MSG(RHIInternal::isNativeWindowHandleValid(nativeWindowHandle),
                      "Native window handle is incomplete.");
    RTRLAB_ASSERT_MSG(nativeWindowHandle.layer != nullptr, "Metal swapchain creation requires a valid CAMetalLayer.");

    m_Data->layer = [(__bridge CAMetalLayer*)nativeWindowHandle.layer retain];
    // Window/platform code only supplies the presentation surface. Metal-specific layer
    // configuration is owned here by the backend swapchain.
    m_Data->layer.device = m_Device.getData()->device;
    m_Data->layer.pixelFormat = toMetalPixelFormat(m_Desc.format);
    m_Data->layer.framebufferOnly = YES;
    updateDrawableSize();
}

MetalSwapchain::~MetalSwapchain()
{
    m_ImageView.reset();
    m_Image.reset();

    if (m_Data != nullptr)
    {
        if (m_Data->currentDrawable != nil)
        {
            [m_Data->currentDrawable release];
            m_Data->currentDrawable = nil;
        }

        if (m_Data->layer != nil)
        {
            [m_Data->layer release];
            m_Data->layer = nil;
        }

        delete m_Data;
        m_Data = nullptr;
    }
}

uint32_t MetalSwapchain::acquireNextImage()
{
    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->layer != nil,
                      "Metal swapchain layer must be valid before acquiring a drawable.");

    id<CAMetalDrawable> drawable = [m_Data->layer nextDrawable];
    RTRLAB_ASSERT_MSG(drawable != nil,
                      "Failed to acquire a CAMetalDrawable from the Metal layer. Window minimized or drawable "
                      "acquisition timed out?");

    if (m_Data->currentDrawable != nil)
        [m_Data->currentDrawable release];
    m_Data->currentDrawable = [drawable retain];

    TextureDesc imageDesc;
    imageDesc.type = TextureType::Tex2D;
    imageDesc.format = m_Desc.format;
    imageDesc.extent = Extent3D{m_Desc.width, m_Desc.height, 1};
    imageDesc.mipLevels = 1;
    imageDesc.arrayLayers = 1;
    imageDesc.usageMask = TextureUsage::RenderTarget;
    imageDesc.debugName = "MetalSwapchainImage";

    auto image = CreateScope<MetalSwapchainTexture>(drawable.texture, imageDesc);

    TextureViewDesc viewDesc;
    viewDesc.type = TextureType::Tex2D;
    viewDesc.format = m_Desc.format;
    viewDesc.aspect = TextureAspect::Color;

    m_ImageView = CreateScope<MetalSwapchainImageView>(image.get(), viewDesc);
    m_Image = std::move(image);
    return 0;
}

Texture* MetalSwapchain::getImage(uint32_t imageIndex) const
{
    RTRLAB_ASSERT_MSG(imageIndex == 0, "Early Metal bring-up only exposes a single acquired drawable at a time.");
    return m_Image.get();
}

TextureView* MetalSwapchain::getImageView(uint32_t imageIndex) const
{
    RTRLAB_ASSERT_MSG(imageIndex == 0, "Early Metal bring-up only exposes a single acquired drawable at a time.");
    return m_ImageView.get();
}

void MetalSwapchain::present(uint32_t imageIndex)
{
    RTRLAB_ASSERT_MSG(imageIndex == 0, "Early Metal bring-up only exposes a single acquired drawable at a time.");
    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->currentDrawable != nil,
                      "Metal presentation requires an acquired drawable.");

    m_Device.presentDrawable((__bridge void*)m_Data->currentDrawable);
    m_ImageView.reset();
    m_Image.reset();
    [m_Data->currentDrawable release];
    m_Data->currentDrawable = nil;
}

void MetalSwapchain::resize(uint32_t newWidth, uint32_t newHeight)
{
    if (newWidth == 0 || newHeight == 0)
        return;

    m_Desc.width = newWidth;
    m_Desc.height = newHeight;
    updateDrawableSize();
}

void MetalSwapchain::updateDrawableSize()
{
    if (m_Data == nullptr || m_Data->layer == nil)
        return;

    m_Data->layer.drawableSize = CGSizeMake(static_cast<CGFloat>(m_Desc.width), static_cast<CGFloat>(m_Desc.height));
}

MetalDevice::MetalDevice() : m_Data(new MetalDeviceData())
{
    m_Data->device = MTLCreateSystemDefaultDevice();
    RTRLAB_ASSERT_MSG(m_Data->device != nil, "Failed to create the system Metal device.");

    m_Data->commandQueue = [m_Data->device newCommandQueue];
    RTRLAB_ASSERT_MSG(m_Data->commandQueue != nil, "Failed to create the Metal command queue.");

    m_CommandList.initialize(m_Data);
}

MetalDevice::~MetalDevice()
{
    m_CommandList.shutdown();

    if (m_Data != nullptr)
    {
        if (m_Data->frameAutoreleasePool != nil)
        {
            [m_Data->frameAutoreleasePool release];
            m_Data->frameAutoreleasePool = nil;
        }

        if (m_Data->currentCommandBuffer != nil)
        {
            [m_Data->currentCommandBuffer release];
            m_Data->currentCommandBuffer = nil;
        }

        if (m_Data->commandQueue != nil)
        {
            [m_Data->commandQueue release];
            m_Data->commandQueue = nil;
        }

        if (m_Data->device != nil)
        {
            [m_Data->device release];
            m_Data->device = nil;
        }
    }

    delete m_Data;
    m_Data = nullptr;
}

Scope<Swapchain> MetalDevice::createSwapchain(const SwapchainDesc& desc, const NativeWindowHandle& nativeWindowHandle)
{
    return CreateScope<MetalSwapchain>(*this, desc, nativeWindowHandle);
}

CommandList* MetalDevice::beginCommandList()
{
    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->frameInProgress,
                      "Metal command recording requires an active frame.");
    RTRLAB_ASSERT_MSG(!m_Data->frameSubmitted, "Metal command recording must happen before submission.");
    return &m_CommandList;
}

void MetalDevice::submit(CommandList* commandList)
{
    RTRLAB_ASSERT_MSG(commandList == &m_CommandList,
                      "MetalDevice only accepts submissions from its backend command list.");
    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->frameInProgress, "Metal submission requires an active frame.");
    RTRLAB_ASSERT_MSG(!m_Data->frameSubmitted,
                      "Metal submit must only happen once per frame in the current bring-up path.");
    RTRLAB_ASSERT_MSG(!m_CommandList.isRenderingActive(), "Metal submit requires endRendering before submission.");

    // Metal keeps the public submit/present split by deferring commit until Swapchain::present().
    m_Data->frameSubmitted = true;
}

FrameContext* MetalDevice::beginFrame()
{
    RTRLAB_ASSERT_MSG(m_Data != nullptr, "Metal device data must be initialized before beginning a frame.");
    RTRLAB_ASSERT_MSG(!m_Data->frameInProgress, "MetalDevice does not support nested frame lifetimes.");
    RTRLAB_ASSERT_MSG(m_Data->frameAutoreleasePool == nil,
                      "Metal frame autorelease pool must be drained before the next frame begins.");

    m_Data->frameAutoreleasePool = [[NSAutoreleasePool alloc] init];

    m_Data->currentCommandBuffer = [[m_Data->commandQueue commandBuffer] retain];
    RTRLAB_ASSERT_MSG(m_Data->currentCommandBuffer != nil, "Failed to allocate the Metal command buffer.");

    m_Data->frameInProgress = true;
    m_Data->frameSubmitted = false;
    return &m_FrameContext;
}

void MetalDevice::endFrame(FrameContext* frameContext)
{
    RTRLAB_ASSERT_MSG(frameContext == &m_FrameContext, "MetalDevice only accepts its backend frame context.");
    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->frameInProgress, "Metal endFrame requires an active frame.");
    RTRLAB_ASSERT_MSG(m_Data->frameSubmitted, "Metal endFrame currently expects submission before the frame closes.");

    m_Data->frameInProgress = false;
}

void MetalDevice::presentDrawable(void* drawable)
{
    RTRLAB_ASSERT_MSG(m_Data != nullptr, "Metal device data must remain valid until presentation.");
    RTRLAB_ASSERT_MSG(!m_Data->frameInProgress, "Metal drawable presentation requires endFrame first.");
    RTRLAB_ASSERT_MSG(m_Data->frameSubmitted, "Metal drawable presentation requires a submitted frame.");
    RTRLAB_ASSERT_MSG(m_Data->currentCommandBuffer != nil,
                      "Metal drawable presentation requires an active command buffer.");

    id<CAMetalDrawable> metalDrawable = (__bridge id<CAMetalDrawable>)drawable;
    RTRLAB_ASSERT_MSG(metalDrawable != nil, "Metal drawable presentation requires a valid CAMetalDrawable.");

    [m_Data->currentCommandBuffer presentDrawable:metalDrawable];
    [m_Data->currentCommandBuffer commit];
    [m_Data->currentCommandBuffer release];
    m_Data->currentCommandBuffer = nil;

    if (m_Data->frameAutoreleasePool != nil)
    {
        [m_Data->frameAutoreleasePool release];
        m_Data->frameAutoreleasePool = nil;
    }

    m_Data->frameSubmitted = false;
}
