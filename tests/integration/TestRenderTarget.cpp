#include <gtest/gtest.h>
#include <memory>

#include "GlTestContext.h"
#include "graphics/Framebuffer.h"
#include "graphics/RenderTarget.h"
#include "graphics/Texture.h"

class RenderTargetIntegrationTests : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        s_Context = CreateScope<GlTestContext>();
    }

    static void TearDownTestSuite()
    {
        s_Context.reset();
    }

    inline static Scope<GlTestContext> s_Context;
};

// --- BackBuffer tests ---

TEST_F(RenderTargetIntegrationTests, BackBufferCreation)
{
    RenderTarget target = RenderTarget::BackBuffer(800, 600);

    EXPECT_TRUE(target.IsBackBuffer());
    EXPECT_FALSE(target.IsFramebuffer());
    EXPECT_EQ(target.GetWidth(), 800u);
    EXPECT_EQ(target.GetHeight(), 600u);
}

TEST_F(RenderTargetIntegrationTests, BackBufferReturnsNullAttachments)
{
    RenderTarget target = RenderTarget::BackBuffer(800, 600);

    EXPECT_EQ(target.GetColorAttachment(), nullptr);
    EXPECT_EQ(target.GetDepthAttachment(), nullptr);
}

TEST_F(RenderTargetIntegrationTests, BackBufferResize)
{
    RenderTarget target = RenderTarget::BackBuffer(800, 600);
    target.Resize(1920, 1080);

    EXPECT_EQ(target.GetWidth(), 1920u);
    EXPECT_EQ(target.GetHeight(), 1080u);
}

TEST_F(RenderTargetIntegrationTests, BackBufferBindUnbindDoNotCrash)
{
    RenderTarget target = RenderTarget::BackBuffer(800, 600);

    EXPECT_NO_THROW(target.Bind());
    EXPECT_NO_THROW(target.Unbind());
}

TEST_F(RenderTargetIntegrationTests, BackBufferFramebufferIsNull)
{
    RenderTarget target = RenderTarget::BackBuffer(800, 600);

    EXPECT_EQ(target.GetFramebuffer(), nullptr);
}

// --- Framebuffer target tests ---

TEST_F(RenderTargetIntegrationTests, FromFramebufferCreation)
{
    FramebufferSpecification spec{};
    spec.Width = 256;
    spec.Height = 256;
    spec.Attachments = {
        TextureFormat::RGBA8,
        TextureFormat::Depth24Stencil8
    };

    auto fb = CreateRef<Framebuffer>(spec);
    RenderTarget target = RenderTarget::FromFramebuffer(fb);

    EXPECT_FALSE(target.IsBackBuffer());
    EXPECT_TRUE(target.IsFramebuffer());
    EXPECT_EQ(target.GetWidth(), 256u);
    EXPECT_EQ(target.GetHeight(), 256u);
}

TEST_F(RenderTargetIntegrationTests, FromFramebufferDelegatesAttachments)
{
    FramebufferSpecification spec{};
    spec.Width = 128;
    spec.Height = 128;
    spec.Attachments = {
        TextureFormat::RGBA8,
        TextureFormat::Depth24Stencil8
    };

    auto fb = CreateRef<Framebuffer>(spec);
    RenderTarget target = RenderTarget::FromFramebuffer(fb);

    EXPECT_NE(target.GetColorAttachment(), nullptr);
    EXPECT_NE(target.GetDepthAttachment(), nullptr);
    EXPECT_EQ(target.GetColorAttachment(), fb->GetColorAttachment());
    EXPECT_EQ(target.GetDepthAttachment(), fb->GetDepthAttachment());
}

TEST_F(RenderTargetIntegrationTests, FromFramebufferDepthOnly)
{
    FramebufferSpecification spec{};
    spec.Width = 512;
    spec.Height = 512;
    spec.Attachments = {
        TextureFormat::Depth
    };

    auto fb = CreateRef<Framebuffer>(spec);
    RenderTarget target = RenderTarget::FromFramebuffer(fb);

    EXPECT_EQ(target.GetColorAttachment(), nullptr);
    EXPECT_NE(target.GetDepthAttachment(), nullptr);
}

TEST_F(RenderTargetIntegrationTests, FromFramebufferResizePropagates)
{
    FramebufferSpecification spec{};
    spec.Width = 64;
    spec.Height = 64;
    spec.Attachments = {
        TextureFormat::RGBA8,
        TextureFormat::Depth24Stencil8
    };

    auto fb = CreateRef<Framebuffer>(spec);
    RenderTarget target = RenderTarget::FromFramebuffer(fb);

    target.Resize(512, 256);

    EXPECT_EQ(target.GetWidth(), 512u);
    EXPECT_EQ(target.GetHeight(), 256u);
    EXPECT_EQ(fb->GetSpecification().Width, 512u);
    EXPECT_EQ(fb->GetSpecification().Height, 256u);
}

TEST_F(RenderTargetIntegrationTests, FromFramebufferBindUnbindDoNotCrash)
{
    FramebufferSpecification spec{};
    spec.Width = 32;
    spec.Height = 32;
    spec.Attachments = {
        TextureFormat::RGBA8,
        TextureFormat::Depth24Stencil8
    };

    auto fb = CreateRef<Framebuffer>(spec);
    RenderTarget target = RenderTarget::FromFramebuffer(fb);

    EXPECT_NO_THROW(target.Bind());
    EXPECT_NO_THROW(target.Unbind());
}

TEST_F(RenderTargetIntegrationTests, FromFramebufferHoldsReference)
{
    FramebufferSpecification spec{};
    spec.Width = 64;
    spec.Height = 64;
    spec.Attachments = { TextureFormat::RGBA8 };

    auto fb = CreateRef<Framebuffer>(spec);
    RenderTarget target = RenderTarget::FromFramebuffer(fb);

    EXPECT_EQ(target.GetFramebuffer(), fb);
}
