#include "GameObject.h"

GameObject::GameObject(glm::vec3 pos, glm::vec3 size, Texture2D texture1, Texture2D texture2, glm::vec3 velocity)
    : Position(pos), Size(size), Velocity(velocity), Texture1(texture1), Texture2(texture2), IsSolid(false), Destroyed(false), Selected(false)
{

}
GameObject::GameObject(glm::vec3 pos, glm::vec3 size, glm::vec3 velocity)
    : Position(pos), Size(size), Velocity(velocity)
{

}

void GameObject::Draw(Renderer& renderer)
{
    renderer.DrawObject(this->Texture1, this->Texture2, this->Position);
}