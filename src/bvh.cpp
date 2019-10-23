#include "bvh.h"
#include "scene.h"

#include <algorithm>
#include <iostream>

namespace lumen
{
BVH::BVH(Scene* scene, BVHBuilder& builder)
{
    m_scene = scene;

    uint32_t num_nodes = 0;

    BVHNodeBuild* root = builder.build(scene, num_nodes);

    m_flattened_bvh.resize(num_nodes);

    flatten(root);

    if (root)
        delete root;
}

BVH::~BVH()
{
}

void BVH::trace(Ray& ray, RayResult& result, bool need_closest_hit)
{
    const int TMIN = 0;
    const int TMAX = 1;

    const glm::vec3* vtx_pos     = m_scene->m_vtx_positions.data();
    const glm::vec3* vtx_normals = m_scene->m_vtx_normals.data();

    uint32_t stack_size = 1;
    uint32_t index_stack[64];

    index_stack[0] = 0;

    while (stack_size != 0)
    {
        // Pop an index
        uint32_t idx = index_stack[--stack_size];

        BVHNodeLinear& node = m_flattened_bvh[idx];

        // Is it a leaf node?
        if (node.num_triangles() > 0)
        {
            for (uint32_t i = node.start; i < node.end; i++)
            {
                glm::ivec4 indices = m_scene->m_triangles[i];

                const glm::vec3& v0 = vtx_pos[indices.x];
                const glm::vec3& v1 = vtx_pos[indices.y];
                const glm::vec3& v2 = vtx_pos[indices.z];

                const glm::vec3& n0 = vtx_normals[indices.x];
                const glm::vec3& n1 = vtx_normals[indices.y];
                const glm::vec3& n2 = vtx_normals[indices.z];

                float u, v, t;

                if (intersect::ray_triangle(v0, v1, v2, ray, u, v, t))
                {
                    ray.tmax        = t;
                    result.t        = t;
                    result.id       = indices.w;
                    result.position = (1.0f - u - v) * v0 + u * v1 + v * v2;
                    result.normal   = glm::normalize((1.0f - u - v) * n0 + u * n1 + v * n2);

                    if (!need_closest_hit)
                        return;
                }
            }
        }
        else
        {
            uint32_t left_idx  = idx + 1;
            uint32_t right_idx = node.right_child_offset;

            BVHNodeLinear& left_node  = m_flattened_bvh[left_idx];
            BVHNodeLinear& right_node = m_flattened_bvh[right_idx];

            glm::vec2 left_span  = intersect::ray_box(left_node.aabb, ray);
            glm::vec2 right_span = intersect::ray_box(right_node.aabb, ray);

            bool left_intersect  = (left_span[TMIN] <= left_span[TMAX]) && (left_span[TMAX] >= ray.tmin) && (left_span[TMIN] <= ray.tmax);
            bool right_intersect = (right_span[TMIN] <= right_span[TMAX]) && (right_span[TMAX] >= ray.tmin) && (right_span[TMIN] <= ray.tmax);

            if (left_intersect && right_intersect)
            {
                if (left_span[TMIN] > right_span[TMIN])
                {
                    std::swap(left_span, right_span);
                    std::swap(left_idx, right_idx);
                }
            }

            if (left_intersect)
                index_stack[stack_size++] = left_idx;

            if (result.hit() && !need_closest_hit)
                return;

            if (right_intersect)
                index_stack[stack_size++] = right_idx;
        }
    }
}

void BVH::flatten(BVHNodeBuild* node)
{
    uint32_t start_idx = 0;
    flatten_recursive(node, start_idx);
}

uint32_t BVH::flatten_recursive(BVHNodeBuild* node, uint32_t& idx)
{
    BVHNodeLinear& linear_node = m_flattened_bvh[idx];

    linear_node.aabb = node->aabb;

    uint32_t current_offset = idx++;

    if (node->is_leaf())
    {
        linear_node.start = node->start;
        linear_node.end   = node->end;
    }
    else
    {
        flatten_recursive(node->left, idx);
        linear_node.right_child_offset = flatten_recursive(node->right, idx);
    }

    return current_offset;
}

BVHNodeBuild* BVHBuilderRecursive::build(Scene* scene, uint32_t& num_nodes)
{
    return recursive_build(scene, 0, scene->m_triangles.size(), num_nodes);
}

void BVHBuilderRecursive::calculate_aabb(BVHNodeBuild* node, Scene* scene, uint32_t start, uint32_t end)
{
    glm::vec3   min       = glm::vec3(FLT_MAX);
    glm::vec3   max       = glm::vec3(-FLT_MAX);
    glm::ivec4* triangles = scene->m_triangles.data();
    glm::vec3*  vertices  = scene->m_vtx_positions.data();

    for (int i = start; i < end; i++)
    {
        for (int tri = 0; tri < 3; tri++)
        {
            int idx = triangles[i][tri];

            if (vertices[idx].x < min.x)
                min.x = vertices[idx].x;
            if (vertices[idx].y < min.y)
                min.y = vertices[idx].y;
            if (vertices[idx].z < min.z)
                min.z = vertices[idx].z;

            if (vertices[idx].x > max.x)
                max.x = vertices[idx].x;
            if (vertices[idx].y > max.y)
                max.y = vertices[idx].y;
            if (vertices[idx].z > max.z)
                max.z = vertices[idx].z;
        }
    }

    node->aabb = AABB(min, max);
}

uint32_t BVHBuilderRecursive::find_longest_axis(BVHNodeBuild* node)
{
    float x_len = node->aabb.max().x - node->aabb.min().x;
    float y_len = node->aabb.max().y - node->aabb.min().y;
    float z_len = node->aabb.max().z - node->aabb.min().z;

    if (x_len > y_len && x_len > z_len)
        return 0;
    else if (y_len > x_len && y_len > z_len)
        return 1;
    else
        return 2;
}

BVHNodeBuild* BVHBuilderEqualCounts::recursive_build(Scene* scene, uint32_t start, uint32_t end, uint32_t& num_nodes)
{
    BVHNodeBuild* node = new BVHNodeBuild();

    num_nodes++;

    calculate_aabb(node, scene, start, end);

    int sort_axis     = find_longest_axis(node);
    int num_triangles = end - start;
    int mid           = (start + end) / 2;

    std::nth_element(&scene->m_triangles[start], &scene->m_triangles[mid], &scene->m_triangles[end - 1] + 1, [scene, sort_axis](const glm::ivec4& a, const glm::ivec4& b) {
        float a_pos = (scene->m_vtx_positions[a.x][sort_axis] + scene->m_vtx_positions[a.y][sort_axis] + scene->m_vtx_positions[a.z][sort_axis]) / 3.0f;
        float b_pos = (scene->m_vtx_positions[b.x][sort_axis] + scene->m_vtx_positions[b.y][sort_axis] + scene->m_vtx_positions[b.z][sort_axis]) / 3.0f;

        return a_pos < b_pos;
    });

    if (num_triangles == 1)
    {
        node->start = start;
        node->end   = end;
    }
    else
    {
        node->left  = recursive_build(scene, start, mid, num_nodes);
        node->right = recursive_build(scene, mid, end, num_nodes);
    }

    return node;
}

BVHNodeBuild* BVHBuilderMiddle::recursive_build(Scene* scene, uint32_t start, uint32_t end, uint32_t& num_nodes)
{
    return nullptr;
}

BVHNodeBuild* BVHBuilderSAH::recursive_build(Scene* scene, uint32_t start, uint32_t end, uint32_t& num_nodes)
{
    return nullptr;
}

uint32_t BVHNodeBuild::num_triangles()
{
    return end - start;
}

bool BVHNodeBuild::is_leaf()
{
    return !left && !right;
}

BVHNodeBuild::~BVHNodeBuild()
{
    if (left)
        delete left;

    if (right)
        delete right;
}

uint32_t BVHNodeLinear::num_triangles()
{
    return end - start;
}

} // namespace lumen