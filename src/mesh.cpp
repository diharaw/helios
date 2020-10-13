#include <mesh.h>

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
    return std::shared_ptr<Mesh>(new Mesh(backend, vertices, indices, submeshes, materials, uploader));
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

Mesh::Mesh(vk::Backend::Ptr                       backend,
           std::vector<Vertex>                    vertices,
           std::vector<uint32_t>                  indices,
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