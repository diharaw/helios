#pragma once

#include <core/vk.h>
#include <glm.hpp>
#include <memory>
#include <vector>

namespace helios
{
struct Vertex
{
    glm::vec4 position;
    glm::vec4 tex_coord;
    glm::vec4 normal;
    glm::vec4 tangent;
    glm::vec4 bitangent;
};

struct SubMesh
{
    std::string name;
    uint32_t    mat_idx;
    uint32_t    index_count;
    uint32_t    vertex_count;
    uint32_t    base_vertex;
    uint32_t    base_index;
    glm::vec3   max_extents;
    glm::vec3   min_extents;
};

class Material;

class Mesh : public vk::Object
{
public:
    using Ptr = std::shared_ptr<Mesh>;

    friend class ResourceManager;

private:
    vk::AccelerationStructure::Ptr         m_blas;
    VkAccelerationStructureCreateInfoKHR   m_blas_info;
    vk::Buffer::Ptr                        m_vbo;
    vk::Buffer::Ptr                        m_ibo;
    std::vector<SubMesh>                   m_sub_meshes;
    std::vector<std::shared_ptr<Material>> m_materials;
    uint32_t                               m_id;

public:
    static Mesh::Ptr create(vk::Backend::Ptr                       backend,
                            vk::Buffer::Ptr                        vbo,
                            vk::Buffer::Ptr                        ibo,
                            std::vector<SubMesh>                   submeshes,
                            std::vector<std::shared_ptr<Material>> materials,
                            vk::BatchUploader&                     uploader);
    static Mesh::Ptr create(vk::Backend::Ptr                       backend,
                            std::vector<Vertex>                    vertices,
                            std::vector<uint32_t>                  indices,
                            std::vector<SubMesh>                   submeshes,
                            std::vector<std::shared_ptr<Material>> materials,
                            vk::BatchUploader&                     uploader);
    ~Mesh();

    inline const std::vector<std::shared_ptr<Material>>& materials() { return m_materials; }
    inline const std::vector<SubMesh>&                   sub_meshes() { return m_sub_meshes; }
    inline vk::AccelerationStructure::Ptr                acceleration_structure() { return m_blas; }
    inline vk::Buffer::Ptr                               vertex_buffer() { return m_vbo; }
    inline vk::Buffer::Ptr                               index_buffer() { return m_ibo; }
    inline uint32_t                                      id() { return m_id; }

private:
    Mesh(vk::Backend::Ptr                       backend,
         vk::Buffer::Ptr                        vbo,
         vk::Buffer::Ptr                        ibo,
         std::vector<SubMesh>                   submeshes,
         std::vector<std::shared_ptr<Material>> materials,
         vk::BatchUploader&                     uploader);
};
} // namespace helios