#include "Material.h"

#include <cassert>

#include "core/Logger.h"

Material::Material(const Ref<Shader> &shader)
    : m_Shader(shader)
{
}

void Material::SetShader(const Ref<Shader> &shader)
{
    m_Shader = shader;
}

void Material::SetTexture(TextureSlot slot, const Ref<Texture2D> &texture)
{
    auto key = static_cast<uint32_t>(slot);
    if (texture)
        m_Textures[key] = texture;
    else
        m_Textures.erase(key);
}

Ref<Texture2D> Material::GetTexture(TextureSlot slot) const
{
    auto it = m_Textures.find(static_cast<uint32_t>(slot));
    if (it == m_Textures.end())
        return nullptr;
    return it->second;
}

void Material::Bind() const
{
    if (!m_Shader) LOG_ERROR("Material::Bind called with no shader");
    assert(m_Shader && "Material has no shader");

    m_Shader->Bind();

    for (const auto &[slot, texture] : m_Textures)
    {
        if (texture)
            texture->Bind(slot);
    }
}