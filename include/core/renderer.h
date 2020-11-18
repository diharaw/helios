#pragma once

#include <resource/scene.h>

namespace lumen
{
class Integrator;
class ResourceManager;

class Renderer
{
private:
    std::weak_ptr<vk::Backend> m_backend;
    vk::Buffer::Ptr            m_tlas_instance_buffer_device;
    vk::Image::Ptr             m_output_images[2];
    vk::ImageView::Ptr         m_output_image_views[2];
    vk::DescriptorSet::Ptr     m_output_storage_image_ds[2];
    vk::DescriptorSet::Ptr     m_input_combined_sampler_ds[2];
    vk::GraphicsPipeline::Ptr  m_tone_map_pipeline;
    vk::PipelineLayout::Ptr    m_tone_map_pipeline_layout;
    bool                       m_output_ping_pong = false;

public:
    Renderer(vk::Backend::Ptr backend);
    ~Renderer();

    void render(RenderState& render_state, std::shared_ptr<Integrator> integrator);
    void on_window_resize();

private:
    void tone_map(vk::CommandBuffer::Ptr cmd_buf, vk::DescriptorSet::Ptr read_image);
    void create_output_images();
    void create_tone_map_pipeline();
    void create_buffers();
    void create_descriptor_sets();
};
} // namespace lumen