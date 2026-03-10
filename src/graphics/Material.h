#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>

#include "core/Base.h"
#include "graphics/Shader.h"
#include "graphics/Texture.h"

class Material
{
public:
    Material() = default;
    explicit Material(const Ref<Shader> &shader);

    ~Material() = default;

    Material(const Material &) = delete;
    Material &operator=(const Material &) = delete;

    Material(Material &&) noexcept = default;
    Material &operator=(Material &&) noexcept = default;

    void SetShader(const Ref<Shader> &shader);
    const Ref<Shader> &GetShader() const { return m_Shader; }

    void SetTexture(uint32_t slot, const Ref<Texture2D> &texture);
    Ref<Texture2D> GetTexture(uint32_t slot) const;

    void Bind() const;

private:
    Ref<Shader> m_Shader;
    std::unordered_map<uint32_t, Ref<Texture2D>> m_Textures;
};