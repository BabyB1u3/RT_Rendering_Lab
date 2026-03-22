#include "GLRenderCommand.h"

#include <glad/glad.h>

#include "graphics/interface/IIndexBuffer.h"
#include "GLCast.h"
#include "GLVertexArray.h"

void GLRenderCommand::Init()
{
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_DEPTH_TEST);
}

void GLRenderCommand::SetClearColor(const glm::vec4 &color)
{
	glClearColor(color.r, color.g, color.b, color.a);
}

void GLRenderCommand::Clear(bool color, bool depth, bool stencil)
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

void GLRenderCommand::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
	glViewport(static_cast<GLint>(x), static_cast<GLint>(y),
			   static_cast<GLsizei>(width), static_cast<GLsizei>(height));
}

void GLRenderCommand::EnableDepthTest(bool enabled)
{
	if (enabled)
		glEnable(GL_DEPTH_TEST);
	else
		glDisable(GL_DEPTH_TEST);
}

void GLRenderCommand::EnableBlend(bool enabled)
{
	if (enabled)
		glEnable(GL_BLEND);
	else
		glDisable(GL_BLEND);
}

void GLRenderCommand::EnableCullFace(bool enabled)
{
	if (enabled)
		glEnable(GL_CULL_FACE);
	else
		glDisable(GL_CULL_FACE);
}

void GLRenderCommand::SetCullFace(bool front)
{
	glCullFace(front ? GL_FRONT : GL_BACK);
}

void GLRenderCommand::DrawIndexed(const Ref<IVertexArray> &vao, uint32_t indexCount)
{
	vao->Bind();
	uint32_t count = indexCount ? indexCount : vao->GetIndexBuffer()->GetCount();
	glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(count), GL_UNSIGNED_INT, nullptr);
}

void GLRenderCommand::DrawArrays(uint32_t mode, uint32_t first, uint32_t count)
{
	glDrawArrays(mode, static_cast<GLint>(first), static_cast<GLsizei>(count));
}
