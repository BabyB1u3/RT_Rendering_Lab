#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>

#include "Shader.h"
#include "Texture.h"

class Material
{
public:
    Material() = default;
    explicit Material(const std::shared_ptr<Shader> &shader);

    ~Material() = default;

    Material(const Material &) = delete;
    Material &operator=(const Material &) = delete;

    Material(Material &&) noexcept = default;
    Material &operator=(Material &&) noexcept = default;

    void SetShader(const std::shared_ptr<Shader> &shader);
    const std::shared_ptr<Shader> &GetShader() const { return m_Shader; }

    void SetTexture(uint32_t slot, const std::shared_ptr<Texture2D> &texture);
    std::shared_ptr<Texture2D> GetTexture(uint32_t slot) const;

    void Bind() const;

private:
    std::shared_ptr<Shader> m_Shader;
    std::unordered_map<uint32_t, std::shared_ptr<Texture2D>> m_Textures;
};