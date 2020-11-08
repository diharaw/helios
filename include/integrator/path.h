#pragma once

#include <core/integrator.h>

namespace lumen
{
class PathIntegrator : public Integrator
{
public:
    PathIntegrator(vk::Backend::Ptr             backend,
                   vk::DescriptorSetLayout::Ptr scene_ds_layout,
                   vk::DescriptorSetLayout::Ptr per_frame_ds_layout);
    ~PathIntegrator();

protected:
    void execute(vk::DescriptorSet::Ptr read_image, vk::DescriptorSet::Ptr write_image, vk::DescriptorSet::Ptr per_scene_ds, vk::DescriptorSet::Ptr per_frame_ds) override;
    void create_pipeline(vk::DescriptorSetLayout::Ptr per_scene_ds_layout, vk::DescriptorSetLayout::Ptr per_frame_ds_layout);

private:
    vk::DescriptorSet::Ptr      m_path_trace_ds[2];
    vk::RayTracingPipeline::Ptr m_path_trace_pipeline;
    vk::PipelineLayout::Ptr     m_path_trace_pipeline_layout;
    vk::ShaderBindingTable::Ptr m_path_trace_sbt;
};
} // namespace lumen