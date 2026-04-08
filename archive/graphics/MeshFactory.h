#pragma once

/// @file MeshFactory.h
/// @brief Procedural mesh generators for common primitives.
///
/// All primitives share the same vertex layout: Position (vec3), Normal (vec3), TexCoord (vec2).
/// Meshes are centered at the origin with unit-ish dimensions (±0.5 for cube/plane/sphere).
///
/// The fullscreen quad spans NDC [-1, 1] and is intended for post-process / texture preview passes.

#include <memory>

#include "Mesh.h"

class MeshFactory
{
public:
    /// Axis-aligned unit cube (side length 1, centered at origin). 24 vertices (unique normals per face).
    static Ref<Mesh> CreateCube();
    /// Horizontal unit plane at Y=0 (1x1, centered at origin). Faces upward (+Y normal).
    static Ref<Mesh> CreatePlane();
    /// NDC fullscreen quad ([-1,1] x [-1,1], Z=0). Used by TexturePreviewPass.
    static Ref<Mesh> CreateFullscreenQuad();
    /// UV sphere (radius 0.5, centered at origin). Normals point outward.
    /// @param stacks  Number of horizontal rings (latitude divisions).
    /// @param slices  Number of vertical segments (longitude divisions).
    static Ref<Mesh> CreateSphere(uint32_t stacks = 16, uint32_t slices = 32);
};