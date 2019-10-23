#pragma once

#include <vector>
#include "geometry.h"

#define BVH_NODE_MAX_TRIANGLES 10

namespace lumen
{
class Scene;

enum BVHSplitMethod
{
    BVH_SPLIT_EQUAL_COUNTS,
    BVH_SPLIT_MIDDLE,
    BVH_SPLIT_SAH
};

struct RecursiveBVHNode
{
    AABB              aabb;
    glm::ivec4*       triangles;
    uint32_t          triangle_count = 0;
    RecursiveBVHNode* left           = nullptr;
    RecursiveBVHNode* right          = nullptr;

    bool is_leaf();
    ~RecursiveBVHNode();
};
struct LinearBVHNode
{
    AABB        aabb;
    glm::ivec4* triangles          = nullptr;
    uint32_t    triangle_count     = 0;
    uint32_t    right_child_offset = 0;
};

class BVHBuilder
{
public:
    virtual RecursiveBVHNode* build(Scene* scene, uint32_t& num_nodes) = 0;
};

class BVH
{
public:
    BVH(Scene* scene, BVHBuilder& builder);
    ~BVH();
    void trace(Ray& ray, RayResult& result, bool need_closest_hit);

private:
    void     flatten(RecursiveBVHNode* node);
    uint32_t flatten_recursive(RecursiveBVHNode* node, uint32_t& idx);

private:
    std::vector<LinearBVHNode> m_flattened_bvh;
    Scene*                     m_scene;
};

class BVHBuilderEqualCounts : public BVHBuilder
{
public:
    RecursiveBVHNode* build(Scene* scene, uint32_t& num_nodes) override;

private:
    RecursiveBVHNode* recursive_build(Scene* scene, uint32_t start, uint32_t end, uint32_t& num_nodes);
    void              calculate_aabb(RecursiveBVHNode* node, Scene* scene, uint32_t start, uint32_t end);
    uint32_t          find_longest_axis(RecursiveBVHNode* node);
};

} // namespace lumen
