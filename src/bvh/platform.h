#pragma once

#include <iostream>
#include <stdint.h>

namespace lumen
{
class LeafNode;
class BVHNode;

class Platform
{
public:
    Platform()
    {
        m_name              = "Default";
        m_sah_node_cost     = 1.0f;
        m_sah_triangle_cost = 1.0f;
        m_node_batch_size   = 1;
        m_tri_batch_size    = 1;
        m_min_leaf_size     = 1;
        m_max_leaf_size     = 0x7FFFFFF;
    }
    Platform(const std::string& name, float node_cost = 1.0f, float tri_cost = 1.f, int32_t node_batch_size = 1, int32_t tri_batch_size = 1)
    {
        m_name              = name;
        m_sah_node_cost     = node_cost;
        m_sah_triangle_cost = tri_cost;
        m_node_batch_size   = node_batch_size;
        m_tri_batch_size    = tri_batch_size;
        m_min_leaf_size     = 1;
        m_max_leaf_size     = 0x7FFFFFF;
    }

    inline const std::string& name() const { return m_name; }

    // SAH weights
    inline float sah_triangle_cost() const { return m_sah_triangle_cost; }
    inline float sah_node_cost() const { return m_sah_node_cost; }

    // SAH costs, raw and batched
    inline float cost(int num_child_nodes, int num_tris) const
    {
        return node_cost(num_child_nodes) + triangle_cost(num_tris);
    }
    inline float triangle_cost(int32_t n) const
    {
        return round_to_triangle_batch_size(n) * m_sah_triangle_cost;
    }
    inline float node_cost(int32_t n) const
    {
        return round_to_node_batch_size(n) * m_sah_node_cost;
    }

    // batch processing (how many ops at the price of one)
    inline int32_t triangle_batch_size() const { return m_tri_batch_size; }
    inline int32_t node_batch_size() const { return m_node_batch_size; }
    inline void    set_triangle_batch_size(int32_t tri_batch_size)
    {
        m_tri_batch_size = tri_batch_size;
    }
    inline void set_node_batch_size(int32_t node_batch_size)
    {
        m_node_batch_size = node_batch_size;
    }
    inline int32_t round_to_triangle_batch_size(int32_t n) const
    {
        return ((n + m_tri_batch_size - 1) / m_tri_batch_size) * m_tri_batch_size;
    }
    inline int32_t round_to_node_batch_size(int32_t n) const
    {
        return ((n + m_node_batch_size - 1) / m_node_batch_size) * m_node_batch_size;
    }

    // leaf preferences
    inline void set_leaf_preferences(int32_t minSize, int32_t maxSize)
    {
        m_min_leaf_size = minSize;
        m_max_leaf_size = maxSize;
    }
    inline int32_t min_leaf_size() const { return m_min_leaf_size; }
    inline int32_t max_leaf_size() const { return m_max_leaf_size; }

private:
    std::string m_name;
    float       m_sah_node_cost;
    float       m_sah_triangle_cost;
    int32_t     m_tri_batch_size;
    int32_t     m_node_batch_size;
    int32_t     m_min_leaf_size;
    int32_t     m_max_leaf_size;
};
} // namespace lumen