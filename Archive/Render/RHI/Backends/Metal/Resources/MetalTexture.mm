#include "Render/RHI/Backends/Metal/Resources/MetalTexture.h"

#include "Core/Diagnostics/Assert/Assert.h"

id<MTLTexture> GetMetalTextureFromTexture(Texture* texture)
{
    if (auto* metalTexture = dynamic_cast<MetalTexture*>(texture))
        return metalTexture->GetMetalTexture();
    if (auto* swapchainTexture = dynamic_cast<MetalSwapchainTexture*>(texture))
        return swapchainTexture->GetMetalTexture();

    RTRLAB_ASSERT_MSG(false, "Texture is not owned by the Metal backend.");
    return nil;
}

id<MTLTexture> GetMetalTextureFromView(TextureView* textureView)
{
    if (auto* metalTextureView = dynamic_cast<MetalTextureView*>(textureView))
        return metalTextureView->GetMetalTextureView();
    if (auto* swapchainImageView = dynamic_cast<MetalSwapchainImageView*>(textureView))
        return GetMetalTextureFromTexture(swapchainImageView->GetTexture());

    RTRLAB_ASSERT_MSG(false, "TextureView is not owned by the Metal backend.");
    return nil;
}

id<MTLTexture> ResolveMetalTextureForBinding(const TextureBinding& textureBinding)
{
    if (textureBinding.m_View != nullptr)
        return GetMetalTextureFromView(textureBinding.m_View);

    RTRLAB_ASSERT_MSG(textureBinding.m_Texture != nullptr, "Metal texture bindings require a texture or view.");
    return GetMetalTextureFromTexture(textureBinding.m_Texture);
}
