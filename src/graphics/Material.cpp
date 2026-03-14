#include "Material.h"

#include "Shader.h"

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

// --- Property setters ---

void Material::SetFloat(const std::string &name, float value) { m_Floats[name] = value; }
void Material::SetInt(const std::string &name, int value) { m_Ints[name] = value; }
void Material::SetVec3(const std::string &name, const glm::vec3 &value) { m_Vec3s[name] = value; }
void Material::SetVec4(const std::string &name, const glm::vec4 &value) { m_Vec4s[name] = value; }

// --- Property getters ---

float Material::GetFloat(const std::string &name, float defaultValue) const
{
    auto it = m_Floats.find(name);
    return it != m_Floats.end() ? it->second : defaultValue;
}

int Material::GetInt(const std::string &name, int defaultValue) const
{
    auto it = m_Ints.find(name);
    return it != m_Ints.end() ? it->second : defaultValue;
}

glm::vec3 Material::GetVec3(const std::string &name, const glm::vec3 &defaultValue) const
{
    auto it = m_Vec3s.find(name);
    return it != m_Vec3s.end() ? it->second : defaultValue;
}

glm::vec4 Material::GetVec4(const std::string &name, const glm::vec4 &defaultValue) const
{
    auto it = m_Vec4s.find(name);
    return it != m_Vec4s.end() ? it->second : defaultValue;
}

// --- Upload to shader ---

void Material::UploadToShader(const Ref<Shader> &shader) const
{
    // Upload scalar/vector properties
    for (const auto &[name, val] : m_Floats)
        shader->SetFloat(name, val);
    for (const auto &[name, val] : m_Ints)
        shader->SetInt(name, val);
    for (const auto &[name, val] : m_Vec3s)
        shader->SetFloat3(name, val);
    for (const auto &[name, val] : m_Vec4s)
        shader->SetFloat4(name, val);

    // Bind textures and set sampler uniforms
    auto albedo = GetTexture(TextureSlot::Albedo);
    if (albedo)
    {
        albedo->Bind(static_cast<uint32_t>(TextureSlot::Albedo));
        shader->SetInt("u_AlbedoMap", static_cast<int>(TextureSlot::Albedo));
        shader->SetBool("u_UseAlbedoMap", true);
    }
    else
    {
        shader->SetBool("u_UseAlbedoMap", false);
    }
}
