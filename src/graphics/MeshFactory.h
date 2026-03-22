#pragma once

#include <memory>

#include "Mesh.h"

class MeshFactory
{
public:
    static Ref<Mesh> CreateCube();
    static Ref<Mesh> CreatePlane();
    static Ref<Mesh> CreateFullscreenQuad();
    static Ref<Mesh> CreateSphere(uint32_t stacks = 16, uint32_t slices = 32);
};