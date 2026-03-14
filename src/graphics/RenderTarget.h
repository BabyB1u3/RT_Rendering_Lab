#pragma once

#include <cstdint>

#include "core/Base.h"

class Framebuffer;
class Texture2D;

class RenderTarget
{
public:
    enum class Type
    {
        BackBuffer,
        Framebuffer
    };

public:
    RenderTarget() = default;
    ~RenderTarget() = default;

    static RenderTarget BackBuffer(uint32_t width, uint32_t height);
    static RenderTarget FromFramebuffer(const Ref<::Framebuffer>& framebuffer);

    void Bind() const;
    void Unbind() const;
    void Resize(uint32_t width, uint32_t height);

    uint32_t GetWidth() const;
    uint32_t GetHeight() const;

    bool IsBackBuffer() const { return m_Type == Type::BackBuffer; }
    bool IsFramebuffer() const { return m_Type == Type::Framebuffer; }

    const Ref<::Framebuffer>& GetFramebuffer() const { return m_Framebuffer; }

    Ref<Texture2D> GetColorAttachment(uint32_t index = 0) const;
    Ref<Texture2D> GetDepthAttachment() const;

private:
    Type m_Type = Type::BackBuffer;
    Ref<::Framebuffer> m_Framebuffer;

    // Only used by BackBuffer type. Framebuffer type reads dimensions from its spec.
    uint32_t m_Width = 0;
    uint32_t m_Height = 0;
};
