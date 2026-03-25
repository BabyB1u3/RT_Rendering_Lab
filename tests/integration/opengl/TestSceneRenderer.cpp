#include "renderer/SceneRenderer.h"

#include <gtest/gtest.h>

#include <array>

#include <glad/glad.h>

#include "GLTestContext.h"
#include "core/FileSystem.h"
#include "graphics/Framebuffer.h"
#include "graphics/interface/IFramebuffer.h"
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
        const auto shaderDir = FileSystem::GetCompiledShaderDir() / "glsl";
        const auto hasArtifacts = [&](const char *name)
        {
            return FileSystem::Exists(shaderDir / (std::string(name) + ".vert.glsl")) &&
                   FileSystem::Exists(shaderDir / (std::string(name) + ".frag.glsl"));
        };

        if (!hasArtifacts("ForwardLit") || !hasArtifacts("ShadowDepth") || !hasArtifacts("TexturePreview"))
            GTEST_SKIP() << "Compiled GLSL artifacts not found for SceneRenderer integration tests";
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
