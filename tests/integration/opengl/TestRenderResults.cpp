#include <array>
#include <cstdint>

#include <gtest/gtest.h>

#include <glad/glad.h>

#include "GLTestContext.h"
#include "graphics/Buffers.h"
#include "graphics/Framebuffer.h"
#include "graphics/GraphicsDevice.h"
#include "graphics/RenderCommand.h"
#include "graphics/RenderTypes.h"
#include "graphics/interface/IFramebuffer.h"
#include "graphics/interface/IRenderTarget.h"
#include "graphics/interface/IShader.h"
#include "graphics/interface/IVertexArray.h"
#include "graphics/interface/IVertexBuffer.h"
#include "graphics/opengl/GLCast.h"
#include "graphics/opengl/GLFramebuffer.h"

class RenderResultsIntegrationTests : public ::testing::Test
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

    inline static Scope<GlTestContext> s_Context;
};

TEST_F(RenderResultsIntegrationTests, BeginRenderPassClearColorWritesFramebuffer)
{
    FramebufferSpecification spec{};
    spec.Width = 8;
    spec.Height = 8;
    spec.Attachments = {
        TextureFormat::RGBA8,
        TextureFormat::Depth24Stencil8};

    auto framebuffer = GetDevice()->CreateFramebuffer(spec);
    auto target = GetDevice()->CreateRenderTargetFromFramebuffer(framebuffer);

    RenderPassDescriptor desc;
    desc.ClearColor = {0.25f, 0.5f, 0.75f, 1.0f};
    desc.DepthLoadAction = LoadAction::Clear;

    RenderCommand::BeginRenderPass(target, desc);
    RenderCommand::EndRenderPass();

    const auto pixel = ReadRgbaPixel(framebuffer, 4, 4);
    EXPECT_NEAR(pixel[0], 64, 1);
    EXPECT_NEAR(pixel[1], 128, 1);
    EXPECT_NEAR(pixel[2], 191, 1);
    EXPECT_EQ(pixel[3], 255);
}

TEST_F(RenderResultsIntegrationTests, DrawArraysRendersTriangleIntoFramebuffer)
{
    static constexpr float kTriangleVertices[] = {
        -0.8f, -0.8f,
        0.8f, -0.8f,
        0.0f, 0.8f};

    static constexpr const char *kTriangleVertexSrc = R"(
#version 330 core
layout(location = 0) in vec2 a_Position;
void main()
{
    gl_Position = vec4(a_Position, 0.0, 1.0);
}
)";

    static constexpr const char *kTriangleFragmentSrc = R"(
#version 330 core
out vec4 FragColor;
void main()
{
    FragColor = vec4(1.0, 0.0, 0.0, 1.0);
}
)";

    FramebufferSpecification spec{};
    spec.Width = 8;
    spec.Height = 8;
    spec.Attachments = {
        TextureFormat::RGBA8};

    auto framebuffer = GetDevice()->CreateFramebuffer(spec);
    auto target = GetDevice()->CreateRenderTargetFromFramebuffer(framebuffer);
    auto shader = GetDevice()->CreateShaderFromSource("TriangleResult", kTriangleVertexSrc, kTriangleFragmentSrc);
    auto vertexBuffer = GetDevice()->CreateVertexBuffer(kTriangleVertices, sizeof(kTriangleVertices));
    vertexBuffer->SetLayout({{ShaderDataType::Float2, "a_Position"}});

    auto vertexArray = GetDevice()->CreateVertexArray();
    vertexArray->AddVertexBuffer(vertexBuffer);

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
    RenderCommand::SetViewport(0, 0, spec.Width, spec.Height);
    shader->Bind();
    vertexArray->Bind();
    RenderCommand::DrawArrays(GL_TRIANGLES, 0, 3);
    RenderCommand::EndRenderPass();

    const auto centerPixel = ReadRgbaPixel(framebuffer, 4, 4);
    EXPECT_GT(centerPixel[0], 200);
    EXPECT_LT(centerPixel[1], 20);
    EXPECT_LT(centerPixel[2], 20);
    EXPECT_EQ(centerPixel[3], 255);

    const auto cornerPixel = ReadRgbaPixel(framebuffer, 0, 7);
    EXPECT_LT(cornerPixel[0], 20);
    EXPECT_LT(cornerPixel[1], 20);
    EXPECT_LT(cornerPixel[2], 20);
    EXPECT_EQ(cornerPixel[3], 255);
}
