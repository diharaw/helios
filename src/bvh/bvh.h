#pragma once

#include "../scene.h"
#include "../geometry.h"
#include "bvh_node.h"
#include "platform.h"
#include <algorithm>

namespace lumen
{
struct RayStats
{
    RayStats() { clear(); }
    void clear() { memset(this, 0, sizeof(RayStats)); }
    void print() const
    {
        if (num_rays > 0) printf("Ray stats: (%s) %d rays, %.1f tris/ray, %.1f nodes/ray (cost=%.2f) %.2f treelets/ray\n", platform.name().c_str(), num_rays, 1.0f * num_triangle_tests / num_rays, 1.0f * num_node_tests / num_rays, (platform.sah_triangle_cost() * num_triangle_tests / num_rays + platform.sah_node_cost() * num_node_tests / num_rays), 1.0f * num_treelets / num_rays);
    }

    int32_t  num_rays;
    int32_t  num_triangle_tests;
    int32_t  num_node_tests;
    int32_t  num_treelets;
    Platform platform; // set by whoever sets the stats
};

class BVH
{
public:
    struct Stats
    {
        Stats() { clear(); }
        void clear() { memset(this, 0, sizeof(Stats)); }
        void print() const
        {
            printf("Tree stats: [bfactor=%d] %d nodes (%d+%d), %.2f SAHCost, %.1f children/inner, %.1f tris/leaf\n", branching_factor, num_leaf_nodes + num_inner_nodes, num_leaf_nodes, num_inner_nodes, sah_cost, 1.0f * num_child_nodes / std::max(num_inner_nodes, 1), 1.0f * num_tris / std::max(num_leaf_nodes, 1));
        }

        float   sah_cost;
        int32_t branching_factor;
        int32_t num_inner_nodes;
        int32_t num_leaf_nodes;
        int32_t num_child_nodes;
        int32_t num_tris;
    };

    struct BuildParams
    {
        Stats* stats;
        bool   enable_prints;
        float  split_alpha; // spatial split area threshold

        BuildParams(void)
        {
            stats         = NULL;
            enable_prints = true;
            split_alpha   = 1.0e-5f;
        }

        /*uint32_t compute_hash(void) const
        {
            return hashBits(floatToBits(splitAlpha));
        }*/
    };

public:
    BVH(Scene* scene, const Platform& platform, const BuildParams& params);
    ~BVH(void)
    {
        if (m_root) m_root->delete_subtree();
    }

    Scene*          scene(void) const { return m_scene; }
    const Platform& platform(void) const { return m_platform; }
    BVHNode*        root(void) const { return m_root; }
    void            trace(Ray& ray, RayResult& result, bool need_closest_hit, RayStats* stats = NULL) const;

    std::vector<int32_t>&       tri_indices(void) { return m_tri_indices; }
    const std::vector<int32_t>& tri_indices(void) const { return m_tri_indices; }

private:
    void trace_recursive(BVHNode* node, Ray& ray, RayResult& result, bool need_closest_hit, RayStats* stats) const;

    Scene*   m_scene;
    Platform m_platform;

    BVHNode*             m_root;
    std::vector<int32_t> m_tri_indices;
};
} // namespace lumen