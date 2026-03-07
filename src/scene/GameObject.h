#ifndef GAMEOBJECT_H
#define GAMEOBJECT_H

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "Texture.h"
#include "Renderer.h"


// Container object for holding all state relevant for a single
// game object entity. Each object in the game likely needs the
// minimal of state as described within GameObject.
class GameObject
{
public:
    // Object state
    glm::vec3   Position, Velocity, Size;
    GLboolean   IsSolid;
    GLboolean   Destroyed;
    GLboolean   Selected;
    // Render state
    Texture2D   Texture1;
    Texture2D   Texture2;
    // Constructor(s)
    GameObject(glm::vec3 pos, glm::vec3 size, Texture2D texture1, Texture2D texture2, glm::vec3 velocity = glm::vec3(0.0f, 0.0f, 0.0f));
    GameObject(glm::vec3 pos, glm::vec3 size, glm::vec3 velocity = glm::vec3(0.0f, 0.0f, 0.0f));
    // Draw sprite
    virtual void Draw(Renderer& renderer);
};

#endif

