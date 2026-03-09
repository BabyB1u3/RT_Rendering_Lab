#include "Player.h"


Player::Player(glm::vec3 spawnpoint)
{
	Position = spawnpoint;
	FirstPlayerCamera = new Camera();
	CollisionBox = new GameObject(glm::vec3(spawnpoint.x, spawnpoint.y - 1.0f, spawnpoint.z), glm::vec3(1.0f, 2.0f, 1.0f));
	Direction = new Ray();
}

Player::~Player()
{
	delete FirstPlayerCamera;
	delete CollisionBox;
	delete Direction;
}

void Player::UpDate(glm::mat4 projection, glm::mat4 view)
{
	CollisionBox->Position = glm::vec3(Position.x, Position.y - 1.0f, Position.z);
	Direction->Update(this->Position, glm::vec3(0.0f), projection, view);
}

void Player::Destroy(Chunk* chunk)
{
	for (GameObject& cube : chunk->Cubes)
	{
		if (this->Direction->Intersect(this->Position, cube))
		{
			cube.Destroyed = true;
		}
	}
}