#include <mesh.h>
#include <vk_mem_alloc.h>

namespace lumen
{
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
           vk::BatchUploader&                     uploader)
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

Mesh::~Mesh()
{
}

// -----------------------------------------------------------------------------------------------------------------------------------
} // namespace lumen