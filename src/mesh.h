#pragma once

#include <glm.hpp>
#include <memory>
#include <string>
#include <unordered_map>

namespace lumen
{
class Material;

struct SubMesh
{
    uint32_t material_index;
    uint32_t index_count;
    uint32_t base_vertex;
    uint32_t base_index;
};

class Mesh
{
public:
    static std::shared_ptr<Mesh> create(const std::string& path);

    std::vector<glm::vec3>                 m_vtx_positions;
    std::vector<glm::vec3>                 m_vtx_normals;
    std::vector<glm::vec2>                 m_vtx_tex_coords;
    std::vector<uint32_t>                  m_indices;
    std::vector<SubMesh>                   m_sub_meshes;
    std::vector<std::shared_ptr<Material>> m_materials;

    static std::unordered_map<std::string, std::weak_ptr<Mesh>> m_cache;
};
} // namespace lumen