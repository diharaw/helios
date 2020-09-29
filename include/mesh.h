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
};
} // namespace lumen