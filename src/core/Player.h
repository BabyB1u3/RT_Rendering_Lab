#ifndef PLAYER_H
#define PLAYER_H

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "GameObject.h"
#include "Camera.h"
#include "Ray.h"
#include "Chunk.h"

class Player
{
public:
	Player(glm::vec3 spawnpoint);
	~Player();
	Camera* FirstPlayerCamera;
	GameObject* CollisionBox;
	glm::vec3 Position;
	Ray* Direction;
	void UpDate(glm::mat4 projection, glm::mat4 view);
	void Destroy(Chunk* chunk);
private:
};

#endif
