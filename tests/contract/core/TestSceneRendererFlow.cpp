#include <gtest/gtest.h>

#include "graphics/GraphicsDevice.h"
#include "renderer/SceneRenderer.h"
#include "renderer/passes/ForwardPass.h"
#include "renderer/passes/ShadowPass.h"
#include "scene/Camera.h"
#include "scene/SceneData.h"
#include "FakeRenderBackend.h"

namespace
{
    Ref<FakeShader> FindShader(const Ref<FakeGraphicsDevice> &device, const char *name)
    {
        for (const auto &shader : device->CreatedShaders)
        {
            if (shader && shader->GetName() == name)
                return shader;
        }

        return nullptr;
    }

    Ref<ITexture2D> GetBoundTexture(const Ref<FakeShader> &shader, uint32_t slot)
    {
        if (!shader)
            return nullptr;

        const auto it = shader->BoundTextures.find(slot);
        if (it == shader->BoundTextures.end())
            return nullptr;

        return it->second;
    }
}

class SceneRendererFlowContractTests : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_Device = CreateRef<FakeGraphicsDevice>();
        SetDevice(m_Device);
    }

    void TearDown() override
    {
        SetDevice(nullptr);
        m_Device.reset();
    }

    Ref<FakeGraphicsDevice> m_Device;
};

TEST_F(SceneRendererFlowContractTests, RenderExecutesPassesInShadowForwardPreviewOrder)
{
    SceneRenderer renderer(64, 32);
    SceneData scene;
    Camera camera;

    renderer.Render(scene, camera);

    ASSERT_EQ(m_Device->RenderCommand->BeginRenderPasses.size(), 3u);
    EXPECT_EQ(m_Device->RenderCommand->EndRenderPassCount, 3u);

    const auto shadowFramebuffer = renderer.GetShadowPass()->GetFramebuffer();
    const auto forwardFramebuffer = renderer.GetForwardPass()->GetFramebuffer();

    EXPECT_EQ(m_Device->RenderCommand->BeginRenderPasses[0].Target->GetFramebuffer(), shadowFramebuffer);
    EXPECT_EQ(m_Device->RenderCommand->BeginRenderPasses[1].Target->GetFramebuffer(), forwardFramebuffer);
    EXPECT_TRUE(m_Device->RenderCommand->BeginRenderPasses[2].Target->IsBackBuffer());
    EXPECT_EQ(m_Device->RenderCommand->BeginRenderPasses[2].Target, m_Device->LastBackBufferTarget);

    ASSERT_EQ(m_Device->RenderCommand->DrawIndexedCalls.size(), 1u);
    EXPECT_EQ(m_Device->RenderCommand->DrawIndexedCalls[0].PassIndex, 2);
}

TEST_F(SceneRendererFlowContractTests, FinalColorAndShadowMapModesRouteExpectedTexturesToPreviewPass)
{
    SceneRenderer renderer(64, 32);
    SceneData scene;
    Camera camera;

    const auto shadowTexture = renderer.GetShadowPass()->GetDepthTexture();
    const auto sceneColor = renderer.GetForwardPass()->GetFramebuffer()->GetColorAttachment(0);

    renderer.SetOutputMode(SceneRendererOutput::FinalColor);
    renderer.Render(scene, camera);

    const auto forwardShader = FindShader(m_Device, "ForwardLit");
    const auto previewShader = FindShader(m_Device, "TexturePreview");
    ASSERT_NE(forwardShader, nullptr);
    ASSERT_NE(previewShader, nullptr);
    EXPECT_EQ(GetBoundTexture(forwardShader, 1), shadowTexture);
    EXPECT_EQ(GetBoundTexture(previewShader, 1), sceneColor);

    m_Device->RenderCommand->Reset();

    renderer.SetOutputMode(SceneRendererOutput::ShadowMap);
    renderer.Render(scene, camera);

    EXPECT_EQ(GetBoundTexture(forwardShader, 1), shadowTexture);
    EXPECT_EQ(GetBoundTexture(previewShader, 1), shadowTexture);
}

TEST_F(SceneRendererFlowContractTests, ResizePropagatesToForwardFramebufferAndBackBufferTarget)
{
    SceneRenderer renderer(64, 32);

    const auto forwardFramebuffer = renderer.GetForwardPass()->GetFramebuffer();
    auto *fakeForwardFramebuffer = static_cast<FakeFramebuffer *>(forwardFramebuffer.get());
    ASSERT_NE(fakeForwardFramebuffer, nullptr);
    ASSERT_NE(m_Device->LastBackBufferTarget, nullptr);

    renderer.Resize(120, 90);

    const auto resizedSpec = forwardFramebuffer->GetSpecification();
    const uint32_t resizedBackBufferWidth = m_Device->LastBackBufferTarget->GetWidth();
    const uint32_t resizedBackBufferHeight = m_Device->LastBackBufferTarget->GetHeight();

    EXPECT_EQ(resizedSpec.Width, 120u);
    EXPECT_EQ(resizedSpec.Height, 90u);
    EXPECT_EQ(fakeForwardFramebuffer->ResizeCount, 1u);
    EXPECT_EQ(resizedBackBufferWidth, 120u);
    EXPECT_EQ(resizedBackBufferHeight, 90u);
    EXPECT_EQ(m_Device->LastBackBufferTarget->ResizeCount, 1u);

    renderer.Resize(0, 0);

    const auto afterNoOpSpec = forwardFramebuffer->GetSpecification();
    const uint32_t afterNoOpBackBufferWidth = m_Device->LastBackBufferTarget->GetWidth();
    const uint32_t afterNoOpBackBufferHeight = m_Device->LastBackBufferTarget->GetHeight();

    EXPECT_EQ(afterNoOpSpec.Width, 120u);
    EXPECT_EQ(afterNoOpSpec.Height, 90u);
    EXPECT_EQ(fakeForwardFramebuffer->ResizeCount, 1u);
    EXPECT_EQ(afterNoOpBackBufferWidth, 120u);
    EXPECT_EQ(afterNoOpBackBufferHeight, 90u);
    EXPECT_EQ(m_Device->LastBackBufferTarget->ResizeCount, 1u);
}

TEST_F(SceneRendererFlowContractTests, RenderUsesExpectedShadowAndForwardViewportSizes)
{
    SceneRendererSpecification spec;
    spec.ShadowMapWidth = 256;
    spec.ShadowMapHeight = 128;

    SceneRenderer renderer(80, 40, spec);
    SceneData scene;
    Camera camera;

    renderer.Render(scene, camera);

    ASSERT_EQ(m_Device->RenderCommand->Viewports.size(), 2u);
    EXPECT_EQ(m_Device->RenderCommand->Viewports[0].PassIndex, 0);
    EXPECT_EQ(m_Device->RenderCommand->Viewports[0].Width, 256u);
    EXPECT_EQ(m_Device->RenderCommand->Viewports[0].Height, 128u);
    EXPECT_EQ(m_Device->RenderCommand->Viewports[1].PassIndex, 1);
    EXPECT_EQ(m_Device->RenderCommand->Viewports[1].Width, 80u);
    EXPECT_EQ(m_Device->RenderCommand->Viewports[1].Height, 40u);
}
