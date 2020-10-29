#include <mesh.h>
#include <material.h>
#include <vk_mem_alloc.h>

namespace lumen
{
// -----------------------------------------------------------------------------------------------------------------------------------

static uint32_t g_last_mesh_id = 0;

// -----------------------------------------------------------------------------------------------------------------------------------

Mesh::Ptr Mesh::create(vk::Backend::Ptr                       backend,
                       vk::Buffer::Ptr                        vbo,
                       vk::Buffer::Ptr                        ibo,
                       std::vector<SubMesh>                   submeshes,
                       std::vector<std::shared_ptr<Material>> materials,
                       vk::BatchUploader&                     uploader)
{
    return std::shared_ptr<Mesh>(new Mesh(backend, vbo, ibo, submeshes, materials, uploader));
}

// -----------------------------------------------------------------------------------------------------------------------------------

Mesh::Ptr Mesh::create(vk::Backend::Ptr                       backend,
                       std::vector<Vertex>                    vertices,
                       std::vector<uint32_t>                  indices,
                       std::vector<SubMesh>                   submeshes,
                       std::vector<std::shared_ptr<Material>> materials,
                       vk::BatchUploader&                     uploader)
{
    vk::Buffer::Ptr vbo = vk::Buffer::create(backend, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(Vertex) * vertices.size(), VMA_MEMORY_USAGE_GPU_ONLY, 0);
    vk::Buffer::Ptr ibo = vk::Buffer::create(backend, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(uint32_t) * indices.size(), VMA_MEMORY_USAGE_GPU_ONLY, 0);

    uploader.upload_buffer_data(vbo, vertices.data(), 0, sizeof(Vertex) * vertices.size());
    uploader.upload_buffer_data(vbo, indices.data(), 0, sizeof(uint32_t) * indices.size());

    return std::shared_ptr<Mesh>(new Mesh(backend, vbo, ibo, submeshes, materials, uploader));
}

// -----------------------------------------------------------------------------------------------------------------------------------

Mesh::Mesh(vk::Backend::Ptr                       backend,
           vk::Buffer::Ptr                        vbo,
           vk::Buffer::Ptr                        ibo,
           std::vector<SubMesh>                   submeshes,
           std::vector<std::shared_ptr<Material>> materials,
           vk::BatchUploader&                     uploader) :
    m_vbo(vbo),
    m_ibo(ibo),
    m_sub_meshes(submeshes),
    m_materials(materials),
    m_id(g_last_mesh_id++)
{
    // Populate geometries
    for (int i = 0; i < submeshes.size(); i++)
    {
        Material::Ptr material = materials[submeshes[i].mat_idx];

        VkGeometryNV geometry = {};

        geometry.sType                              = VK_STRUCTURE_TYPE_GEOMETRY_NV;
        geometry.pNext                              = nullptr;
        geometry.geometryType                       = VK_GEOMETRY_TYPE_TRIANGLES_NV;
        geometry.geometry.triangles.sType           = VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NV;
        geometry.geometry.triangles.pNext           = nullptr;
        geometry.geometry.triangles.vertexData      = m_vbo->handle();
        geometry.geometry.triangles.vertexOffset    = 0;
        geometry.geometry.triangles.vertexCount     = submeshes[i].index_count / 3;
        geometry.geometry.triangles.vertexStride    = sizeof(Vertex);
        geometry.geometry.triangles.vertexFormat    = VK_FORMAT_R32G32B32_SFLOAT;
        geometry.geometry.triangles.indexData       = m_ibo->handle();
        geometry.geometry.triangles.indexOffset     = submeshes[i].base_index;
        geometry.geometry.triangles.indexCount      = submeshes[i].index_count;
        geometry.geometry.triangles.indexType       = VK_INDEX_TYPE_UINT32;
        geometry.geometry.triangles.transformData   = VK_NULL_HANDLE;
        geometry.geometry.triangles.transformOffset = 0;
        geometry.geometry.aabbs                     = {};
        geometry.geometry.aabbs.sType               = VK_STRUCTURE_TYPE_GEOMETRY_AABB_NV;
        geometry.flags                              = material->type() == MATERIAL_OPAQUE ? VK_GEOMETRY_OPAQUE_BIT_NV : 0;

        m_geometries.push_back(geometry);
    }

    // Create blas
    vk::AccelerationStructure::Desc desc;

    desc.set_type(VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV);
    desc.set_geometries(m_geometries);
    desc.set_instance_count(0);

    m_blas = vk::AccelerationStructure::create(backend, desc);

    uploader.build_blas(m_blas, desc.create_info.info);
}

// -----------------------------------------------------------------------------------------------------------------------------------

Mesh::~Mesh()
{
}

// -----------------------------------------------------------------------------------------------------------------------------------
} // namespace lumen