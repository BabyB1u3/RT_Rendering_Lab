#pragma once

/// @file Material.h
/// @brief Surface data container for the pass-centric (Model B) material system.
///
/// A Material holds textures and scalar/vector properties that describe a surface,
/// but does NOT own or reference a Shader. The RenderPass decides which shader to
/// use; the Material just uploads its data via UploadToShader().
///
/// TextureSlot is a type-safe enum mapping to GPU texture unit indices.
/// Slot values must match the sampler uniform indices in the GLSL shaders.
///
/// See docs/material_system_design.md for the full architecture and evolution plan.

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include <glm/glm.hpp>

#include "core/Base.h"
#include "Texture.h"

class IShader;
class ITexture2D;

/// Type-safe texture unit binding slots. Values correspond to GL texture unit indices.
enum class TextureSlot : uint32_t
{
    ShadowMap = 0,  // unit 0 — reserved for the shadow depth map
    Albedo    = 1,  // unit 1 — base color / diffuse map
};

/// Pure surface data container (textures + typed properties).
/// Does not own a shader — the pass controls which shader is bound.
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
    void SetTexture(TextureSlot slot, const Ref<ITexture2D> &texture);
    Ref<ITexture2D> GetTexture(TextureSlot slot) const;

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

    /// Upload all stored properties as uniforms and bind textures to the given shader.
    /// The shader must already be bound (or use DSA uniform calls internally).
    void UploadToShader(const Ref<IShader> &shader) const;

private:
    std::unordered_map<uint32_t, Ref<ITexture2D>> m_Textures;

    std::unordered_map<std::string, float>     m_Floats;
    std::unordered_map<std::string, int>       m_Ints;
    std::unordered_map<std::string, glm::vec3> m_Vec3s;
    std::unordered_map<std::string, glm::vec4> m_Vec4s;
};
