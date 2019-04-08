#include "bvh_node.h"
#include <vector>

namespace lumen
{
int BVHNode::subtree_size(BVH_STAT stat) const
{
    int cnt;
    switch (stat)
    {
        default: assert(0); // unknown mode
        case BVH_STAT_NODE_COUNT: cnt = 1; break;
        case BVH_STAT_LEAF_COUNT: cnt = is_leaf() ? 1 : 0; break;
        case BVH_STAT_INNER_COUNT: cnt = is_leaf() ? 0 : 1; break;
        case BVH_STAT_TRIANGLE_COUNT: cnt = is_leaf() ? reinterpret_cast<const LeafNode*>(this)->num_triangles() : 0; break;
        case BVH_STAT_CHILDNODE_COUNT: cnt = num_child_nodes(); break;
    }

    if (!is_leaf())
    {
        for (int i = 0; i < num_child_nodes(); i++)
            cnt += child_node(i)->subtree_size(stat);
    }

    return cnt;
}

void BVHNode::delete_subtree()
{
    for (int i = 0; i < num_child_nodes(); i++)
        child_node(i)->delete_subtree();

    delete this;
}

void BVHNode::compute_subtree_probabilities(const Platform& p, float probability, float& sah)
{
    sah += probability * p.cost(this->num_child_nodes(), this->num_triangles());

    m_probability = probability;

    for (int i = 0; i < num_child_nodes(); i++)
    {
        BVHNode* child              = child_node(i);
        child->m_parent_probability = probability;
        float child_probability     = 0.0f;

        if (probability > 0.0f)
            child_probability = probability * child->m_bounds.area() / this->m_bounds.area();

        child->compute_subtree_probabilities(p, child_probability, sah);
    }
}

// TODO: requires valid probabilities...
float BVHNode::compute_subtree_sah_cost(const Platform& p) const
{
    float SAH = m_probability * p.cost(num_child_nodes(), num_triangles());

    for (int i = 0; i < num_child_nodes(); i++)
        SAH += child_node(i)->compute_subtree_sah_cost(p);

    return SAH;
}

//-------------------------------------------------------------

void assign_indices_depth_first_recursive(BVHNode* node, int32_t& index, bool includeLeafNodes)
{
    if (node->is_leaf() && !includeLeafNodes)
        return;

    node->m_index = index++;
    for (int i = 0; i < node->num_child_nodes(); i++)
        assign_indices_depth_first_recursive(node->child_node(i), index, includeLeafNodes);
}

void BVHNode::assign_indices_depth_first(int32_t index, bool includeLeafNodes)
{
    assign_indices_depth_first_recursive(this, index, includeLeafNodes);
}

//-------------------------------------------------------------

void BVHNode::assign_indices_breadth_first(int32_t index, bool includeLeafNodes)
{
    std::vector<BVHNode*> nodes;
    nodes.push_back(this);
    int32_t head = 0;

    while (head < nodes.size())
    {
        // pop
        BVHNode* node = nodes[head++];

        // discard
        if (node->is_leaf() && !includeLeafNodes)
            continue;

        // assign
        node->m_index = index++;

        // push children
        for (int i = 0; i < node->num_child_nodes(); i++)
            nodes.push_back(node->child_node(i));
    }
}
} // namespace lumen