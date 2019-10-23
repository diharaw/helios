#pragma once

#include <vector>
#include "geometry.h"

namespace lumen
{
class Scene;

enum BVHSplitMethod
{
    BVH_SPLIT_EQUAL_COUNTS,
    BVH_SPLIT_MIDDLE,
    BVH_SPLIT_SAH
};

struct BVHNodeBuild
{
    AABB          aabb;
    uint32_t      start = 0;
    uint32_t      end   = 0;
    BVHNodeBuild* left  = nullptr;
    BVHNodeBuild* right = nullptr;

    uint32_t num_triangles();
    bool     is_leaf();
    ~BVHNodeBuild();
};
struct BVHNodeLinear
{
    AABB     aabb;
    uint32_t start              = 0;
    uint32_t end                = 0;
    uint32_t right_child_offset = 0;

    uint32_t num_triangles();
};

class BVHBuilder
{
public:
    virtual BVHNodeBuild* build(Scene* scene, uint32_t& num_nodes) = 0;
};

class BVHBuilderRecursive : public BVHBuilder
{
public:
    BVHNodeBuild* build(Scene* scene, uint32_t& num_nodes) override;

private:
    virtual BVHNodeBuild* recursive_build(Scene* scene, uint32_t start, uint32_t end, uint32_t& num_nodes) = 0;

protected:
    uint32_t find_longest_axis(BVHNodeBuild* node);
    void     calculate_aabb(BVHNodeBuild* node, Scene* scene, uint32_t start, uint32_t end);
};

class BVHBuilderEqualCounts : public BVHBuilderRecursive
{
private:
    BVHNodeBuild* recursive_build(Scene* scene, uint32_t start, uint32_t end, uint32_t& num_nodes) override;
};

class BVHBuilderMiddle : public BVHBuilderRecursive
{
private:
    BVHNodeBuild* recursive_build(Scene* scene, uint32_t start, uint32_t end, uint32_t& num_nodes) override;
};

class BVHBuilderSAH : public BVHBuilderRecursive
{
private:
    BVHNodeBuild* recursive_build(Scene* scene, uint32_t start, uint32_t end, uint32_t& num_nodes) override;
};

class BVH
{
public:
    BVH(Scene* scene, BVHBuilder& builder);
    ~BVH();
    void trace(Ray& ray, RayResult& result, bool need_closest_hit);

private:
    void     flatten(BVHNodeBuild* node);
    uint32_t flatten_recursive(BVHNodeBuild* node, uint32_t& idx);

private:
    std::vector<BVHNodeLinear> m_flattened_bvh;
    Scene*                     m_scene;
};

} // namespace lumen
