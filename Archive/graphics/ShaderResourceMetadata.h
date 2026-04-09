#pragma once

/// @file ShaderResourceMetadata.h
/// @brief Shared runtime metadata for logical shader resources and backend bindings.

#include <optional>
#include <string>
#include <unordered_map>

#include "graphics/ShaderBinding.h"

struct ShaderResourceLayout
{
    std::string Name;
    ShaderResourceKind Kind = ShaderResourceKind::UniformBuffer;
    ShaderBindingPoint LogicalBinding = {};
};

struct ShaderBackendBinding
{
    ShaderResourceKind Kind = ShaderResourceKind::UniformBuffer;
    ShaderBindingPoint LogicalBinding = {};

    std::optional<uint32_t> BufferIndex;
    std::optional<uint32_t> TextureIndex;
    std::optional<uint32_t> SamplerIndex;
};

using ShaderResourceLayoutMap =
    std::unordered_map<ShaderBindingPoint, ShaderResourceLayout, ShaderBindingPointHash>;

using ShaderBackendBindingMap =
    std::unordered_map<ShaderBindingPoint, ShaderBackendBinding, ShaderBindingPointHash>;
