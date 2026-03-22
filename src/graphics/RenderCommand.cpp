#include "RenderCommand.h"

#include "graphics/GraphicsDevice.h"
#include "graphics/interface/IRenderCommand.h"

void RenderCommand::Init()
{
	GetDevice()->GetRenderCommand()->Init();
}

void RenderCommand::SetClearColor(const glm::vec4 &color)
{
	GetDevice()->GetRenderCommand()->SetClearColor(color);
}

void RenderCommand::Clear(bool color, bool depth, bool stencil)
{
	GetDevice()->GetRenderCommand()->Clear(color, depth, stencil);
}

void RenderCommand::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
	GetDevice()->GetRenderCommand()->SetViewport(x, y, width, height);
}

void RenderCommand::EnableDepthTest(bool enabled)
{
	GetDevice()->GetRenderCommand()->EnableDepthTest(enabled);
}

void RenderCommand::EnableBlend(bool enabled)
{
	GetDevice()->GetRenderCommand()->EnableBlend(enabled);
}

void RenderCommand::EnableCullFace(bool enabled)
{
	GetDevice()->GetRenderCommand()->EnableCullFace(enabled);
}

void RenderCommand::SetCullFace(bool front)
{
	GetDevice()->GetRenderCommand()->SetCullFace(front);
}

void RenderCommand::DrawIndexed(const Ref<IVertexArray> &vao, uint32_t indexCount)
{
	GetDevice()->GetRenderCommand()->DrawIndexed(vao, indexCount);
}

void RenderCommand::DrawArrays(uint32_t mode, uint32_t first, uint32_t count)
{
	GetDevice()->GetRenderCommand()->DrawArrays(mode, first, count);
}
