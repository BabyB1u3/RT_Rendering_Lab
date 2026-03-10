#pragma once

#include <vector>

#include "Scene/Light.h"
#include "Renderer/RenderItem.h"

class Camera;

struct SceneData
{
    // Non-owning pointer. SceneData does not manage camera lifetime.
    Camera *ActiveCamera = nullptr;
    DirectionalLight MainDirectionalLight;
    // TODO: Change to use vector<Ref<RenderItem>>
    std::vector<RenderItem> RenderItems;
};