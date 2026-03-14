#pragma once

#include <vector>

#include "scene/Light.h"
#include "renderer/RenderItem.h"

struct SceneData
{
    DirectionalLight MainDirectionalLight;
    // TODO: Change to use vector<Ref<RenderItem>>
    std::vector<RenderItem> RenderItems;
};