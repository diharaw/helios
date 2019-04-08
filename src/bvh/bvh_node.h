#pragma once

#include <iostream>
#include <stdint.h>
#include <assert.h>
#include "platform.h"
#include "../geometry.h"

namespace lumen
{
enum BVH_STAT
{
    BVH_STAT_NODE_COUNT,
    BVH_STAT_INNER_COUNT,
    BVH_STAT_LEAF_COUNT,
    BVH_STAT_TRIANGLE_COUNT,
    BVH_STAT_CHILDNODE_COUNT,
};

class BVHNode
{
public:
    BVHNode() :
        m_probability(1.0f), m_parent_probability(1.0f), m_treelet(-1), m_index(-1) {}
    virtual bool     is_leaf() const             = 0;
    virtual int32_t  num_child_nodes() const     = 0;
    virtual BVHNode* child_node(int32_t i) const = 0;
    virtual int32_t  num_triangles() const { return 0; }

    float area() const { return m_bounds.area(); }

    AABB m_bounds;

    // These are somewhat experimental, for some specific test and may be invalid...
    float m_probability;        // probability of coming here (widebvh uses this)
    float m_parent_probability; // probability of coming to parent (widebvh uses this)

    int m_treelet; // for queuing tests (qmachine uses this)
    int m_index;   // in linearized tree (qmachine uses this)

    // Subtree functions
    int   subtree_size(BVH_STAT stat = BVH_STAT_NODE_COUNT) const;
    void  compute_subtree_probabilities(const Platform& p, float parent_probability, float& sah);
    float compute_subtree_sah_cost(const Platform& p) const; // NOTE: assumes valid probabilities
    void  delete_subtree();

    void assign_indices_depth_first(int32_t index = 0, bool include_leaf_nodes = true);
    void assign_indices_breadth_first(int32_t index = 0, bool include_leaf_nodes = true);
};

class InnerNode : public BVHNode
{
public:
    InnerNode(const AABB& bounds, BVHNode* child0, BVHNode* child1)
    {
        m_bounds      = bounds;
        m_children[0] = child0;
        m_children[1] = child1;
    }

    bool     is_leaf() const { return false; }
    int32_t  num_child_nodes() const { return 2; }
    BVHNode* child_node(int32_t i) const
    {
        assert(i >= 0 && i < 2);
        return m_children[i];
    }

    BVHNode* m_children[2];
};

class LeafNode : public BVHNode
{
public:
    LeafNode(const AABB& bounds, int lo, int hi)
    {
        m_bounds = bounds;
        m_lo     = lo;
        m_hi     = hi;
    }
    LeafNode(const LeafNode& s) { *this = s; }

    bool     is_leaf() const { return true; }
    int32_t  num_child_nodes() const { return 0; }
    BVHNode* child_node(int32_t) const { return NULL; }

    int32_t num_triangles() const { return m_hi - m_lo; }
    int32_t m_lo;
    int32_t m_hi;
};
} // namespace lumen