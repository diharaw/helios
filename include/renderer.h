#pragma once

#include <scene.h>

namespace lumen
{
class ResourceManager;

class Renderer
{
private:
    uint32_t                       m_width  = 0;
    uint32_t                       m_height = 0;
    vk::Buffer::Ptr                m_tlas_scratch_buffer;
    vk::Buffer::Ptr                m_tlas_instance_buffer_device;
    std::weak_ptr<vk::Backend>     m_backend;
    std::weak_ptr<ResourceManager> m_resource_manager;

    // Path Tracing pass
    vk::DescriptorSetLayout::Ptr m_scene_ds_layout;
    vk::DescriptorSetLayout::Ptr m_per_frame_ds_layout;
    vk::Buffer::Ptr              m_per_frame_ubo;
    size_t                       m_per_frame_ubo_size;
    vk::DescriptorSet::Ptr       m_path_trace_ds[2];
    vk::RayTracingPipeline::Ptr  m_path_trace_pipeline;
    vk::PipelineLayout::Ptr      m_path_trace_pipeline_layout;
    vk::Image::Ptr               m_path_trace_images[2];
    vk::ImageView::Ptr           m_path_trace_image_views[2];
    vk::ShaderBindingTable::Ptr  m_path_trace_sbt;

    // Tone map pass
    vk::GraphicsPipeline::Ptr    m_tone_map_pipeline;
    vk::PipelineLayout::Ptr      m_tone_map_pipeline_layout;
    vk::DescriptorSet::Ptr       m_tone_map_ds[2];
    vk::DescriptorSetLayout::Ptr m_tone_map_layout;

public:
    Renderer(uint32_t width, uint32_t height, vk::Backend::Ptr backend, std::shared_ptr<ResourceManager> resource_manager);
    ~Renderer();

    void render(vk::CommandBuffer::Ptr cmd_buffer, Scene::Ptr scene, RenderState& render_state);
    void on_window_resize(uint32_t width, uint32_t height);

    inline vk::DescriptorSetLayout::Ptr scene_ds_layout() { return m_scene_ds_layout; }

private:
    void create_scene_descriptor_set_layout();
    void create_path_trace_ray_tracing_pipeline();
    void create_tone_map_pipeline();
    void create_output_images();
    void create_buffers();
};
} // namespace lumen