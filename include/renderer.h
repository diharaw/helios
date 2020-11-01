#pragma once

#include <scene.h>

namespace lumen
{
class Renderer
{
private:
    vk::Buffer::Ptr            m_tlas_scratch_buffer;
    vk::Buffer::Ptr            m_tlas_instance_buffer_device;
    std::weak_ptr<vk::Backend> m_backend;

public:
    Renderer(vk::Backend::Ptr backend);
    ~Renderer();

    void render(vk::CommandBuffer::Ptr cmd_buffer, Scene::Ptr scene, RenderState& render_state);
};
} // namespace lumen