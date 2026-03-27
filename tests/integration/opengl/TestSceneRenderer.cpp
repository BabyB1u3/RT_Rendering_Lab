#include "renderer/SceneRenderer.h"

#include <gtest/gtest.h>

#include <array>

#include <glad/glad.h>

#include "GLTestContext.h"
#include "ShaderTestUtils.h"
#include "core/FileSystem.h"
#include "graphics/Framebuffer.h"
#include "graphics/Material.h"
#include "graphics/MeshFactory.h"
#include "graphics/interfaces/IFramebuffer.h"
#include "renderer/passes/ForwardPass.h"
#include "scene/Camera.h"
#include "scene/SceneData.h"

class SceneRendererIntegrationTests : public ::testing::Test
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

    static std::array<uint8_t, 4> ReadDefaultFramebufferPixel(int x, int y)
    {
        std::array<uint8_t, 4> pixel{0, 0, 0, 0};
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glReadBuffer(GL_BACK);
        glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
        return pixel;
    }

    static void SkipIfCompiledShadersMissing()
    {
        ShaderTestUtils::SkipOrFailIfShadersMissing("SceneRenderer integration tests require all shaders.");
    }

    inline static Scope<GlTestContext> s_Context;
};

TEST_F(SceneRendererIntegrationTests, ResizeZeroLeavesForwardFramebufferUnchanged)
{
    SkipIfCompiledShadersMissing();

    SceneRenderer renderer(64, 32);
    const auto framebuffer = renderer.GetForwardPass()->GetFramebuffer();
    ASSERT_NE(framebuffer, nullptr);

    const FramebufferSpecification beforeSpec = framebuffer->GetSpecification();

    renderer.Resize(0, 0);

    const FramebufferSpecification afterSpec = framebuffer->GetSpecification();
    EXPECT_EQ(afterSpec.Width, beforeSpec.Width);
    EXPECT_EQ(afterSpec.Height, beforeSpec.Height);
}

TEST_F(SceneRendererIntegrationTests, ResizeUpdatesForwardFramebufferDimensions)
{
    SkipIfCompiledShadersMissing();

    SceneRenderer renderer(64, 32);
    const auto framebuffer = renderer.GetForwardPass()->GetFramebuffer();
    ASSERT_NE(framebuffer, nullptr);

    renderer.Resize(96, 48);

    const FramebufferSpecification spec = framebuffer->GetSpecification();
    EXPECT_EQ(spec.Width, 96u);
    EXPECT_EQ(spec.Height, 48u);
}

TEST_F(SceneRendererIntegrationTests, RenderEmptySceneDoesNotThrow)
{
    SkipIfCompiledShadersMissing();

    SceneRenderer renderer(64, 64);
    SceneData scene;
    Camera camera;

    EXPECT_NO_THROW(renderer.Render(scene, camera));
}

TEST_F(SceneRendererIntegrationTests, FinalColorModePresentsForwardClearColor)
{
    SkipIfCompiledShadersMissing();

    SceneRendererSpecification spec;
    spec.ClearColor = {0.2f, 0.4f, 0.6f, 1.0f};

    SceneRenderer renderer(64, 64, spec);
    renderer.SetOutputMode(SceneRendererOutput::FinalColor);

    SceneData scene;
    Camera camera;

    ASSERT_NO_THROW(renderer.Render(scene, camera));

    const auto pixel = ReadDefaultFramebufferPixel(32, 32);
    EXPECT_NEAR(pixel[0], 51, 1);
    EXPECT_NEAR(pixel[1], 102, 1);
    EXPECT_NEAR(pixel[2], 153, 1);
    EXPECT_EQ(pixel[3], 255);
}

TEST_F(SceneRendererIntegrationTests, ShadowMapModePresentsClearedShadowPreview)
{
    SkipIfCompiledShadersMissing();

    SceneRenderer renderer(64, 64);
    renderer.SetOutputMode(SceneRendererOutput::ShadowMap);

    SceneData scene;
    Camera camera;

    ASSERT_NO_THROW(renderer.Render(scene, camera));

    const auto pixel = ReadDefaultFramebufferPixel(32, 32);
    EXPECT_NEAR(pixel[0], 255, 1);
    EXPECT_NEAR(pixel[1], 255, 1);
    EXPECT_NEAR(pixel[2], 255, 1);
    EXPECT_EQ(pixel[3], 255);
}

TEST_F(SceneRendererIntegrationTests, FinalColorModePresentsRenderedSceneColor)
{
    SkipIfCompiledShadersMissing();

    SceneRendererSpecification spec;
    spec.ClearColor = {0.0f, 0.0f, 0.0f, 1.0f};

    SceneRenderer renderer(64, 64, spec);
    renderer.SetOutputMode(SceneRendererOutput::FinalColor);

    auto material = CreateRef<Material>();
    material->SetVec3("u_Albedo", {0.0f, 1.0f, 0.0f});
    material->SetFloat("u_AmbientStrength", 1.0f);
    material->SetFloat("u_SpecularPower", 1.0f);

    SceneData scene;
    scene.MainDirectionalLight.Color = {0.0f, 0.0f, 0.0f};
    scene.RenderItems.push_back({MeshFactory::CreateFullscreenQuad(),
                                 material,
                                 {}});

    Camera camera;

    ASSERT_NO_THROW(renderer.Render(scene, camera));

    const auto pixel = ReadDefaultFramebufferPixel(32, 32);
    EXPECT_LT(pixel[0], 20);
    EXPECT_GT(pixel[1], 200);
    EXPECT_LT(pixel[2], 20);
    EXPECT_EQ(pixel[3], 255);
}

TEST_F(SceneRendererIntegrationTests, ShadowMapModePresentsRenderedShadowDepth)
{
    SkipIfCompiledShadersMissing();

    SceneRenderer renderer(64, 64);
    renderer.SetOutputMode(SceneRendererOutput::ShadowMap);

    auto material = CreateRef<Material>();
    material->SetVec3("u_Albedo", {1.0f, 1.0f, 1.0f});

    RenderItem item;
    item.Mesh = MeshFactory::CreateCube();
    item.Material = material;
    item.Transform.Scale = {4.0f, 4.0f, 4.0f};

    SceneData scene;
    scene.RenderItems.push_back(item);

    Camera camera;

    ASSERT_NO_THROW(renderer.Render(scene, camera));

    const auto pixel = ReadDefaultFramebufferPixel(32, 32);
    EXPECT_EQ(pixel[0], pixel[1]);
    EXPECT_EQ(pixel[1], pixel[2]);
    EXPECT_LT(pixel[0], 250);
    EXPECT_GT(pixel[0], 0);
    EXPECT_EQ(pixel[3], 255);
}
