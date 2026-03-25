#include <gtest/gtest.h>
#include <memory>

#include "GlTestContext.h"
#include "graphics/Framebuffer.h"
#include "graphics/GraphicsDevice.h"
#include "graphics/RenderCommand.h"
#include "graphics/RenderTypes.h"
#include "graphics/interface/IFramebuffer.h"
#include "graphics/interface/IRenderTarget.h"
#include "graphics/interface/ITexture2D.h"
#include "graphics/opengl/GLRenderTarget.h"

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
    auto target = GetDevice()->CreateRenderTargetBackBuffer(800, 600);

    ASSERT_NE(target, nullptr);
    EXPECT_EQ(target->GetWidth(), 800u);
    EXPECT_EQ(target->GetHeight(), 600u);
}

TEST_F(RenderTargetIntegrationTests, BackBufferReturnsNullAttachments)
{
    auto target = GetDevice()->CreateRenderTargetBackBuffer(800, 600);

    EXPECT_EQ(target->GetColorAttachment(), nullptr);
    EXPECT_EQ(target->GetDepthAttachment(), nullptr);
}

TEST_F(RenderTargetIntegrationTests, BackBufferBeginEndRenderPassDoNotCrash)
{
    auto target = GetDevice()->CreateRenderTargetBackBuffer(800, 600);

    RenderPassDescriptor desc;
    desc.ColorLoadAction = LoadAction::Clear;
    desc.ClearColor = {0.0f, 0.0f, 0.0f, 1.0f};

    EXPECT_NO_THROW(RenderCommand::BeginRenderPass(target, desc));
    EXPECT_NO_THROW(RenderCommand::EndRenderPass());
}

TEST_F(RenderTargetIntegrationTests, BackBufferIsNotFramebuffer)
{
    auto target = GetDevice()->CreateRenderTargetBackBuffer(800, 600);
    auto *glTarget = dynamic_cast<GLRenderTarget *>(target.get());

    ASSERT_NE(glTarget, nullptr);
    EXPECT_TRUE(glTarget->IsBackBuffer());
    EXPECT_FALSE(glTarget->IsBackBuffer() == false);
    EXPECT_EQ(glTarget->GetFramebuffer(), nullptr);
}

// --- Framebuffer target tests ---

TEST_F(RenderTargetIntegrationTests, FromFramebufferCreation)
{
    FramebufferSpecification spec{};
    spec.Width = 256;
    spec.Height = 256;
    spec.Attachments = {
        TextureFormat::RGBA8,
        TextureFormat::Depth24Stencil8};

    auto fb = GetDevice()->CreateFramebuffer(spec);
    auto target = GetDevice()->CreateRenderTargetFromFramebuffer(fb);

    ASSERT_NE(target, nullptr);
    EXPECT_EQ(target->GetWidth(), 256u);
    EXPECT_EQ(target->GetHeight(), 256u);
}

TEST_F(RenderTargetIntegrationTests, FromFramebufferIsFramebuffer)
{
    FramebufferSpecification spec{};
    spec.Width = 128;
    spec.Height = 128;
    spec.Attachments = {TextureFormat::RGBA8};

    auto fb = GetDevice()->CreateFramebuffer(spec);
    auto target = GetDevice()->CreateRenderTargetFromFramebuffer(fb);
    auto *glTarget = dynamic_cast<GLRenderTarget *>(target.get());

    ASSERT_NE(glTarget, nullptr);
    EXPECT_FALSE(glTarget->IsBackBuffer());
    EXPECT_NE(glTarget->GetFramebuffer(), nullptr);
}

TEST_F(RenderTargetIntegrationTests, FromFramebufferDelegatesAttachments)
{
    FramebufferSpecification spec{};
    spec.Width = 128;
    spec.Height = 128;
    spec.Attachments = {
        TextureFormat::RGBA8,
        TextureFormat::Depth24Stencil8};

    auto fb = GetDevice()->CreateFramebuffer(spec);
    auto target = GetDevice()->CreateRenderTargetFromFramebuffer(fb);

    EXPECT_NE(target->GetColorAttachment(), nullptr);
    EXPECT_NE(target->GetDepthAttachment(), nullptr);
    EXPECT_EQ(target->GetColorAttachment(), fb->GetColorAttachment());
    EXPECT_EQ(target->GetDepthAttachment(), fb->GetDepthAttachment());
}

TEST_F(RenderTargetIntegrationTests, FromFramebufferDepthOnly)
{
    FramebufferSpecification spec{};
    spec.Width = 512;
    spec.Height = 512;
    spec.Attachments = {
        TextureFormat::Depth};

    auto fb = GetDevice()->CreateFramebuffer(spec);
    auto target = GetDevice()->CreateRenderTargetFromFramebuffer(fb);

    EXPECT_EQ(target->GetColorAttachment(), nullptr);
    EXPECT_NE(target->GetDepthAttachment(), nullptr);
}

TEST_F(RenderTargetIntegrationTests, FromFramebufferBeginEndRenderPassDoNotCrash)
{
    FramebufferSpecification spec{};
    spec.Width = 32;
    spec.Height = 32;
    spec.Attachments = {
        TextureFormat::RGBA8,
        TextureFormat::Depth24Stencil8};

    auto fb = GetDevice()->CreateFramebuffer(spec);
    auto target = GetDevice()->CreateRenderTargetFromFramebuffer(fb);

    RenderPassDescriptor desc;
    desc.ColorLoadAction = LoadAction::Clear;
    desc.ClearColor = {0.1f, 0.1f, 0.1f, 1.0f};
    desc.DepthLoadAction = LoadAction::Clear;

    EXPECT_NO_THROW(RenderCommand::BeginRenderPass(target, desc));
    EXPECT_NO_THROW(RenderCommand::EndRenderPass());
}
