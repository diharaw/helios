#pragma once

#include <vk.h>
#include <memory>

namespace lumen
{
class Mesh
{
public:
    using Ptr = std::shared_ptr<Mesh>;

    friend class ResourceManager;

private:
    vk::AccelerationStructure::Ptr m_blas;
    vk::Buffer::Ptr                m_vbo;
    vk::Buffer::Ptr                m_ibo;

public:
    Mesh(vk::AccelerationStructure::Ptr blas,
         vk::Buffer::Ptr                vbo,
         vk::Buffer::Ptr                ibo);
    ~Mesh();

    inline vk::AccelerationStructure::Ptr acceleration_structure() { return m_blas; }
    inline vk::Buffer::Ptr                vertex_buffer() { return m_vbo; }
    inline vk::Buffer::Ptr                index_buffer() { return m_ibo; }
};
} // namespace lumen