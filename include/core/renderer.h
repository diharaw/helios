#pragma once

#include <resource/scene.h>

namespace lumen
{
class Integrator;
class ResourceManager;

class Renderer
{
private:
    std::weak_ptr<vk::Backend>   m_backend;
    vk::Buffer::Ptr              m_tlas_scratch_buffer;
    vk::Buffer::Ptr              m_tlas_instance_buffer_device;
    vk::Buffer::Ptr              m_per_frame_ubo;
    vk::DescriptorSetLayout::Ptr m_per_scene_ds_layout;
    vk::DescriptorSetLayout::Ptr m_per_frame_ds_layout;
    vk::DescriptorSetLayout::Ptr m_ouput_image_ds_layout;
    vk::Image::Ptr               m_output_images[2];
    vk::ImageView::Ptr           m_output_image_views[2];
    vk::DescriptorSet::Ptr       m_output_storage_image_ds[2];
    vk::GraphicsPipeline::Ptr    m_tone_map_pipeline;
    vk::PipelineLayout::Ptr      m_tone_map_pipeline_layout;
    size_t                       m_per_frame_ubo_size;

public:
    Renderer(vk::Backend::Ptr backend);
    ~Renderer();

    void render(std::shared_ptr<Integrator> integrator, vk::CommandBuffer::Ptr cmd_buffer, Scene::Ptr scene, RenderState& render_state);
    void on_window_resize();

    inline vk::DescriptorSetLayout::Ptr per_scene_ds_layout() { return m_per_scene_ds_layout; }
    inline vk::DescriptorSetLayout::Ptr per_frame_ds_layout() { return m_per_frame_ds_layout; }

private:
    void create_output_images();
    void create_tone_map_pipeline();
    void create_descriptor_set_layouts();
    void create_buffers();
};
} // namespace lumen