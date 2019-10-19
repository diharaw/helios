#pragma once

#include "mesh.h"

namespace lumen
{
class BVH;

class Scene
{
public:
    static std::shared_ptr<Scene> create(const std::string& path);
    ~Scene();
    uint32_t add_mesh(const std::shared_ptr<Mesh> mesh,
                      const glm::mat4&            transform);
    void     set_transform(const uint32_t& id, const glm::mat4& transform);
    void     remove_mesh(const uint32_t& id);
    void     build();

    inline uint32_t num_vertices() { return (uint32_t)m_vtx_positions.size(); }
    inline uint32_t num_triangles() { return (uint32_t)m_triangles.size(); }

    struct MeshInstance
    {
        std::shared_ptr<Mesh> mesh;
        glm::mat4             transform;
    };

    std::vector<MeshInstance>              m_instances;
    std::vector<glm::vec3>                 m_vtx_positions;
    std::vector<glm::vec3>                 m_vtx_normals;
    std::vector<glm::vec2>                 m_vtx_tex_coords;
    std::vector<glm::ivec4>                m_triangles;
    std::vector<std::shared_ptr<Material>> m_materials;

    BVH* m_bvh = nullptr;
};
} // namespace lumen