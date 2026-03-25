#include <array>
#include <cstdint>

#include <gtest/gtest.h>

#include <glad/glad.h>

#include "GLTestContext.h"
#include "ShaderTestUtils.h"
#include "core/FileSystem.h"
#include "graphics/Framebuffer.h"
#include "graphics/GraphicsDevice.h"
#include "graphics/Material.h"
#include "graphics/MeshFactory.h"
#include "graphics/RenderCommand.h"
#include "graphics/RenderTypes.h"
#include "graphics/Texture.h"
#include "graphics/interface/IFramebuffer.h"
#include "graphics/interface/IRenderTarget.h"
#include "graphics/interface/ITexture2D.h"
#include "graphics/opengl/GLCast.h"
#include "graphics/opengl/GLFramebuffer.h"
#include "renderer/RenderContext.h"
#include "renderer/RenderItem.h"
#include "renderer/SceneRendererTypes.h"
#include "renderer/passes/ForwardPass.h"
#include "renderer/passes/ShadowPass.h"
#include "renderer/passes/TexturePreviewPass.h"
#include "scene/Camera.h"
#include "scene/SceneData.h"

class RenderPassContractTests : public ::testing::Test
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

    static void SkipIfCompiledShadersMissing()
    {
        ShaderTestUtils::SkipOrFailIfShadersMissing("Render pass contract tests require all shaders.");
    }

    static std::array<uint8_t, 4> ReadRgbaPixel(const Ref<IFramebuffer> &framebuffer, int x, int y)
    {
        auto *glFramebuffer = AsGL<GLFramebuffer>(framebuffer);
        std::array<uint8_t, 4> pixel{0, 0, 0, 0};

        glBindFramebuffer(GL_READ_FRAMEBUFFER, glFramebuffer->GetRendererID());
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

        return pixel;
    }

    static float ReadDepthPixel(const Ref<IFramebuffer> &framebuffer, int x, int y)
    {
        auto *glFramebuffer = AsGL<GLFramebuffer>(framebuffer);
        float depth = 0.0f;

        glBindFramebuffer(GL_READ_FRAMEBUFFER, glFramebuffer->GetRendererID());
        glReadPixels(x, y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

        return depth;
    }

    static Ref<ITexture2D> CreateSolidColorTexture(uint32_t rgba)
    {
        TextureSpecification spec;
        spec.Width = 1;
        spec.Height = 1;
        spec.Format = TextureFormat::RGBA8;

        auto texture = GetDevice()->CreateTexture2D(spec);
        texture->SetData(&rgba);
        return texture;
    }

    static Ref<IFramebuffer> CreateColorFramebuffer(uint32_t width, uint32_t height, bool withDepth = true)
    {
        FramebufferSpecification spec{};
        spec.Width = width;
        spec.Height = height;
        spec.Attachments = withDepth
                               ? FramebufferAttachmentSpecification{{TextureFormat::RGBA8}, {TextureFormat::Depth24Stencil8}}
                               : FramebufferAttachmentSpecification{{TextureFormat::RGBA8}};

        return GetDevice()->CreateFramebuffer(spec);
    }

    inline static Scope<GlTestContext> s_Context;
};

TEST_F(RenderPassContractTests, ForwardPassRenderToTargetOwnsFramebufferAndResizesIt)
{
    SkipIfCompiledShadersMissing();

    ForwardPass pass(64, 32, true);
    auto framebuffer = pass.GetFramebuffer();
    ASSERT_NE(framebuffer, nullptr);

    EXPECT_EQ(framebuffer->GetSpecification().Width, 64u);
    EXPECT_EQ(framebuffer->GetSpecification().Height, 32u);

    pass.Resize(128, 48);

    EXPECT_EQ(framebuffer->GetSpecification().Width, 128u);
    EXPECT_EQ(framebuffer->GetSpecification().Height, 48u);
}

TEST_F(RenderPassContractTests, ForwardPassBackBufferModeDoesNotOwnFramebuffer)
{
    SkipIfCompiledShadersMissing();

    ForwardPass pass(64, 32, false);

    EXPECT_EQ(pass.GetFramebuffer(), nullptr);
}

TEST_F(RenderPassContractTests, ForwardPassRenderToTargetClearsOutputWithoutShadowMap)
{
    SkipIfCompiledShadersMissing();

    ForwardPass pass(8, 8, true, {0.2f, 0.4f, 0.6f, 1.0f});
    SceneData scene;
    Camera camera;
    SceneView view{scene, camera, 8, 8};
    FrameResources resources;
    resources.LightViewProjection = glm::mat4(1.0f);
    RenderContext ctx{view, SceneRendererSpecification{}, resources, SceneRendererOutput::FinalColor};

    ASSERT_NE(pass.GetFramebuffer(), nullptr);
    EXPECT_NO_THROW(pass.Execute(ctx));

    const auto pixel = ReadRgbaPixel(pass.GetFramebuffer(), 4, 4);
    EXPECT_NEAR(pixel[0], 51, 1);
    EXPECT_NEAR(pixel[1], 102, 1);
    EXPECT_NEAR(pixel[2], 153, 1);
    EXPECT_EQ(pixel[3], 255);
}

TEST_F(RenderPassContractTests, ForwardPassBackBufferModeUsesContextBackBuffer)
{
    SkipIfCompiledShadersMissing();

    ForwardPass pass(8, 8, false, {0.3f, 0.1f, 0.2f, 1.0f});

    auto backBufferFramebuffer = CreateColorFramebuffer(8, 8);
    auto backBufferTarget = GetDevice()->CreateRenderTargetFromFramebuffer(backBufferFramebuffer);

    SceneData scene;
    scene.RenderItems.push_back(RenderItem{});

    Camera camera;
    SceneView view{scene, camera, 8, 8};
    FrameResources resources;
    resources.LightViewProjection = glm::mat4(1.0f);
    resources.BackBuffer = backBufferTarget;
    RenderContext ctx{view, SceneRendererSpecification{}, resources, SceneRendererOutput::FinalColor};

    EXPECT_NO_THROW(pass.Execute(ctx));

    const auto pixel = ReadRgbaPixel(backBufferFramebuffer, 4, 4);
    EXPECT_NEAR(pixel[0], 77, 1);
    EXPECT_NEAR(pixel[1], 26, 1);
    EXPECT_NEAR(pixel[2], 51, 1);
    EXPECT_EQ(pixel[3], 255);
}

TEST_F(RenderPassContractTests, ForwardPassWithoutAlbedoTextureUsesMaterialAlbedo)
{
    SkipIfCompiledShadersMissing();

    ForwardPass pass(8, 8, true, {0.0f, 0.0f, 0.0f, 1.0f});

    auto material = CreateRef<Material>();
    material->SetVec3("u_Albedo", {0.0f, 1.0f, 0.0f});
    material->SetFloat("u_AmbientStrength", 1.0f);
    material->SetFloat("u_SpecularPower", 1.0f);

    SceneData scene;
    scene.MainDirectionalLight.Color = {0.0f, 0.0f, 0.0f};
    scene.MainDirectionalLight.Direction = {0.0f, 0.0f, -1.0f};
    scene.RenderItems.push_back({MeshFactory::CreateFullscreenQuad(),
                                 material,
                                 {}});

    Camera camera;
    SceneView view{scene, camera, 8, 8};
    FrameResources resources;
    resources.LightViewProjection = glm::mat4(1.0f);
    RenderContext ctx{view, SceneRendererSpecification{}, resources, SceneRendererOutput::FinalColor};

    ASSERT_NE(pass.GetFramebuffer(), nullptr);
    EXPECT_NO_THROW(pass.Execute(ctx));

    const auto pixel = ReadRgbaPixel(pass.GetFramebuffer(), 4, 4);
    EXPECT_LT(pixel[0], 20);
    EXPECT_GT(pixel[1], 200);
    EXPECT_LT(pixel[2], 20);
}

TEST_F(RenderPassContractTests, ForwardPassWithAlbedoTextureUsesTextureSample)
{
    SkipIfCompiledShadersMissing();

    ForwardPass pass(8, 8, true, {0.0f, 0.0f, 0.0f, 1.0f});

    auto material = CreateRef<Material>();
    material->SetVec3("u_Albedo", {1.0f, 1.0f, 1.0f});
    material->SetFloat("u_AmbientStrength", 1.0f);
    material->SetFloat("u_SpecularPower", 1.0f);
    material->SetTexture(TextureSlot::Albedo, CreateSolidColorTexture(0xFF0000FFu));

    SceneData scene;
    scene.MainDirectionalLight.Color = {0.0f, 0.0f, 0.0f};
    scene.MainDirectionalLight.Direction = {0.0f, 0.0f, -1.0f};
    scene.RenderItems.push_back({MeshFactory::CreateFullscreenQuad(),
                                 material,
                                 {}});

    Camera camera;
    SceneView view{scene, camera, 8, 8};
    FrameResources resources;
    resources.LightViewProjection = glm::mat4(1.0f);
    RenderContext ctx{view, SceneRendererSpecification{}, resources, SceneRendererOutput::FinalColor};

    ASSERT_NE(pass.GetFramebuffer(), nullptr);
    EXPECT_NO_THROW(pass.Execute(ctx));

    const auto pixel = ReadRgbaPixel(pass.GetFramebuffer(), 4, 4);
    EXPECT_GT(pixel[0], 200);
    EXPECT_LT(pixel[1], 20);
    EXPECT_LT(pixel[2], 20);
}

TEST_F(RenderPassContractTests, ShadowPassOwnsDepthFramebufferAndResizeUpdatesDepthTexture)
{
    SkipIfCompiledShadersMissing();

    ShadowPass pass(32, 16);
    auto framebuffer = pass.GetFramebuffer();
    ASSERT_NE(framebuffer, nullptr);
    ASSERT_NE(pass.GetDepthTexture(), nullptr);

    EXPECT_EQ(pass.GetDepthTexture()->GetWidth(), 32u);
    EXPECT_EQ(pass.GetDepthTexture()->GetHeight(), 16u);

    pass.Resize(64, 24);

    EXPECT_EQ(pass.GetDepthTexture()->GetWidth(), 64u);
    EXPECT_EQ(pass.GetDepthTexture()->GetHeight(), 24u);
}

TEST_F(RenderPassContractTests, ShadowPassClearsDepthForEmptyScene)
{
    SkipIfCompiledShadersMissing();

    ShadowPass pass(8, 8);
    SceneData scene;
    scene.RenderItems.push_back(RenderItem{});

    Camera camera;
    SceneView view{scene, camera, 8, 8};
    FrameResources resources;
    resources.LightViewProjection = glm::mat4(1.0f);
    RenderContext ctx{view, SceneRendererSpecification{}, resources, SceneRendererOutput::FinalColor};

    ASSERT_NE(pass.GetFramebuffer(), nullptr);
    EXPECT_NO_THROW(pass.Execute(ctx));
    EXPECT_NEAR(ReadDepthPixel(pass.GetFramebuffer(), 4, 4), 1.0f, 1e-4f);
}

TEST_F(RenderPassContractTests, TexturePreviewPassRoutesFinalColorToBackBufferTarget)
{
    SkipIfCompiledShadersMissing();

    TexturePreviewPass pass;
    auto outputFramebuffer = CreateColorFramebuffer(8, 8, false);
    auto outputTarget = GetDevice()->CreateRenderTargetFromFramebuffer(outputFramebuffer);

    SceneData scene;
    Camera camera;
    SceneView view{scene, camera, 8, 8};
    FrameResources resources;
    resources.SceneColor = CreateSolidColorTexture(0xFF0000FFu);
    resources.BackBuffer = outputTarget;
    RenderContext ctx{view, SceneRendererSpecification{}, resources, SceneRendererOutput::FinalColor};

    EXPECT_NO_THROW(pass.Execute(ctx));

    const auto pixel = ReadRgbaPixel(outputFramebuffer, 4, 4);
    EXPECT_GT(pixel[0], 200);
    EXPECT_LT(pixel[1], 20);
    EXPECT_LT(pixel[2], 20);
    EXPECT_EQ(pixel[3], 255);
}

TEST_F(RenderPassContractTests, TexturePreviewPassRoutesShadowMapAsGrayscale)
{
    SkipIfCompiledShadersMissing();

    TexturePreviewPass pass;

    FramebufferSpecification depthSpec{};
    depthSpec.Width = 1;
    depthSpec.Height = 1;
    depthSpec.Attachments = {TextureFormat::Depth};
    auto depthFramebuffer = GetDevice()->CreateFramebuffer(depthSpec);
    auto depthTarget = GetDevice()->CreateRenderTargetFromFramebuffer(depthFramebuffer);

    RenderPassDescriptor depthClear;
    depthClear.ColorLoadAction = LoadAction::DontCare;
    depthClear.ColorStoreAction = StoreAction::DontCare;
    depthClear.DepthLoadAction = LoadAction::Clear;
    depthClear.DepthStoreAction = StoreAction::Store;
    depthClear.ClearDepth = 0.25f;
    RenderCommand::BeginRenderPass(depthTarget, depthClear);
    RenderCommand::EndRenderPass();

    auto outputFramebuffer = CreateColorFramebuffer(8, 8, false);
    auto outputTarget = GetDevice()->CreateRenderTargetFromFramebuffer(outputFramebuffer);

    SceneData scene;
    Camera camera;
    SceneView view{scene, camera, 8, 8};
    FrameResources resources;
    resources.ShadowMap = depthFramebuffer->GetDepthAttachment();
    resources.BackBuffer = outputTarget;
    RenderContext ctx{view, SceneRendererSpecification{}, resources, SceneRendererOutput::ShadowMap};

    EXPECT_NO_THROW(pass.Execute(ctx));

    const auto pixel = ReadRgbaPixel(outputFramebuffer, 4, 4);
    EXPECT_NEAR(pixel[0], 64, 2);
    EXPECT_NEAR(pixel[1], 64, 2);
    EXPECT_NEAR(pixel[2], 64, 2);
    EXPECT_EQ(pixel[3], 255);
}
