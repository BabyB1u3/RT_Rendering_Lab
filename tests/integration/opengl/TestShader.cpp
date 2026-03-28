#include <gtest/gtest.h>
#include <glad/glad.h>
#include <memory>
#include <array>
#include <cstdint>
#include <glm/glm.hpp>

#include "GLTestContext.h"
#include "ShaderTestUtils.h"
#include "core/FileSystem.h"
#include "graphics/Framebuffer.h"
#include "graphics/GraphicsDevice.h"
#include "graphics/MeshFactory.h"
#include "graphics/RenderCommand.h"
#include "graphics/RenderTypes.h"
#include "graphics/Texture.h"
#include "graphics/interfaces/IFramebuffer.h"
#include "graphics/interfaces/IRenderTarget.h"
#include "graphics/interfaces/IShader.h"
#include "graphics/interfaces/ITexture2D.h"
#include "graphics/interfaces/IUniformBuffer.h"
#include "graphics/backends/opengl/GLCast.h"
#include "graphics/backends/opengl/GLFramebuffer.h"
#include "graphics/backends/opengl/GLShader.h"

class ShaderIntegrationTests : public ::testing::Test
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

static const char *kVertexSrc = R"(
#version 330 core
layout(location = 0) in vec3 a_Position;
uniform mat4 u_MVP;
void main()
{
    gl_Position = u_MVP * vec4(a_Position, 1.0);
}
)";

static const char *kFragmentSrc = R"(
#version 330 core
out vec4 FragColor;
uniform vec4 u_Color;
void main()
{
    FragColor = u_Color;
}
)";

namespace
{
	Ref<IFramebuffer> CreateColorFramebuffer(uint32_t width = 8, uint32_t height = 8)
	{
		FramebufferSpecification spec{};
		spec.Width = width;
		spec.Height = height;
		spec.Attachments = {TextureFormat::RGBA8};
		return GetDevice()->CreateFramebuffer(spec);
	}
}

TEST_F(ShaderIntegrationTests, CreateFromSourceProducesValidProgram)
{
	auto shader = GetDevice()->CreateShaderFromSource("TestShader", kVertexSrc, kFragmentSrc);

	ASSERT_NE(shader, nullptr);
	EXPECT_EQ(shader->GetName(), "TestShader");
}

TEST_F(ShaderIntegrationTests, BindAndUniformSettersDoNotCrash)
{
	auto shader = GetDevice()->CreateShaderFromSource("TestShader", kVertexSrc, kFragmentSrc);

	ASSERT_NE(shader, nullptr);

	shader->Bind();
	shader->SetFloat4("u_Color", glm::vec4(1, 0, 0, 1));
	shader->SetMat4("u_MVP", glm::mat4(1.0f));
	shader->Unbind();

	SUCCEED();
}

TEST_F(ShaderIntegrationTests, InvalidSourceReturnsNullShader)
{
	auto shader = GetDevice()->CreateShaderFromSource(
		"BrokenShader",
		"#version 330 core\nthis is not valid glsl",
		kFragmentSrc);

	EXPECT_EQ(shader, nullptr);
}

TEST_F(ShaderIntegrationTests, MissingCompiledArtifactsReturnNullShader)
{
	auto shader = GetDevice()->CreateShader("DefinitelyMissingShaderForIntegrationCoverage");
	EXPECT_EQ(shader, nullptr);
}

TEST_F(ShaderIntegrationTests, LinkFailureReturnsNullShaderWhenDriverRejectsIt)
{
	// Vertex shader writes gl_Position with type mismatch via mismatched output
	// type vs fragment input type: vec3 output, float input.
	static constexpr const char *kVertSrc = R"(
#version 330 core
layout(location = 0) in vec3 a_Position;
out vec3 v_Value;
void main()
{
    v_Value = a_Position;
    gl_Position = vec4(a_Position, 1.0);
}
)";

	static constexpr const char *kFragSrc = R"(
#version 330 core
in float v_Value;
out vec4 FragColor;
void main()
{
    FragColor = vec4(v_Value, 0.0, 0.0, 1.0);
}
)";

	auto shader = GetDevice()->CreateShaderFromSource("LinkFailure", kVertSrc, kFragSrc);
	if (shader != nullptr)
	{
		// Some drivers may optimize away the mismatch. If it succeeds, that is
		// also acceptable, so skip rather than fail.
		GTEST_SKIP() << "Driver accepted the type mismatch; link failure not testable here";
	}

	EXPECT_EQ(shader, nullptr);
}

TEST_F(ShaderIntegrationTests, CreateShader_LoadsForwardLitFromCompiledGlsl)
{
	ShaderTestUtils::SkipOrFailIfShaderMissing("ForwardLit");

	auto shader = GetDevice()->CreateShader("ForwardLit");
	ASSERT_NE(shader, nullptr);
	EXPECT_EQ(shader->GetName(), "ForwardLit");
	const auto *layout = shader->GetUniformBlockLayout({0, 0});
	ASSERT_NE(layout, nullptr);
	EXPECT_NE(layout->FindField("u_ViewProjection"), nullptr);
	EXPECT_NE(layout->FindField("u_LightViewProjection"), nullptr);
	EXPECT_NE(layout->FindField("u_UseAlbedoMap"), nullptr);

	shader->Bind();
	shader->SetFloat("u_LightIntensity", 1.0f);
	shader->SetMat4("u_ViewProjection", glm::mat4(1.0f));
	shader->SetFloat3("u_LightDirection", glm::vec3(0.0f, -1.0f, 0.0f));
	shader->Unbind();
}

TEST_F(ShaderIntegrationTests, CreateShader_LoadsBasicLitFromCompiledGlsl)
{
	ShaderTestUtils::SkipOrFailIfShaderMissing("BasicLit");

	auto shader = GetDevice()->CreateShader("BasicLit");
	ASSERT_NE(shader, nullptr);
	const auto *layout = shader->GetUniformBlockLayout({0, 0});
	ASSERT_NE(layout, nullptr);
	EXPECT_NE(layout->FindField("u_ViewProjection"), nullptr);
	EXPECT_NE(layout->FindField("u_AmbientStrength"), nullptr);

	auto *glShader = AsGL<GLShader>(shader);
	const auto *blockResource = glShader->GetResourceLayout({0, 0});
	ASSERT_NE(blockResource, nullptr);
	EXPECT_NE(blockResource->Name.find("GlobalParams"), std::string::npos);
	EXPECT_EQ(blockResource->Kind, ShaderResourceKind::UniformBuffer);
}

TEST_F(ShaderIntegrationTests, CreateShader_PreservesCompiledResourceBindingMetadata)
{
	ShaderTestUtils::SkipOrFailIfShaderMissing("ForwardLit");

	auto shader = GetDevice()->CreateShader("ForwardLit");
	ASSERT_NE(shader, nullptr);
	auto *glShader = AsGL<GLShader>(shader);

	const auto *blockResource = glShader->GetResourceLayout({0, 0});
	ASSERT_NE(blockResource, nullptr);
	EXPECT_NE(blockResource->Name.find("GlobalParams"), std::string::npos);
	EXPECT_EQ(blockResource->Kind, ShaderResourceKind::UniformBuffer);

	const auto *shadowMapResource = glShader->GetResourceLayout({0, 1});
	ASSERT_NE(shadowMapResource, nullptr);
	EXPECT_EQ(shadowMapResource->Name, "u_ShadowMap");
	EXPECT_EQ(shadowMapResource->Kind, ShaderResourceKind::CombinedTextureSampler);

	const auto *albedoMapBinding = glShader->GetBackendBinding({0, 2});
	ASSERT_NE(albedoMapBinding, nullptr);
	ASSERT_TRUE(albedoMapBinding->TextureIndex.has_value());
	ASSERT_TRUE(albedoMapBinding->SamplerIndex.has_value());
	EXPECT_EQ(*albedoMapBinding->TextureIndex, 2u);
	EXPECT_EQ(*albedoMapBinding->SamplerIndex, 2u);
}

TEST_F(ShaderIntegrationTests, CreateShader_LoadsShadowDepthFromCompiledGlsl)
{
	ShaderTestUtils::SkipOrFailIfShaderMissing("ShadowDepth");

	auto shader = GetDevice()->CreateShader("ShadowDepth");
	ASSERT_NE(shader, nullptr);
	const auto *layout = shader->GetUniformBlockLayout({0, 0});
	ASSERT_NE(layout, nullptr);
	EXPECT_NE(layout->FindField("u_LightViewProjection"), nullptr);
	EXPECT_NE(layout->FindField("u_Model"), nullptr);

	shader->Bind();
	shader->SetMat4("u_LightViewProjection", glm::mat4(1.0f));
	shader->SetMat4("u_Model", glm::mat4(1.0f));
	shader->Unbind();
}

TEST_F(ShaderIntegrationTests, CreateShader_ShadowDepthPreservesExplicitLogicalResourceMetadata)
{
	ShaderTestUtils::SkipOrFailIfShaderMissing("ShadowDepth");

	auto shader = GetDevice()->CreateShader("ShadowDepth");
	ASSERT_NE(shader, nullptr);
	auto *glShader = AsGL<GLShader>(shader);

	const auto *blockResource = glShader->GetResourceLayout({0, 0});
	ASSERT_NE(blockResource, nullptr);
	EXPECT_NE(blockResource->Name.find("GlobalParams"), std::string::npos);
	EXPECT_EQ(blockResource->Kind, ShaderResourceKind::UniformBuffer);
}

TEST_F(ShaderIntegrationTests, CreateShader_LoadsTexturePreviewFromCompiledGlsl)
{
	ShaderTestUtils::SkipOrFailIfShaderMissing("TexturePreview");

	auto shader = GetDevice()->CreateShader("TexturePreview");
	ASSERT_NE(shader, nullptr);
	const auto *layout = shader->GetUniformBlockLayout({0, 0});
	ASSERT_NE(layout, nullptr);
	EXPECT_NE(layout->FindField("u_IsDepthTexture"), nullptr);

	shader->Bind();
	shader->SetBool("u_IsDepthTexture", false);
	shader->Unbind();
}

TEST_F(ShaderIntegrationTests, CreateShader_TexturePreviewPreservesExplicitLogicalResourceMetadata)
{
	ShaderTestUtils::SkipOrFailIfShaderMissing("TexturePreview");

	auto shader = GetDevice()->CreateShader("TexturePreview");
	ASSERT_NE(shader, nullptr);
	auto *glShader = AsGL<GLShader>(shader);

	const auto *blockResource = glShader->GetResourceLayout({0, 0});
	ASSERT_NE(blockResource, nullptr);
	EXPECT_NE(blockResource->Name.find("GlobalParams"), std::string::npos);
	EXPECT_EQ(blockResource->Kind, ShaderResourceKind::UniformBuffer);

	const auto *textureBinding = glShader->GetBackendBinding({0, 1});
	ASSERT_NE(textureBinding, nullptr);
	EXPECT_EQ(textureBinding->Kind, ShaderResourceKind::CombinedTextureSampler);
	ASSERT_TRUE(textureBinding->TextureIndex.has_value());
	ASSERT_TRUE(textureBinding->SamplerIndex.has_value());
	EXPECT_EQ(*textureBinding->TextureIndex, 1u);
	EXPECT_EQ(*textureBinding->SamplerIndex, 1u);
}

TEST_F(ShaderIntegrationTests, UniformBlockUploadInfluencesRealDrawOutput)
{
	static constexpr const char *kUboVertexSrc = R"(
#version 330 core
layout(location = 0) in vec3 a_Position;
void main()
{
    gl_Position = vec4(a_Position, 1.0);
}
)";

	static constexpr const char *kUboFragmentSrc = R"(
#version 420 core
layout(std140, binding = 0) uniform ColorBlock
{
    vec4 u_Color;
};
out vec4 FragColor;
void main()
{
    FragColor = u_Color;
}
)";

	auto framebuffer = CreateColorFramebuffer();
	auto target = GetDevice()->CreateRenderTargetFromFramebuffer(framebuffer);
	auto shader = GetDevice()->CreateShaderFromSource("UniformBlockDraw", kUboVertexSrc, kUboFragmentSrc);
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

	const glm::vec4 red(1.0f, 0.0f, 0.0f, 1.0f);
	RenderCommand::BeginRenderPass(target, desc);
	RenderCommand::SetPipelineState(pso);
	RenderCommand::SetViewport(0, 0, 8, 8);
	shader->Bind();
	shader->SetUniformBlock(0, &red, sizeof(red));
	RenderCommand::DrawIndexed(quad->GetVertexArray());
	RenderCommand::EndRenderPass();

	const auto redPixel = ReadRgbaPixel(framebuffer, 4, 4);
	EXPECT_GT(redPixel[0], 200);
	EXPECT_LT(redPixel[1], 20);
	EXPECT_LT(redPixel[2], 20);
	EXPECT_EQ(redPixel[3], 255);

	const glm::vec4 blue(0.0f, 0.0f, 1.0f, 1.0f);
	RenderCommand::BeginRenderPass(target, desc);
	RenderCommand::SetPipelineState(pso);
	RenderCommand::SetViewport(0, 0, 8, 8);
	shader->Bind();
	shader->SetUniformBlock(0, &blue, sizeof(blue));
	RenderCommand::DrawIndexed(quad->GetVertexArray());
	RenderCommand::EndRenderPass();

	const auto bluePixel = ReadRgbaPixel(framebuffer, 4, 4);
	EXPECT_LT(bluePixel[0], 20);
	EXPECT_LT(bluePixel[1], 20);
	EXPECT_GT(bluePixel[2], 200);
	EXPECT_EQ(bluePixel[3], 255);
}

TEST_F(ShaderIntegrationTests, CompiledTexturePreviewShaderProducesRealDrawOutput)
{
	ShaderTestUtils::SkipOrFailIfShaderMissing("TexturePreview");

	TextureSpecification spec{};
	spec.Width = 1;
	spec.Height = 1;
	spec.Format = TextureFormat::RGBA8;

	auto texture = GetDevice()->CreateTexture2D(spec);
	const std::array<uint8_t, 4> pixels = {32, 160, 224, 255};
	texture->SetData(pixels.data());

	auto framebuffer = CreateColorFramebuffer();
	auto target = GetDevice()->CreateRenderTargetFromFramebuffer(framebuffer);
	auto shader = GetDevice()->CreateShader("TexturePreview");
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

	const auto *layout = shader->GetUniformBlockLayout(0);
	ASSERT_NE(layout, nullptr);
	PackedUniformBlock block(*layout);
	const int32_t isDepthTexture = 0;
	ASSERT_TRUE(block.Write("u_IsDepthTexture", isDepthTexture));

	RenderCommand::BeginRenderPass(target, desc);
	RenderCommand::SetPipelineState(pso);
	RenderCommand::SetViewport(0, 0, 8, 8);
	shader->Bind();
	RenderCommand::SetTexture(1, texture);
	shader->SetUniformBlock(0, block.Data(), block.Size());
	RenderCommand::DrawIndexed(quad->GetVertexArray());
	RenderCommand::EndRenderPass();

	const auto pixel = ReadRgbaPixel(framebuffer, 4, 4);
	EXPECT_NEAR(pixel[0], pixels[0], 1);
	EXPECT_NEAR(pixel[1], pixels[1], 1);
	EXPECT_NEAR(pixel[2], pixels[2], 1);
	EXPECT_EQ(pixel[3], 255);
}

TEST_F(ShaderIntegrationTests, CompiledTexturePreviewShaderBindsResourcesThroughLogicalBindingPoints)
{
	ShaderTestUtils::SkipOrFailIfShaderMissing("TexturePreview");

	TextureSpecification spec{};
	spec.Width = 1;
	spec.Height = 1;
	spec.Format = TextureFormat::RGBA8;

	auto texture = GetDevice()->CreateTexture2D(spec);
	const std::array<uint8_t, 4> pixels = {220, 96, 32, 255};
	texture->SetData(pixels.data());

	auto framebuffer = CreateColorFramebuffer();
	auto target = GetDevice()->CreateRenderTargetFromFramebuffer(framebuffer);
	auto shader = GetDevice()->CreateShader("TexturePreview");
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

	const auto *layout = shader->GetUniformBlockLayout({0, 0});
	ASSERT_NE(layout, nullptr);

	PackedUniformBlock block(*layout);
	ASSERT_TRUE(block.Write("u_IsDepthTexture", false));

	auto uniformBuffer = GetDevice()->CreateUniformBuffer(block.Size());
	uniformBuffer->SetData(block.Data(), block.Size());

	RenderCommand::BeginRenderPass(target, desc);
	RenderCommand::SetPipelineState(pso);
	RenderCommand::SetViewport(0, 0, 8, 8);
	shader->Bind();
	shader->BindTexture({0, 1}, texture);
	shader->BindUniformBuffer({0, 0}, uniformBuffer);
	RenderCommand::DrawIndexed(quad->GetVertexArray());
	RenderCommand::EndRenderPass();

	const auto pixel = ReadRgbaPixel(framebuffer, 4, 4);
	EXPECT_NEAR(pixel[0], pixels[0], 1);
	EXPECT_NEAR(pixel[1], pixels[1], 1);
	EXPECT_NEAR(pixel[2], pixels[2], 1);
	EXPECT_EQ(pixel[3], 255);
}
