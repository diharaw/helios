#pragma once

#include <resource/scene.h>

namespace lumen
{
class Integrator;
class ResourceManager;

class Renderer
{
private:
    uint32_t                       m_width  = 0;
    uint32_t                       m_height = 0;
    vk::Buffer::Ptr                m_tlas_scratch_buffer;
    vk::Buffer::Ptr                m_tlas_instance_buffer_device;
    std::weak_ptr<vk::Backend>     m_backend;
    std::weak_ptr<Integrator>      m_integrator;

    // Path Tracing pass
    vk::DescriptorSetLayout::Ptr m_scene_ds_layout;
    vk::DescriptorSetLayout::Ptr m_per_frame_ds_layout;
    vk::Buffer::Ptr              m_per_frame_ubo;
    size_t                       m_per_frame_ubo_size;   

public:
    Renderer(uint32_t width, uint32_t height, vk::Backend::Ptr backend);
    ~Renderer();

    void render(std::shared_ptr<Integrator> integrator, vk::CommandBuffer::Ptr cmd_buffer, Scene::Ptr scene, RenderState& render_state);
    void on_window_resize(uint32_t width, uint32_t height);

    inline vk::DescriptorSetLayout::Ptr per_scene_ds_layout() { return m_scene_ds_layout; }
    inline vk::DescriptorSetLayout::Ptr per_frame_ds_layout() { return m_per_frame_ds_layout; }
    inline uint32_t                     width() { return m_width; }
    inline uint32_t                     height() { return m_height; }

private:
    void create_scene_descriptor_set_layout();
    void create_buffers();
};
} // namespace lumen