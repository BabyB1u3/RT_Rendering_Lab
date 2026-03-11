#include <gtest/gtest.h>
#include <memory>

#include "GlTestContext.h"
#include "graphics/Framebuffer.h"

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
        TextureFormat::RGBA8
    };

    Framebuffer framebuffer(spec);

    EXPECT_NE(framebuffer.GetRendererID(), 0u);
    EXPECT_EQ(framebuffer.GetSpecification().Width, 128u);
    EXPECT_EQ(framebuffer.GetSpecification().Height, 128u);

    EXPECT_NE(framebuffer.GetColorAttachment(), nullptr);
}

TEST_F(FramebufferIntegrationTests, CreateFramebufferWithDepthAttachments)
{
    FramebufferSpecification spec{};
    spec.Width = 128;
    spec.Height = 128;
    spec.Attachments = {
        TextureFormat::Depth
    };

    Framebuffer framebuffer(spec);

    EXPECT_NE(framebuffer.GetRendererID(), 0u);
    EXPECT_EQ(framebuffer.GetSpecification().Width, 128u);
    EXPECT_EQ(framebuffer.GetSpecification().Height, 128u);

    EXPECT_NE(framebuffer.GetDepthAttachment(), nullptr);
}

TEST_F(FramebufferIntegrationTests, CreateFramebufferWithColorAndDepthAttachments)
{
    FramebufferSpecification spec{};
    spec.Width = 128;
    spec.Height = 128;
    spec.Attachments = {
        TextureFormat::RGBA8,
        TextureFormat::Depth24Stencil8
    };

    Framebuffer framebuffer(spec);

    EXPECT_NE(framebuffer.GetRendererID(), 0u);
    EXPECT_EQ(framebuffer.GetSpecification().Width, 128u);
    EXPECT_EQ(framebuffer.GetSpecification().Height, 128u);

    EXPECT_NE(framebuffer.GetColorAttachment(), nullptr);
    EXPECT_NE(framebuffer.GetDepthAttachment(), nullptr);
}

TEST_F(FramebufferIntegrationTests, ResizeUpdatesAttachmentSizes)
{
    FramebufferSpecification spec{};
    spec.Width = 64;
    spec.Height = 64;
    spec.Attachments = {
        TextureFormat::RGBA8,
        TextureFormat::Depth24Stencil8
    };

    Framebuffer framebuffer(spec);
    framebuffer.Resize(256, 128);

    EXPECT_EQ(framebuffer.GetSpecification().Width, 256u);
    EXPECT_EQ(framebuffer.GetSpecification().Height, 128u);

    auto color = framebuffer.GetColorAttachment();
    ASSERT_NE(color, nullptr);
    EXPECT_EQ(color->GetWidth(), 256u);
    EXPECT_EQ(color->GetHeight(), 128u);
}

TEST_F(FramebufferIntegrationTests, BindAndUnbindDoNotCrash)
{
    FramebufferSpecification spec{};
    spec.Width = 32;
    spec.Height = 32;
    spec.Attachments = {
        TextureFormat::RGBA8,
        TextureFormat::Depth24Stencil8
    };

    Framebuffer framebuffer(spec);

    EXPECT_NO_THROW(framebuffer.Bind());
    EXPECT_NO_THROW(framebuffer.Unbind());
}