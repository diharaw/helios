#pragma once

#include <core/integrator.h>

namespace lumen
{
class PathIntegrator : public Integrator
{
public:
    PathIntegrator(vk::Backend::Ptr backend, std::weak_ptr<Renderer> renderer);
    ~PathIntegrator();

protected:
    void execute(vk::DescriptorSet::Ptr per_scene_ds, vk::DescriptorSet::Ptr per_frame_ds) override;
    void on_window_resized() override;
    void create_path_trace_ray_tracing_pipeline();
    void create_tone_map_pipeline();
    void create_output_images();

private:
    vk::DescriptorSet::Ptr      m_path_trace_ds[2];
    vk::RayTracingPipeline::Ptr m_path_trace_pipeline;
    vk::PipelineLayout::Ptr     m_path_trace_pipeline_layout;
    vk::Image::Ptr              m_path_trace_images[2];
    vk::ImageView::Ptr          m_path_trace_image_views[2];
    vk::ShaderBindingTable::Ptr m_path_trace_sbt;

    // Tone map pass
    vk::GraphicsPipeline::Ptr    m_tone_map_pipeline;
    vk::PipelineLayout::Ptr      m_tone_map_pipeline_layout;
    vk::DescriptorSet::Ptr       m_tone_map_ds[2];
    vk::DescriptorSetLayout::Ptr m_tone_map_layout;
};
} // namespace lumen