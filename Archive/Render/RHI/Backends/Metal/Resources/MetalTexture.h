#pragma once

/// @file MetalTexture.h
/// @brief Backend-private Metal texture/view wrappers, including swapchain
/// drawable wrappers.

#include "Render/RHI/Backends/Metal/Common/MetalCommon.h"
#include "Render/RHI/RHIResources.h"

class MetalSwapchainTexture final : public Texture
{
public:
    MetalSwapchainTexture(id<MTLTexture> texture, const TextureDesc& desc) : m_Texture(texture), m_Desc(desc) {}

    const TextureDesc& GetDesc() const override { return m_Desc; }
    id<MTLTexture> GetMetalTexture() const { return m_Texture; }

private:
    id<MTLTexture> m_Texture = nil;
    TextureDesc m_Desc;
};

class MetalSwapchainImageView final : public TextureView
{
public:
    MetalSwapchainImageView(Texture* texture, const TextureViewDesc& desc) : m_Texture(texture), m_Desc(desc) {}

    Texture* GetTexture() const override { return m_Texture; }
    const TextureViewDesc& GetDesc() const override { return m_Desc; }

private:
    Texture* m_Texture = nullptr;
    TextureViewDesc m_Desc;
};

class MetalTexture final : public Texture
{
public:
    MetalTexture(id<MTLTexture> texture, const TextureDesc& desc) : m_Texture([texture retain]), m_Desc(desc) {}

    ~MetalTexture() override
    {
        if (m_Texture != nil)
        {
            [m_Texture release];
            m_Texture = nil;
        }
    }

    const TextureDesc& GetDesc() const override { return m_Desc; }
    id<MTLTexture> GetMetalTexture() const { return m_Texture; }

private:
    id<MTLTexture> m_Texture = nil;
    TextureDesc m_Desc;
};

class MetalTextureView final : public TextureView
{
public:
    MetalTextureView(Texture* texture, id<MTLTexture> textureView, const TextureViewDesc& desc)
        : m_Texture(texture), m_TextureView([textureView retain]), m_Desc(desc)
    {
    }

    ~MetalTextureView() override
    {
        if (m_TextureView != nil)
        {
            [m_TextureView release];
            m_TextureView = nil;
        }
    }

    Texture* GetTexture() const override { return m_Texture; }
    const TextureViewDesc& GetDesc() const override { return m_Desc; }
    id<MTLTexture> GetMetalTextureView() const { return m_TextureView; }

private:
    Texture* m_Texture = nullptr;
    id<MTLTexture> m_TextureView = nil;
    TextureViewDesc m_Desc;
};

id<MTLTexture> GetMetalTextureFromTexture(Texture* texture);
id<MTLTexture> GetMetalTextureFromView(TextureView* textureView);
id<MTLTexture> ResolveMetalTextureForBinding(const TextureBinding& textureBinding);
