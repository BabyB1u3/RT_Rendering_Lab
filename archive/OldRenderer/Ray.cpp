#include "Ray.h"

Ray::Ray()
{
}

Ray::~Ray()
{
}

void Ray::Update(glm::vec3 origin, glm::vec3 end, glm::mat4 projection, glm::mat4 view)
{
	glm::vec3 ray_ndc = end;

	glm::vec4 ray_clip = glm::vec4(ray_ndc, 1.0f);

	glm::vec4 ray_eye = inverse(projection) * ray_clip;

	glm::vec4 ray_world = inverse(view) * ray_eye;

	if (ray_world.w != 0.0)
	{
		ray_world.x /= ray_world.w;
		ray_world.y /= ray_world.w;
		ray_world.z /= ray_world.w;
	}

	glm::vec3 ray_dir = normalize(glm::vec3(ray_world) - origin);

	this->ray = ray_dir;
}

bool Ray::Intersect(glm::vec3& origin,  GameObject& aabb)
{
    glm::vec3 ptOnPlane; //射线与包围盒某面的交点
    glm::vec3 min = glm::vec3(aabb.Position.x - aabb.Size.x / 2, aabb.Position.y - aabb.Size.y / 2, aabb.Position.z - aabb.Size.z / 2); //aabb包围盒最小点坐标
    glm::vec3 max = glm::vec3(aabb.Position.x + aabb.Size.x / 2, aabb.Position.y + aabb.Size.y / 2, aabb.Position.z + aabb.Size.z / 2); //aabb包围盒最大点坐标

    const glm::vec3& dir = this->ray; //方向矢量

    float t;

    //分别判断射线与各面的相交情况

    //判断射线与包围盒x轴方向的面是否有交点
    if (dir.x != 0.f) //射线x轴方向分量不为0 若射线方向矢量的x轴分量为0，射线不可能经过包围盒朝x轴方向的两个面
    {
        /*
          使用射线与平面相交的公式求交点
         */
        if (dir.x > 0)//若射线沿x轴正方向偏移
            t = (min.x - origin.x) / dir.x;
        else  //射线沿x轴负方向偏移
            t = (max.x - origin.x) / dir.x;

        if (t > 0.f && t < 3.0f) //t>0时则射线与平面相交
        {
            ptOnPlane = origin + t * dir; //计算交点坐标
            //判断交点是否在当前面内
            if (min.y < ptOnPlane.y && ptOnPlane.y < max.y && min.z < ptOnPlane.z && ptOnPlane.z < max.z)
            {
                return true; //射线与包围盒有交点
            }
        }
    }

    //若射线沿y轴方向有分量 判断是否与包围盒y轴方向有交点
    if (dir.y != 0.f)
    {
        if (dir.y > 0)
            t = (min.y - origin.y) / dir.y;
        else
            t = (max.y - origin.y) / dir.y;

        if (t > 0.f && t < 3.0f)
        {
            ptOnPlane = origin + t * dir;

            if (min.z < ptOnPlane.z && ptOnPlane.z < max.z && min.x < ptOnPlane.x && ptOnPlane.x < max.x)
            {
                return true;
            }
        }
    }

    //若射线沿z轴方向有分量 判断是否与包围盒y轴方向有交点
    if (dir.z != 0.f)
    {
        if (dir.z > 0)
            t = (min.z - origin.z) / dir.z;
        else
            t = (max.z - origin.z) / dir.z;

        if (t > 0.f && t < 3.0f)
        {
            ptOnPlane = origin + t * dir;

            if (min.x < ptOnPlane.x && ptOnPlane.x < max.x && min.y < ptOnPlane.y && ptOnPlane.y < max.y)
            {
                return true;
            }
        }
    }

    return false;
}