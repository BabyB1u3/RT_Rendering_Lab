#include "GLRenderCommand.h"

#include <glad/glad.h>

#include "graphics/interfaces/IFramebuffer.h"
#include "graphics/interfaces/IIndexBuffer.h"
#include "graphics/interfaces/IRenderTarget.h"
#include "graphics/interfaces/ITexture2D.h"
#include "GLCast.h"
#include "GLVertexArray.h"

void GLRenderCommand::Init()
{
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_DEPTH_TEST);
}

void GLRenderCommand::BeginRenderPass(const Ref<IRenderTarget> &target, const RenderPassDescriptor &desc)
{
	// Bind the framebuffer (or default FBO 0 for back buffer)
	auto fb = target->GetFramebuffer();
	if (fb)
		fb->Bind();
	else
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Build clear mask from load actions.
	// OpenGL quirk: glClear is affected by write masks. If a previous pass
	// disabled depth/color writes via glDepthMask(GL_FALSE) or glColorMask,
	// the clear silently does nothing for that buffer. We must temporarily
	// enable writes before clearing, then let SetPipelineState restore them.
	GLbitfield clearMask = 0;

	if (desc.ColorLoadAction == LoadAction::Clear)
	{
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glClearColor(desc.ClearColor.r, desc.ClearColor.g, desc.ClearColor.b, desc.ClearColor.a);
		clearMask |= GL_COLOR_BUFFER_BIT;
	}

	if (desc.DepthLoadAction == LoadAction::Clear)
	{
		glDepthMask(GL_TRUE);
		glClearDepth(static_cast<double>(desc.ClearDepth));
		clearMask |= GL_DEPTH_BUFFER_BIT;
	}

	if (desc.StencilLoadAction == LoadAction::Clear)
	{
		glStencilMask(0xFF);
		glClearStencil(static_cast<GLint>(desc.ClearStencil));
		clearMask |= GL_STENCIL_BUFFER_BIT;
	}

	if (clearMask)
		glClear(clearMask);
}

void GLRenderCommand::EndRenderPass()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GLRenderCommand::SetPipelineState(const PipelineState &state)
{
	// Depth
	if (state.DepthTestEnabled)
		glEnable(GL_DEPTH_TEST);
	else
		glDisable(GL_DEPTH_TEST);

	glDepthMask(state.DepthWriteEnabled ? GL_TRUE : GL_FALSE);

	// Blend
	if (state.BlendEnabled)
		glEnable(GL_BLEND);
	else
		glDisable(GL_BLEND);

	// Cull face
	if (state.CullFaceEnabled)
	{
		glEnable(GL_CULL_FACE);
		glCullFace(state.CullFront ? GL_FRONT : GL_BACK);
	}
	else
	{
		glDisable(GL_CULL_FACE);
	}
}

void GLRenderCommand::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
	glViewport(static_cast<GLint>(x), static_cast<GLint>(y),
			   static_cast<GLsizei>(width), static_cast<GLsizei>(height));
}

void GLRenderCommand::SetTexture(uint32_t slot, const Ref<ITexture2D> &texture)
{
	if (texture)
		texture->Bind(slot);
	else
		glBindTextureUnit(slot, 0);
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
