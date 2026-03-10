#pragma once

#include <vector>

#include "scene/Light.h"
#include "renderer/RenderItem.h"

class Camera;

struct SceneData
{
    // Non-owning pointer. SceneData does not manage camera lifetime.
    Camera *ActiveCamera = nullptr;
    DirectionalLight MainDirectionalLight;
    // TODO: Change to use vector<Ref<RenderItem>>
    std::vector<RenderItem> RenderItems;
};