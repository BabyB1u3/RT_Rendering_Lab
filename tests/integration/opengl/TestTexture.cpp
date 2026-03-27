#include <gtest/gtest.h>
#include <glad/glad.h>
#include <memory>
#include <array>

#include "GLTestContext.h"
#include "graphics/Framebuffer.h"
#include "graphics/GraphicsDevice.h"
#include "graphics/MeshFactory.h"
#include "graphics/RenderCommand.h"
#include "graphics/RenderTypes.h"
#include "graphics/Texture.h"
#include "graphics/interface/IFramebuffer.h"
#include "graphics/interface/IRenderTarget.h"
#include "graphics/interface/ITexture2D.h"
#include "graphics/interface/IShader.h"
#include "graphics/opengl/GLCast.h"
#include "graphics/opengl/GLFramebuffer.h"

class TextureIntegrationTests : public ::testing::Test
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

    static std::array<uint8_t, 4> RenderTextureToPixel(const Ref<ITexture2D> &texture)
    {
        static constexpr const char *kVertexSrc = R"(
#version 330 core
layout(location = 0) in vec3 a_Position;
layout(location = 2) in vec2 a_TexCoord;
out vec2 v_TexCoord;
void main()
{
    v_TexCoord = a_TexCoord;
    gl_Position = vec4(a_Position, 1.0);
}
)";

        static constexpr const char *kFragmentSrc = R"(
#version 330 core
in vec2 v_TexCoord;
out vec4 FragColor;
uniform sampler2D u_Texture;
void main()
{
    FragColor = texture(u_Texture, v_TexCoord);
}
)";

        FramebufferSpecification spec{};
        spec.Width = 8;
        spec.Height = 8;
        spec.Attachments = {TextureFormat::RGBA8};

        auto framebuffer = GetDevice()->CreateFramebuffer(spec);
        auto target = GetDevice()->CreateRenderTargetFromFramebuffer(framebuffer);
        auto shader = GetDevice()->CreateShaderFromSource("TextureSamplingShader", kVertexSrc, kFragmentSrc);
        auto quad = MeshFactory::CreateFullscreenQuad();

        RenderPassDescriptor desc;
        desc.ClearColor = {0.0f, 0.0f, 0.0f, 1.0f};
        desc.DepthLoadAction = LoadAction::DontCare;
        desc.DepthStoreAction = StoreAction::DontCare;

        PipelineState pso;
        pso.DepthTestEnabled = false;
        pso.DepthWriteEnabled = false;
        pso.BlendEnabled = false;
        pso.CullFaceEnabled = false;

        RenderCommand::BeginRenderPass(target, desc);
        RenderCommand::SetPipelineState(pso);
        RenderCommand::SetViewport(0, 0, 8, 8);
        shader->Bind();
        shader->SetInt("u_Texture", 0);
        RenderCommand::SetTexture(0, texture);
        RenderCommand::DrawIndexed(quad->GetVertexArray());
        RenderCommand::EndRenderPass();

        return ReadRgbaPixel(framebuffer, 4, 4);
    }

    inline static Scope<GlTestContext> s_Context;
};

TEST_F(TextureIntegrationTests, CreateTextureFromSpecification)
{
    TextureSpecification spec{};
    spec.Width = 2;
    spec.Height = 2;
    spec.Format = TextureFormat::RGBA8;

    auto texture = GetDevice()->CreateTexture2D(spec);

    ASSERT_NE(texture, nullptr);
    EXPECT_EQ(texture->GetWidth(), 2u);
    EXPECT_EQ(texture->GetHeight(), 2u);
    EXPECT_EQ(texture->GetFormat(), TextureFormat::RGBA8);
}

TEST_F(TextureIntegrationTests, SetDataUpdatesSampledPixelsInRealDraw)
{
    TextureSpecification spec{};
    spec.Width = 1;
    spec.Height = 1;
    spec.Format = TextureFormat::RGBA8;

    auto texture = GetDevice()->CreateTexture2D(spec);
    ASSERT_NE(texture, nullptr);

    const std::array<uint8_t, 4> red = {255, 0, 0, 255};
    texture->SetData(red.data());

    const auto redPixel = RenderTextureToPixel(texture);
    EXPECT_GT(redPixel[0], 200);
    EXPECT_LT(redPixel[1], 20);
    EXPECT_LT(redPixel[2], 20);
    EXPECT_EQ(redPixel[3], 255);

    const std::array<uint8_t, 4> blue = {0, 0, 255, 255};
    texture->SetData(blue.data());

    const auto bluePixel = RenderTextureToPixel(texture);
    EXPECT_LT(bluePixel[0], 20);
    EXPECT_LT(bluePixel[1], 20);
    EXPECT_GT(bluePixel[2], 200);
    EXPECT_EQ(bluePixel[3], 255);
}

TEST_F(TextureIntegrationTests, SetDataRejectsUnsupportedDepthFormatWithoutThrowing)
{
    TextureSpecification spec{};
    spec.Width = 1;
    spec.Height = 1;
    spec.Format = TextureFormat::Depth;

    auto texture = GetDevice()->CreateTexture2D(spec);
    ASSERT_NE(texture, nullptr);

    const float depth = 0.5f;

    EXPECT_NO_THROW(texture->SetData(&depth));
}

TEST_F(TextureIntegrationTests, Rgb8TextureSamplesExpectedColorAndOpaqueAlpha)
{
    TextureSpecification spec{};
    spec.Width = 1;
    spec.Height = 1;
    spec.Format = TextureFormat::RGB8;

    auto texture = GetDevice()->CreateTexture2D(spec);
    ASSERT_NE(texture, nullptr);

    const std::array<uint8_t, 3> green = {0, 255, 0};
    texture->SetData(green.data());

    const auto pixel = RenderTextureToPixel(texture);
    EXPECT_LT(pixel[0], 20);
    EXPECT_GT(pixel[1], 200);
    EXPECT_LT(pixel[2], 20);
    EXPECT_EQ(pixel[3], 255);
}

TEST_F(TextureIntegrationTests, BindAndUnbindDoNotCrash)
{
    TextureSpecification spec{};
    spec.Width = 1;
    spec.Height = 1;
    spec.Format = TextureFormat::RGBA8;

    auto texture = GetDevice()->CreateTexture2D(spec);
    ASSERT_NE(texture, nullptr);

    EXPECT_NO_THROW(texture->Bind(0));
    EXPECT_NO_THROW(texture->Unbind());
}

// ── Error path tests ─────────────────────────────────────────────────

TEST_F(TextureIntegrationTests, CreateFromFileReturnsNullOnMissingFile)
{
    auto texture = GetDevice()->CreateTexture2DFromFile("definitely/missing/texture.png");
    EXPECT_EQ(texture, nullptr);
}

TEST_F(TextureIntegrationTests, SetDataOnRedIntegerFormatReturnsWithoutThrowing)
{
    TextureSpecification spec{};
    spec.Width = 1;
    spec.Height = 1;
    spec.Format = TextureFormat::RedInteger;

    auto texture = GetDevice()->CreateTexture2D(spec);
    ASSERT_NE(texture, nullptr);

    const int32_t value = 42;

    EXPECT_NO_THROW(texture->SetData(&value));
}
