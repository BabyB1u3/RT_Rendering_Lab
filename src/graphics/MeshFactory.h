#pragma once

#include <memory>

#include "Mesh.h"

class MeshFactory
{
public:
    static Ref<Mesh> CreateCube();
    static Ref<Mesh> CreatePlane();
    static Ref<Mesh> CreateFullscreenQuad();
};