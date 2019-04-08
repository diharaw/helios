#include "bvh.h"
#include "split_bvh_builder.h"
#include <set>

namespace lumen
{
BVH::BVH(Scene* scene, const Platform& platform, const BuildParams& params)
{
    assert(scene);
    m_scene    = scene;
    m_platform = platform;

    if (params.enable_prints)
        printf("BVH builder: %d tris, %d vertices\n", scene->getNumTriangles(), scene->getNumVertices());

    m_root = SplitBVHBuilder(*this, params).run();

    if (params.enable_prints)
        printf("BVH: Scene bounds: (%.1f,%.1f,%.1f) - (%.1f,%.1f,%.1f)\n", m_root->m_bounds.min().x, m_root->m_bounds.min().y, m_root->m_bounds.min().z, m_root->m_bounds.max().x, m_root->m_bounds.max().y, m_root->m_bounds.max().z);

    float sah = 0.f;
    m_root->compute_subtree_probabilities(m_platform, 1.f, sah);
    if (params.enable_prints)
        printf("top-down sah: %.2f\n", sah);

    if (params.stats)
    {
        params.stats->sah_cost         = sah;
        params.stats->branching_factor = 2;
        params.stats->num_leaf_nodes   = m_root->subtree_size(BVH_STAT_LEAF_COUNT);
        params.stats->num_inner_nodes  = m_root->subtree_size(BVH_STAT_INNER_COUNT);
        params.stats->num_tris         = m_root->subtree_size(BVH_STAT_TRIANGLE_COUNT);
        params.stats->num_child_nodes  = m_root->subtree_size(BVH_STAT_CHILDNODE_COUNT);
    }
}

static int32_t           current_treelet;
static std::set<int32_t> unique_treelets;

void BVH::trace(Ray& ray, RayResult& result, bool need_closest_hit, RayStats* stats) const
{
    trace_recursive(m_root, ray, result, need_closest_hit, stats);
}

void BVH::trace_recursive(BVHNode* node, Ray& ray, RayResult& result, bool need_closest_hit, RayStats* stats) const
{
    if (current_treelet != node->m_treelet)
    {
        if (stats)
        {
            //          if(!uniqueTreelets.contains(node->m_treelet))   // count unique treelets (comment this line to count all)
            stats->num_treelets++;
        }
        current_treelet = node->m_treelet;
    }

    if (node->is_leaf())
    {
        const LeafNode*  leaf        = reinterpret_cast<const LeafNode*>(node);
        const Vec3i*     triVtxIndex = (const Vec3i*)m_scene->getTriVtxIndexBuffer().getPtr();
        const glm::vec3* vtxPos      = (const glm::vec3*)m_scene->getVtxPosBuffer().getPtr();

        if (stats)
            stats->num_triangle_tests += m_platform.round_to_triangle_batch_size(leaf->num_triangles());

        for (int i = leaf->m_lo; i < leaf->m_hi; i++)
        {
            int32_t          index = m_tri_indices[i];
            const Vec3i&     ind   = triVtxIndex[index];
            const glm::vec3& v0    = vtxPos[ind.x];
            const glm::vec3& v1    = vtxPos[ind.y];
            const glm::vec3& v2    = vtxPos[ind.z];
            glm::vec3        bary  = Intersect::RayTriangle(v0, v1, v2, ray);
            float            t     = bary[2];

            if (t > ray.tmin && t < ray.tmax)
            {
                ray.tmax  = t;
                result.t  = t;
                result.id = index;

                if (!need_closest_hit)
                    return;
            }
        }
    }
    else
    {
        if (stats)
            stats->num_node_tests += m_platform.round_to_node_batch_size(node->num_child_nodes());

        const int        TMIN       = 0;
        const int        TMAX       = 1;
        const InnerNode* inner      = reinterpret_cast<const InnerNode*>(node);
        BVHNode*         child0     = inner->m_children[0];
        BVHNode*         child1     = inner->m_children[1];
        glm::vec2        tspan0     = Intersect::RayBox(child0->m_bounds, ray);
        glm::vec2        tspan1     = Intersect::RayBox(child1->m_bounds, ray);
        bool             intersect0 = (tspan0[TMIN] <= tspan0[TMAX]) && (tspan0[TMAX] >= ray.tmin) && (tspan0[TMIN] <= ray.tmax);
        bool             intersect1 = (tspan1[TMIN] <= tspan1[TMAX]) && (tspan1[TMAX] >= ray.tmin) && (tspan1[TMIN] <= ray.tmax);

        if (intersect0 && intersect1)
            if (tspan0[TMIN] > tspan1[TMIN])
            {
                std::swap(tspan0, tspan1);
                std::swap(child0, child1);
            }

        if (intersect0)
            trace_recursive(child0, ray, result, need_closest_hit, stats);

        if (result.hit() && !need_closest_hit)
            return;

        //      if(tspan1[TMIN] <= ray.tmax)    // this test helps only about 1-2%
        if (intersect1)
            trace_recursive(child1, ray, result, need_closest_hit, stats);
    }
}
} // namespace lumen