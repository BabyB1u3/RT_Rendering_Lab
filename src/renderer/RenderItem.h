#pragma once

#include <memory>

#include "core/Base.h"
#include "Scene/Transform.h"

class Mesh;
class Material;

struct RenderItem
{
    Ref<Mesh> Mesh;
    Ref<Material> Material;
    Transform Transform;
};