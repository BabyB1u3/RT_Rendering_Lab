#include "RenderCommand.h"

#include <glad/glad.h>

void RenderCommand::Init()
{
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glEnable(GL_DEPTH_TEST);

#if 0
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
#endif
}

void RenderCommand::SetClearColor(const glm::vec4 &color)
{
	glClearColor(color.r, color.g, color.b, color.a);
}

void RenderCommand::Clear(bool color, bool depth, bool stencil)
{
	GLbitfield mask = 0;
	if (color)
		mask |= GL_COLOR_BUFFER_BIT;
	if (depth)
		mask |= GL_DEPTH_BUFFER_BIT;
	if (stencil)
		mask |= GL_STENCIL_BUFFER_BIT;

	glClear(mask);
}

void RenderCommand::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
	glViewport(static_cast<GLint>(x), static_cast<GLint>(y),
			   static_cast<GLsizei>(width), static_cast<GLsizei>(height));
}

void RenderCommand::EnableDepthTest(bool enabled)
{
	if (enabled)
		glEnable(GL_DEPTH_TEST);
	else
		glDisable(GL_DEPTH_TEST);
}

void RenderCommand::EnableBlend(bool enabled)
{
	if (enabled)
		glEnable(GL_BLEND);
	else
		glDisable(GL_BLEND);
}

void RenderCommand::EnableCullFace(bool enabled)
{
	if (enabled)
		glEnable(GL_CULL_FACE);
	else
		glDisable(GL_CULL_FACE);
}

void RenderCommand::DrawIndexed(const Ref<VertexArray> &vao, uint32_t indexCount)
{
	vao->Bind();
	uint32_t count = indexCount ? indexCount : vao->GetIndexBuffer()->GetCount();
	glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(count), GL_UNSIGNED_INT, nullptr);
}

void RenderCommand::DrawArrays(uint32_t mode, uint32_t first, uint32_t count)
{
	glDrawArrays(mode, static_cast<GLint>(first), static_cast<GLsizei>(count));
}