#pragma once

#include "geometry.h"

#define BVH_NODE_MAX_TRIANGLES 10

namespace lumen
{
class Scene;
class BVHSimpleNode;

class BVHSimple
{
public:
    BVHSimple(Scene* scene);
    ~BVHSimple();
	void trace(Ray& ray, RayResult& result, bool need_closest_hit);

private:
	void flatten();

private:
    Scene* m_scene;
    BVHSimpleNode* m_root;
};

class BVHSimpleNode
{
public:
    BVHSimpleNode(Scene* scene, glm::ivec4* triangles, uint32_t n);
    ~BVHSimpleNode();

	bool is_leaf();

private:
    uint32_t pick_sort_axis();
    void     calculate_aabb(Scene* scene, glm::ivec4* triangles, uint32_t n);

private:
    glm::vec3 m_min;
    glm::vec3 m_max;
    glm::ivec4* m_triangles;
    uint32_t   m_triangle_count = 0;
    BVHSimpleNode* m_left = nullptr;
    BVHSimpleNode* m_right = nullptr;
};
}
