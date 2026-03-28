#pragma once

/// @file ShaderBinding.h
/// @brief Shared logical shader resource binding types.

#include <cstddef>
#include <cstdint>
#include <functional>

enum class ShaderResourceKind : uint8_t
{
    UniformBuffer = 0,
    Texture2D,
    Sampler,
    CombinedTextureSampler,
    StorageBuffer,
};

struct ShaderBindingPoint
{
    uint32_t Set = 0;
    uint32_t Binding = 0;

    constexpr bool operator==(const ShaderBindingPoint &) const = default;
};

struct ShaderBindingPointHash
{
    size_t operator()(const ShaderBindingPoint &binding) const noexcept
    {
        const size_t setHash = std::hash<uint32_t>{}(binding.Set);
        const size_t bindingHash = std::hash<uint32_t>{}(binding.Binding);
        return setHash ^ (bindingHash + 0x9e3779b9u + (setHash << 6) + (setHash >> 2));
    }
};

constexpr ShaderBindingPoint MakeShaderBindingPoint(uint32_t set, uint32_t binding)
{
    return {set, binding};
}

/// Bridge helper for the current compatibility rule: flat slot N maps to {0, N}.
constexpr ShaderBindingPoint MakeFlatShaderBindingPoint(uint32_t binding)
{
    return {0u, binding};
}

namespace ShaderBindingSets
{
    inline constexpr uint32_t FramePass = 0;
    inline constexpr uint32_t Material = 1;
    inline constexpr uint32_t Draw = 2;
}

namespace ShaderBindingPoints
{
    inline constexpr ShaderBindingPoint PerFrame{ShaderBindingSets::FramePass, 0};
    inline constexpr ShaderBindingPoint PerPass{ShaderBindingSets::FramePass, 1};
    inline constexpr ShaderBindingPoint PerMaterial{ShaderBindingSets::Material, 0};
    inline constexpr ShaderBindingPoint PerDraw{ShaderBindingSets::Draw, 0};
}
