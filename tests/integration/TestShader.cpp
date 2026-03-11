#include <gtest/gtest.h>
#include <memory>
#include <glm/glm.hpp>

#include "GlTestContext.h"
#include "graphics/Shader.h"

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
	auto shader = Shader::CreateFromSource("TestShader", kVertexSrc, kFragmentSrc);

	ASSERT_NE(shader, nullptr);
	EXPECT_NE(shader->GetRendererID(), 0u);
	EXPECT_EQ(shader->GetName(), "TestShader");
}

TEST_F(ShaderIntegrationTests, BindAndUniformSettersDoNotCrash)
{
	auto shader = Shader::CreateFromSource("TestShader", kVertexSrc, kFragmentSrc);

	ASSERT_NE(shader, nullptr);

	shader->Bind();
	shader->SetFloat4("u_Color", glm::vec4(1, 0, 0, 1));
	shader->SetMat4("u_MVP", glm::mat4(1.0f));
	shader->Unbind();

	SUCCEED();
}