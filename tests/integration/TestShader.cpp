#include <gtest/gtest.h>
#include <memory>
#include <glm/glm.hpp>

#include "GlTestContext.h"
#include "core/FileSystem.h"
#include "graphics/GraphicsDevice.h"
#include "graphics/interface/IShader.h"

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

	inline static Scope<GlTestContext> s_Context;
};

static const char* kVertexSrc = R"(
#version 330 core
layout(location = 0) in vec3 a_Position;
uniform mat4 u_MVP;
void main()
{
    gl_Position = u_MVP * vec4(a_Position, 1.0);
}
)";

static const char* kFragmentSrc = R"(
#version 330 core
out vec4 FragColor;
uniform vec4 u_Color;
void main()
{
    FragColor = u_Color;
}
)";

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

TEST_F(ShaderIntegrationTests, CreateFromStem_LoadsForwardLitFromSPIRV)
{
	auto stem = FileSystem::GetShaderStem("ForwardLit");
	if (!FileSystem::Exists(stem.string() + ".vert.spv"))
	{
		GTEST_SKIP() << "SPIR-V artifacts not found (GLAB_COMPILE_SHADERS=OFF?)";
	}

	auto shader = GetDevice()->CreateShaderFromStem(stem, "ForwardLit");
	ASSERT_NE(shader, nullptr);
	EXPECT_EQ(shader->GetName(), "ForwardLit");

	shader->Bind();
	shader->SetFloat("u_LightIntensity", 1.0f);
	shader->SetMat4("u_ViewProjection", glm::mat4(1.0f));
	shader->SetFloat3("u_LightDirection", glm::vec3(0.0f, -1.0f, 0.0f));
	shader->Unbind();
}

TEST_F(ShaderIntegrationTests, CreateFromStem_LoadsShadowDepthFromSPIRV)
{
	auto stem = FileSystem::GetShaderStem("ShadowDepth");
	if (!FileSystem::Exists(stem.string() + ".vert.spv"))
	{
		GTEST_SKIP() << "SPIR-V artifacts not found";
	}

	auto shader = GetDevice()->CreateShaderFromStem(stem, "ShadowDepth");
	ASSERT_NE(shader, nullptr);

	shader->Bind();
	shader->SetMat4("u_LightViewProjection", glm::mat4(1.0f));
	shader->SetMat4("u_Model", glm::mat4(1.0f));
	shader->Unbind();
}

TEST_F(ShaderIntegrationTests, CreateFromStem_LoadsTexturePreviewFromSPIRV)
{
	auto stem = FileSystem::GetShaderStem("TexturePreview");
	if (!FileSystem::Exists(stem.string() + ".vert.spv"))
	{
		GTEST_SKIP() << "SPIR-V artifacts not found";
	}

	auto shader = GetDevice()->CreateShaderFromStem(stem, "TexturePreview");
	ASSERT_NE(shader, nullptr);

	shader->Bind();
	shader->SetBool("u_IsDepthTexture", false);
	shader->Unbind();
}
