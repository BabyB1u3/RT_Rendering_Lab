#ifndef RAY_H
#define RAY_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include "GameObject.h"

class Ray
{
public:
	Ray();
	~Ray();
	void Update(glm::vec3 origin, glm::vec3 end, glm::mat4 projection, glm::mat4 view);
	bool Intersect(glm::vec3& origin, GameObject& aabb);
	glm::vec3 ray;
private:

};

#endif // !RAY_H

