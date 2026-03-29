#include "MeshFactory.h"

#include <cmath>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

namespace
{
    struct PrimitiveVertex
    {
        float Position[3];
        float Normal[3];
        float TexCoord[2];
    };

    BufferLayout GetPrimitiveLayout()
    {
        return {
            {ShaderDataType::Float3, "a_Position"},
            {ShaderDataType::Float3, "a_Normal"},
            {ShaderDataType::Float2, "a_TexCoord"}};
    }
}

Ref<Mesh> MeshFactory::CreatePlane()
{
    static const PrimitiveVertex vertices[] =
        {
            {{-0.5f, 0.0f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
            {{0.5f, 0.0f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
            {{0.5f, 0.0f, 0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
            {{-0.5f, 0.0f, 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}}};

    static const uint32_t indices[] =
        {
            0, 3, 2,
            2, 1, 0};

    return CreateRef<Mesh>(
        vertices,
        static_cast<uint32_t>(sizeof(vertices)),
        GetPrimitiveLayout(),
        indices,
        static_cast<uint32_t>(sizeof(indices) / sizeof(uint32_t)));
}

Ref<Mesh> MeshFactory::CreateFullscreenQuad()
{
    static const PrimitiveVertex vertices[] =
        {
            {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
            {{1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
            {{1.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
            {{-1.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}};

    static const uint32_t indices[] =
        {
            0, 1, 2,
            2, 3, 0};

    return CreateRef<Mesh>(
        vertices,
        static_cast<uint32_t>(sizeof(vertices)),
        GetPrimitiveLayout(),
        indices,
        static_cast<uint32_t>(sizeof(indices) / sizeof(uint32_t)));
}

Ref<Mesh> MeshFactory::CreateCube()
{
    static const PrimitiveVertex vertices[] =
        {
            // Front
            {{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
            {{0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
            {{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
            {{-0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},

            // Back
            {{0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},
            {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}},
            {{-0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}},
            {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}},

            // Left
            {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
            {{-0.5f, -0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
            {{-0.5f, 0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
            {{-0.5f, 0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},

            // Right
            {{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
            {{0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
            {{0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
            {{0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},

            // Bottom
            {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
            {{0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}},
            {{0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
            {{-0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},

            // Top
            {{-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
            {{0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
            {{0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
            {{-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}}};

    static const uint32_t indices[] =
        {
            0, 1, 2, 2, 3, 0,       // Front
            4, 5, 6, 6, 7, 4,       // Back
            8, 9, 10, 10, 11, 8,    // Left
            12, 13, 14, 14, 15, 12, // Right
            16, 17, 18, 18, 19, 16, // Bottom
            20, 21, 22, 22, 23, 20  // Top
        };

    return CreateRef<Mesh>(
        vertices,
        static_cast<uint32_t>(sizeof(vertices)),
        GetPrimitiveLayout(),
        indices,
        static_cast<uint32_t>(sizeof(indices) / sizeof(uint32_t)));
}

Ref<Mesh> MeshFactory::CreateSphere(uint32_t stacks, uint32_t slices)
{
    // Generate a UV sphere by sweeping theta (pole to pole, 0..PI) and phi (around equator, 0..2PI).
    // Vertex grid is (stacks+1) x (slices+1); degenerate triangles at the poles are skipped.
    std::vector<PrimitiveVertex> vertices;
    std::vector<uint32_t> indices;

    for (uint32_t i = 0; i <= stacks; ++i)
    {
        float theta = static_cast<float>(i) * glm::pi<float>() / static_cast<float>(stacks);
        float sinTheta = std::sin(theta);
        float cosTheta = std::cos(theta);

        for (uint32_t j = 0; j <= slices; ++j)
        {
            float phi = static_cast<float>(j) * 2.0f * glm::pi<float>() / static_cast<float>(slices);
            float sinPhi = std::sin(phi);
            float cosPhi = std::cos(phi);

            float x = sinTheta * cosPhi;
            float y = cosTheta;
            float z = sinTheta * sinPhi;

            float u = static_cast<float>(j) / static_cast<float>(slices);
            float v = static_cast<float>(i) / static_cast<float>(stacks);

            PrimitiveVertex vert{};
            vert.Position[0] = x * 0.5f;
            vert.Position[1] = y * 0.5f;
            vert.Position[2] = z * 0.5f;
            vert.Normal[0] = x;
            vert.Normal[1] = y;
            vert.Normal[2] = z;
            vert.TexCoord[0] = u;
            vert.TexCoord[1] = v;

            vertices.push_back(vert);
        }
    }

    for (uint32_t i = 0; i < stacks; ++i)
    {
        for (uint32_t j = 0; j < slices; ++j)
        {
            uint32_t current = i * (slices + 1) + j;
            uint32_t next = current + slices + 1;

            // Skip the top-cap degenerate triangle at the north pole (i == 0).
            if (i != 0)
            {
                indices.push_back(current);
                indices.push_back(next);
                indices.push_back(current + 1);
            }

            // Skip the bottom-cap degenerate triangle at the south pole (i == stacks-1).
            if (i != stacks - 1)
            {
                indices.push_back(current + 1);
                indices.push_back(next);
                indices.push_back(next + 1);
            }
        }
    }

    return CreateRef<Mesh>(
        vertices.data(),
        static_cast<uint32_t>(vertices.size() * sizeof(PrimitiveVertex)),
        GetPrimitiveLayout(),
        indices.data(),
        static_cast<uint32_t>(indices.size()));
}