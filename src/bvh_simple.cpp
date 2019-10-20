#include "bvh_simple.h"
#include "scene.h"

#include <algorithm>
#include <iostream>

namespace lumen
{
Scene* g_scene = nullptr;

BVHSimple::BVHSimple(Scene* scene)
{
    g_scene = scene;
    m_root = new BVHSimpleNode(scene, scene->m_triangles.data(), scene->m_triangles.size());
    g_scene = nullptr;

	flatten();
}

BVHSimple::~BVHSimple()
{
    if (m_root)
		delete m_root;
}

void BVHSimple::trace(Ray& ray, RayResult& result, bool need_closest_hit)
{
}

void BVHSimple::flatten()
{
}

bool compare_along_x_axis(glm::ivec4 a, glm::ivec4 b)
{
    float a_pos_x = (g_scene->m_vtx_positions[a.x].x + g_scene->m_vtx_positions[a.y].x + g_scene->m_vtx_positions[a.z].x) / 3.0f;
    float b_pos_x = (g_scene->m_vtx_positions[b.x].x + g_scene->m_vtx_positions[b.y].x + g_scene->m_vtx_positions[b.z].x) / 3.0f;

	return a_pos_x < b_pos_x;
}

bool compare_along_y_axis(glm::ivec4 a, glm::ivec4 b)
{
    float a_pos_y = (g_scene->m_vtx_positions[a.x].y + g_scene->m_vtx_positions[a.y].y + g_scene->m_vtx_positions[a.z].y) / 3.0f;
    float b_pos_y = (g_scene->m_vtx_positions[b.x].y + g_scene->m_vtx_positions[b.y].y + g_scene->m_vtx_positions[b.z].y) / 3.0f;

    return a_pos_y < b_pos_y;
}

bool compare_along_z_axis(glm::ivec4 a, glm::ivec4 b)
{
    float a_pos_z = (g_scene->m_vtx_positions[a.x].z + g_scene->m_vtx_positions[a.y].z + g_scene->m_vtx_positions[a.z].z) / 3.0f;
    float b_pos_z = (g_scene->m_vtx_positions[b.x].z + g_scene->m_vtx_positions[b.y].z + g_scene->m_vtx_positions[b.z].z) / 3.0f;

    return a_pos_z < b_pos_z;
}

BVHSimpleNode::BVHSimpleNode(Scene* scene, glm::ivec4* triangles, uint32_t n)
{
    calculate_aabb(scene, triangles, n);

	int sort_axis = pick_sort_axis();

	if (sort_axis == 0)
		std::sort(triangles, triangles + n, compare_along_x_axis);
    else if (sort_axis == 1)
        std::sort(triangles, triangles + n, compare_along_y_axis);
    else
        std::sort(triangles, triangles + n, compare_along_z_axis);

    if (n < BVH_NODE_MAX_TRIANGLES)
    {
        m_triangles      = triangles;
        m_triangle_count = n;
    }
    else
    {
        uint32_t left_count = n / 2;
        uint32_t right_count = n - n / 2;

        m_left  = new BVHSimpleNode(scene, triangles, left_count);
        m_right = new BVHSimpleNode(scene, triangles + n / 2, right_count);
    }
}

BVHSimpleNode::~BVHSimpleNode()
{
    if (m_left)
        delete m_left;

	if (m_right)
        delete m_right;
}

bool BVHSimpleNode::is_leaf()
{
    return !m_left && !m_right;
}

uint32_t BVHSimpleNode::pick_sort_axis()
{
    float x_len = m_max.x - m_min.x;
    float y_len = m_max.y - m_min.y;
    float z_len = m_max.z - m_min.z;

    if (x_len > y_len && x_len > z_len)
        return 0;
    else if (y_len > x_len && y_len > z_len)
        return 1;
    else
        return 2;
}

void BVHSimpleNode::calculate_aabb(Scene* scene, glm::ivec4* triangles, uint32_t n)
{
    m_min = glm::vec3(FLT_MAX);
    m_max = glm::vec3(-FLT_MAX);
    glm::vec3* vertices = scene->m_vtx_positions.data();

	for (int i = 0; i < n; i++)
	{
		for (int tri = 0; tri < 3; tri++)
		{
            int idx = triangles[i][tri];

            if (vertices[idx].x < m_min.x)
                m_min.x = vertices[idx].x;
            if (vertices[idx].y < m_min.y)
                m_min.y = vertices[idx].y;
            if (vertices[idx].z < m_min.z)
                m_min.z = vertices[idx].z;

			if (vertices[idx].x > m_max.x)
                m_max.x = vertices[idx].x;
            if (vertices[idx].y > m_max.y)
                m_max.y = vertices[idx].y;
            if (vertices[idx].z > m_max.z)
                m_max.z = vertices[idx].z;
		}
	}
}
} // namespace lumen