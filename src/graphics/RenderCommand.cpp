#include "RenderCommand.h"

#include "graphics/GraphicsDevice.h"
#include "graphics/interface/IRenderCommand.h"

void RenderCommand::Init()
{
	GetDevice()->GetRenderCommand()->Init();
}

void RenderCommand::BeginFrame()
{
	GetDevice()->GetRenderCommand()->BeginFrame();
}

void RenderCommand::EndFrame()
{
	GetDevice()->GetRenderCommand()->EndFrame();
}

void RenderCommand::BeginRenderPass(const Ref<IRenderTarget> &target, const RenderPassDescriptor &desc)
{
	GetDevice()->GetRenderCommand()->BeginRenderPass(target, desc);
}

void RenderCommand::EndRenderPass()
{
	GetDevice()->GetRenderCommand()->EndRenderPass();
}

void RenderCommand::SetPipelineState(const PipelineState &state)
{
	GetDevice()->GetRenderCommand()->SetPipelineState(state);
}

void RenderCommand::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
	GetDevice()->GetRenderCommand()->SetViewport(x, y, width, height);
}

void RenderCommand::SetTexture(uint32_t slot, const Ref<ITexture2D> &texture)
{
	GetDevice()->GetRenderCommand()->SetTexture(slot, texture);
}

void RenderCommand::DrawIndexed(const Ref<IVertexArray> &vao, uint32_t indexCount)
{
	GetDevice()->GetRenderCommand()->DrawIndexed(vao, indexCount);
}

void RenderCommand::DrawArrays(uint32_t mode, uint32_t first, uint32_t count)
{
	GetDevice()->GetRenderCommand()->DrawArrays(mode, first, count);
}
