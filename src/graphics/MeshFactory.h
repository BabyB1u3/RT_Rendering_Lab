#pragma once

#include <memory>

#include "Mesh.h"

class MeshFactory
{
public:
    static std::shared_ptr<Mesh> CreateCube();
    static std::shared_ptr<Mesh> CreatePlane();
    static std::shared_ptr<Mesh> CreateFullscreenQuad();
};