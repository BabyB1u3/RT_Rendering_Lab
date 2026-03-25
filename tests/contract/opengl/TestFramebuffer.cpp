#include <gtest/gtest.h>
#include <memory>

#include "GLTestContext.h"
#include "graphics/Framebuffer.h"
#include "graphics/GraphicsDevice.h"
#include "graphics/interface/IFramebuffer.h"
#include "graphics/interface/ITexture2D.h"

class FramebufferIntegrationTests : public ::testing::Test
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

TEST_F(FramebufferIntegrationTests, CreateFramebufferWithColorAttachments)
{
    FramebufferSpecification spec{};
    spec.Width = 128;
    spec.Height = 128;
    spec.Attachments = {
        TextureFormat::RGBA8};

    auto framebuffer = GetDevice()->CreateFramebuffer(spec);

    ASSERT_NE(framebuffer, nullptr);
    EXPECT_EQ(framebuffer->GetSpecification().Width, 128u);
    EXPECT_EQ(framebuffer->GetSpecification().Height, 128u);

    EXPECT_NE(framebuffer->GetColorAttachment(), nullptr);
}

TEST_F(FramebufferIntegrationTests, CreateFramebufferWithDepthAttachments)
{
    FramebufferSpecification spec{};
    spec.Width = 128;
    spec.Height = 128;
    spec.Attachments = {
        TextureFormat::Depth};

    auto framebuffer = GetDevice()->CreateFramebuffer(spec);

    ASSERT_NE(framebuffer, nullptr);
    EXPECT_EQ(framebuffer->GetSpecification().Width, 128u);
    EXPECT_EQ(framebuffer->GetSpecification().Height, 128u);

    EXPECT_NE(framebuffer->GetDepthAttachment(), nullptr);
}

TEST_F(FramebufferIntegrationTests, CreateFramebufferWithColorAndDepthAttachments)
{
    FramebufferSpecification spec{};
    spec.Width = 128;
    spec.Height = 128;
    spec.Attachments = {
        TextureFormat::RGBA8,
        TextureFormat::Depth24Stencil8};

    auto framebuffer = GetDevice()->CreateFramebuffer(spec);

    ASSERT_NE(framebuffer, nullptr);
    EXPECT_EQ(framebuffer->GetSpecification().Width, 128u);
    EXPECT_EQ(framebuffer->GetSpecification().Height, 128u);

    EXPECT_NE(framebuffer->GetColorAttachment(), nullptr);
    EXPECT_NE(framebuffer->GetDepthAttachment(), nullptr);
}

TEST_F(FramebufferIntegrationTests, ResizeUpdatesAttachmentSizes)
{
    FramebufferSpecification spec{};
    spec.Width = 64;
    spec.Height = 64;
    spec.Attachments = {
        TextureFormat::RGBA8,
        TextureFormat::Depth24Stencil8};

    auto framebuffer = GetDevice()->CreateFramebuffer(spec);
    framebuffer->Resize(256, 128);

    EXPECT_EQ(framebuffer->GetSpecification().Width, 256u);
    EXPECT_EQ(framebuffer->GetSpecification().Height, 128u);

    auto color = framebuffer->GetColorAttachment();
    ASSERT_NE(color, nullptr);
    EXPECT_EQ(color->GetWidth(), 256u);
    EXPECT_EQ(color->GetHeight(), 128u);
}

TEST_F(FramebufferIntegrationTests, ResizeZeroDoesNotChangeFramebufferState)
{
    FramebufferSpecification spec{};
    spec.Width = 64;
    spec.Height = 32;
    spec.Attachments = {
        TextureFormat::RGBA8,
        TextureFormat::Depth24Stencil8
    };

    auto framebuffer = GetDevice()->CreateFramebuffer(spec);
    const auto beforeSpec = framebuffer->GetSpecification();
    const auto beforeColor = framebuffer->GetColorAttachment();
    const auto beforeDepth = framebuffer->GetDepthAttachment();

    framebuffer->Resize(0, 0);

    const auto afterSpec = framebuffer->GetSpecification();
    EXPECT_EQ(afterSpec.Width, beforeSpec.Width);
    EXPECT_EQ(afterSpec.Height, beforeSpec.Height);
    EXPECT_EQ(framebuffer->GetColorAttachment(), beforeColor);
    EXPECT_EQ(framebuffer->GetDepthAttachment(), beforeDepth);
}

TEST_F(FramebufferIntegrationTests, ResizeTooLargeDoesNotChangeFramebufferState)
{
    FramebufferSpecification spec{};
    spec.Width = 64;
    spec.Height = 32;
    spec.Attachments = {
        TextureFormat::RGBA8,
        TextureFormat::Depth24Stencil8
    };

    auto framebuffer = GetDevice()->CreateFramebuffer(spec);
    const auto beforeSpec = framebuffer->GetSpecification();
    const auto beforeColor = framebuffer->GetColorAttachment();
    const auto beforeDepth = framebuffer->GetDepthAttachment();

    framebuffer->Resize(9000, 9000);

    const auto afterSpec = framebuffer->GetSpecification();
    EXPECT_EQ(afterSpec.Width, beforeSpec.Width);
    EXPECT_EQ(afterSpec.Height, beforeSpec.Height);
    EXPECT_EQ(framebuffer->GetColorAttachment(), beforeColor);
    EXPECT_EQ(framebuffer->GetDepthAttachment(), beforeDepth);
}

TEST_F(FramebufferIntegrationTests, ResizeUpdatesDepthAttachmentSize)
{
    FramebufferSpecification spec{};
    spec.Width = 32;
    spec.Height = 64;
    spec.Attachments = {
        TextureFormat::RGBA8,
        TextureFormat::Depth24Stencil8
    };

    auto framebuffer = GetDevice()->CreateFramebuffer(spec);
    framebuffer->Resize(80, 40);

    auto depth = framebuffer->GetDepthAttachment();
    ASSERT_NE(depth, nullptr);
    EXPECT_EQ(depth->GetWidth(), 80u);
    EXPECT_EQ(depth->GetHeight(), 40u);
}

TEST_F(FramebufferIntegrationTests, BindAndUnbindDoNotCrash)
{
    FramebufferSpecification spec{};
    spec.Width = 32;
    spec.Height = 32;
    spec.Attachments = {
        TextureFormat::RGBA8,
        TextureFormat::Depth24Stencil8};

    auto framebuffer = GetDevice()->CreateFramebuffer(spec);

    EXPECT_NO_THROW(framebuffer->Bind());
    EXPECT_NO_THROW(framebuffer->Unbind());
}

TEST_F(FramebufferIntegrationTests, OutOfRangeColorAttachmentReturnsNull)
{
    FramebufferSpecification spec{};
    spec.Width = 32;
    spec.Height = 32;
    spec.Attachments = {
        TextureFormat::RGBA8,
        TextureFormat::Depth24Stencil8
    };

    auto framebuffer = GetDevice()->CreateFramebuffer(spec);

    EXPECT_EQ(framebuffer->GetColorAttachment(1), nullptr);
    EXPECT_EQ(framebuffer->GetColorAttachment(42), nullptr);
}

TEST_F(FramebufferIntegrationTests, RedIntegerAttachmentCanBeClearedAndReadBack)
{
    FramebufferSpecification spec{};
    spec.Width = 8;
    spec.Height = 8;
    spec.Attachments = {
        TextureFormat::RedInteger,
        TextureFormat::Depth24Stencil8};

    auto framebuffer = GetDevice()->CreateFramebuffer(spec);

    ASSERT_NE(framebuffer, nullptr);

    framebuffer->ClearAttachment(0, 1337);

    EXPECT_EQ(framebuffer->ReadPixel(0, 4, 4), 1337);
    EXPECT_EQ(framebuffer->ReadPixel(0, 0, 0), 1337);
}
