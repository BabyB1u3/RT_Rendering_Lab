#include "RenderTarget.h"

#include <cassert>

#include <glad/glad.h>

#include "Framebuffer.h"
#include "Texture.h"

RenderTarget RenderTarget::BackBuffer(uint32_t width, uint32_t height)
{
    RenderTarget target;
    target.m_Type = Type::BackBuffer;
    target.m_Width = width;
    target.m_Height = height;
    return target;
}

RenderTarget RenderTarget::FromFramebuffer(const Ref<::Framebuffer>& framebuffer)
{
    assert(framebuffer && "RenderTarget::FromFramebuffer requires a valid framebuffer");

    RenderTarget target;
    target.m_Type = Type::Framebuffer;
    target.m_Framebuffer = framebuffer;
    return target;
}

void RenderTarget::Bind() const
{
    if (m_Type == Type::BackBuffer)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    else
    {
        assert(m_Framebuffer && "Framebuffer target is null");
        m_Framebuffer->Bind();
    }
}

void RenderTarget::Unbind() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderTarget::Resize(uint32_t width, uint32_t height)
{
    if (m_Type == Type::BackBuffer)
    {
        m_Width  = width;
        m_Height = height;
    }
    else
    {
        assert(m_Framebuffer && "Framebuffer target is null");
        m_Framebuffer->Resize(width, height);
    }
}

uint32_t RenderTarget::GetWidth() const
{
    if (m_Type == Type::Framebuffer)
    {
        assert(m_Framebuffer && "Framebuffer target is null");
        return m_Framebuffer->GetSpecification().Width;
    }
    return m_Width;
}

uint32_t RenderTarget::GetHeight() const
{
    if (m_Type == Type::Framebuffer)
    {
        assert(m_Framebuffer && "Framebuffer target is null");
        return m_Framebuffer->GetSpecification().Height;
    }
    return m_Height;
}

Ref<Texture2D> RenderTarget::GetColorAttachment(uint32_t index) const
{
    if (m_Type == Type::Framebuffer)
    {
        assert(m_Framebuffer && "Framebuffer target is null");
        return m_Framebuffer->GetColorAttachment(index);
    }
    return nullptr;
}

Ref<Texture2D> RenderTarget::GetDepthAttachment() const
{
    if (m_Type == Type::Framebuffer)
    {
        assert(m_Framebuffer && "Framebuffer target is null");
        return m_Framebuffer->GetDepthAttachment();
    }
    return nullptr;
}
