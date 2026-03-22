#pragma once

/// @file SceneData.h
/// @brief Container for all renderable content in a scene.
///
/// Each demo builds a SceneData with its lights and objects, then passes it
/// to SceneRenderer::Render(). The renderer does not own or modify the scene —
/// it only reads from it during the frame.

#include <vector>

#include "scene/Light.h"
#include "renderer/RenderItem.h"

struct SceneData
{
    DirectionalLight MainDirectionalLight;
    std::vector<RenderItem> RenderItems;
};