#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include <glm/glm.hpp>

#include "core/Base.h"
#include "Texture.h"

class Shader;

enum class TextureSlot : uint32_t
{
    ShadowMap = 0,
    Albedo    = 1,
};

class Material
{
public:
    Material() = default;
    ~Material() = default;

    Material(const Material &) = delete;
    Material &operator=(const Material &) = delete;
    Material(Material &&) noexcept = default;
    Material &operator=(Material &&) noexcept = default;

    // Texture bindings
    void SetTexture(TextureSlot slot, const Ref<Texture2D> &texture);
    Ref<Texture2D> GetTexture(TextureSlot slot) const;

    // Property setters
    void SetFloat(const std::string &name, float value);
    void SetInt(const std::string &name, int value);
    void SetVec3(const std::string &name, const glm::vec3 &value);
    void SetVec4(const std::string &name, const glm::vec4 &value);

    // Property getters (return defaultValue if not set)
    float GetFloat(const std::string &name, float defaultValue = 0.0f) const;
    int GetInt(const std::string &name, int defaultValue = 0) const;
    glm::vec3 GetVec3(const std::string &name, const glm::vec3 &defaultValue = glm::vec3(0.0f)) const;
    glm::vec4 GetVec4(const std::string &name, const glm::vec4 &defaultValue = glm::vec4(0.0f)) const;

    // Upload all properties + bind textures to an externally-provided shader
    void UploadToShader(const Ref<Shader> &shader) const;

private:
    std::unordered_map<uint32_t, Ref<Texture2D>> m_Textures;

    std::unordered_map<std::string, float>     m_Floats;
    std::unordered_map<std::string, int>       m_Ints;
    std::unordered_map<std::string, glm::vec3> m_Vec3s;
    std::unordered_map<std::string, glm::vec4> m_Vec4s;
};
