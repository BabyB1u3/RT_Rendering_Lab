#include "Material.h"

#include <cassert>

Material::Material(const Ref<Shader> &shader)
    : m_Shader(shader)
{
}

void Material::SetShader(const Ref<Shader> &shader)
{
    m_Shader = shader;
}

void Material::SetTexture(uint32_t slot, const Ref<Texture2D> &texture)
{
    if (texture)
        m_Textures[slot] = texture;
    else
        m_Textures.erase(slot);
}

Ref<Texture2D> Material::GetTexture(uint32_t slot) const
{
    auto it = m_Textures.find(slot);
    if (it == m_Textures.end())
        return nullptr;
    return it->second;
}

void Material::Bind() const
{
    assert(m_Shader && "Material has no shader");

    m_Shader->Bind();

    for (const auto &[slot, texture] : m_Textures)
    {
        if (texture)
            texture->Bind(slot);
    }
}