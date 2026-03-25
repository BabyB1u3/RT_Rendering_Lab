#include "renderer/SceneRenderer.h"

#include <gtest/gtest.h>

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
