#include <array>
#include <cstdint>

#include <gtest/gtest.h>

#include <glad/glad.h>

#include "GLTestContext.h"
#include "graphics/Buffers.h"
#include "graphics/Framebuffer.h"
#include "graphics/GraphicsDevice.h"
#include "graphics/MeshFactory.h"
#include "graphics/RenderCommand.h"
#include "graphics/RenderTypes.h"
#include "graphics/interfaces/IFramebuffer.h"
#include "graphics/interfaces/IRenderTarget.h"
#include "graphics/interfaces/IShader.h"
#include "graphics/interfaces/ITexture2D.h"
#include "graphics/interfaces/IVertexArray.h"
#include "graphics/interfaces/IVertexBuffer.h"
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

    static float ReadDepthPixel(const Ref<IFramebuffer> &framebuffer, int x, int y)
    {
        auto *glFramebuffer = AsGL<GLFramebuffer>(framebuffer);
        float depth = 0.0f;

        glBindFramebuffer(GL_READ_FRAMEBUFFER, glFramebuffer->GetRendererID());
        glReadPixels(x, y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

        return depth;
    }

    static Ref<IFramebuffer> CreateFramebuffer(uint32_t width, uint32_t height, bool withDepth = true)
    {
        FramebufferSpecification spec{};
        spec.Width = width;
        spec.Height = height;
        spec.Attachments = withDepth
            ? FramebufferAttachmentSpecification{{TextureFormat::RGBA8}, {TextureFormat::Depth24Stencil8}}
            : FramebufferAttachmentSpecification{{TextureFormat::RGBA8}};

        return GetDevice()->CreateFramebuffer(spec);
    }

    static Ref<IFramebuffer> CreateDepthOnlyFramebuffer(uint32_t width, uint32_t height)
    {
        FramebufferSpecification spec{};
        spec.Width = width;
        spec.Height = height;
        spec.Attachments = {TextureFormat::Depth};
        return GetDevice()->CreateFramebuffer(spec);
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

TEST_F(RenderResultsIntegrationTests, BeginRenderPassClearDepthWritesFramebufferDepth)
{
    auto framebuffer = CreateFramebuffer(8, 8, true);
    auto target = GetDevice()->CreateRenderTargetFromFramebuffer(framebuffer);

    RenderPassDescriptor desc;
    desc.ColorLoadAction = LoadAction::DontCare;
    desc.ColorStoreAction = StoreAction::DontCare;
    desc.DepthLoadAction = LoadAction::Clear;
    desc.DepthStoreAction = StoreAction::Store;
    desc.ClearDepth = 0.25f;

    RenderCommand::BeginRenderPass(target, desc);
    RenderCommand::EndRenderPass();

    EXPECT_NEAR(ReadDepthPixel(framebuffer, 4, 4), 0.25f, 1e-4f);
}

TEST_F(RenderResultsIntegrationTests, BeginRenderPassFramebufferTargetBindsFramebuffer)
{
    auto framebuffer = CreateFramebuffer(13, 7, false);
    auto target = GetDevice()->CreateRenderTargetFromFramebuffer(framebuffer);
    auto *glFramebuffer = AsGL<GLFramebuffer>(framebuffer);

    RenderPassDescriptor desc;
    desc.DepthLoadAction = LoadAction::DontCare;
    desc.DepthStoreAction = StoreAction::DontCare;

    RenderCommand::BeginRenderPass(target, desc);

    GLint binding = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &binding);
    EXPECT_EQ(static_cast<uint32_t>(binding), glFramebuffer->GetRendererID());

    RenderCommand::EndRenderPass();

    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &binding);
    EXPECT_EQ(binding, 0);
}

TEST_F(RenderResultsIntegrationTests, BeginRenderPassBackBufferTargetBindsDefaultFramebuffer)
{
    auto framebuffer = CreateFramebuffer(8, 8, false);
    framebuffer->Bind();

    auto target = GetDevice()->CreateRenderTargetBackBuffer(11, 5);

    RenderPassDescriptor desc;
    desc.DepthLoadAction = LoadAction::DontCare;
    desc.DepthStoreAction = StoreAction::DontCare;

    RenderCommand::BeginRenderPass(target, desc);

    GLint binding = -1;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &binding);
    EXPECT_EQ(binding, 0);

    RenderCommand::EndRenderPass();

    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &binding);
    EXPECT_EQ(binding, 0);
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

TEST_F(RenderResultsIntegrationTests, DrawIndexedRendersTexturedQuadIntoFramebuffer)
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

    auto framebuffer = CreateFramebuffer(8, 8, false);
    auto target = GetDevice()->CreateRenderTargetFromFramebuffer(framebuffer);
    auto shader = GetDevice()->CreateShaderFromSource("TexturedQuadResult", kVertexSrc, kFragmentSrc);
    auto quad = MeshFactory::CreateFullscreenQuad();

    TextureSpecification texSpec{};
    texSpec.Width = 1;
    texSpec.Height = 1;
    texSpec.Format = TextureFormat::RGBA8;
    auto texture = GetDevice()->CreateTexture2D(texSpec);
    const std::array<uint8_t, 4> pixelData = {255, 0, 255, 255};
    texture->SetData(pixelData.data());

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

    const auto pixel = ReadRgbaPixel(framebuffer, 4, 4);
    EXPECT_GT(pixel[0], 200);
    EXPECT_LT(pixel[1], 20);
    EXPECT_GT(pixel[2], 200);
    EXPECT_EQ(pixel[3], 255);
}

TEST_F(RenderResultsIntegrationTests, DepthOnlyRenderWritesExpectedDepthIntoFramebuffer)
{
    static constexpr const char *kVertexSrc = R"(
#version 330 core
layout(location = 0) in vec3 a_Position;
void main()
{
    gl_Position = vec4(a_Position, 1.0);
}
)";

    static constexpr const char *kFragmentSrc = R"(
#version 330 core
void main()
{
    gl_FragDepth = 0.25;
}
)";

    auto framebuffer = CreateDepthOnlyFramebuffer(8, 8);
    auto target = GetDevice()->CreateRenderTargetFromFramebuffer(framebuffer);
    auto shader = GetDevice()->CreateShaderFromSource("DepthOnlyResult", kVertexSrc, kFragmentSrc);
    auto quad = MeshFactory::CreateFullscreenQuad();

    RenderPassDescriptor desc;
    desc.ColorLoadAction = LoadAction::DontCare;
    desc.ColorStoreAction = StoreAction::DontCare;
    desc.DepthLoadAction = LoadAction::Clear;
    desc.DepthStoreAction = StoreAction::Store;
    desc.ClearDepth = 1.0f;

    PipelineState pso;
    pso.DepthTestEnabled = true;
    pso.DepthWriteEnabled = true;
    pso.BlendEnabled = false;
    pso.CullFaceEnabled = false;

    RenderCommand::BeginRenderPass(target, desc);
    RenderCommand::SetPipelineState(pso);
    RenderCommand::SetViewport(0, 0, 8, 8);
    shader->Bind();
    RenderCommand::DrawIndexed(quad->GetVertexArray());
    RenderCommand::EndRenderPass();

    EXPECT_NEAR(ReadDepthPixel(framebuffer, 4, 4), 0.25f, 1e-4f);
}
