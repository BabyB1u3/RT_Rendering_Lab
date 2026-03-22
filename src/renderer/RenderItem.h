#pragma once

/// @file RenderItem.h
/// @brief A single drawable object: mesh + material + transform.
///
/// RenderItem is the input unit for all render passes. SceneData holds a
/// vector of these. Each pass iterates over them, uploading the per-object
/// model matrix and material properties before issuing a draw call.

#include <memory>

#include "core/Base.h"
#include "scene/Transform.h"

class Mesh;
class Material;

/// One drawable object in the scene — what to draw, how it looks, and where it is.
struct RenderItem
{
    Ref<Mesh> Mesh;
    Ref<Material> Material;
    Transform Transform;
};