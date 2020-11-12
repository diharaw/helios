#pragma once

#include <core/integrator.h>

namespace lumen
{
class PathIntegrator : public Integrator
{
public:
    using Ptr = std::shared_ptr<PathIntegrator>;

public:
    PathIntegrator(vk::Backend::Ptr backend);
    ~PathIntegrator();

protected:
    void execute(RenderState& render_state) override;
    void create_pipeline(vk::DescriptorSetLayout::Ptr scene_ds_layout);

private:
    vk::DescriptorSet::Ptr      m_path_trace_ds[2];
    vk::RayTracingPipeline::Ptr m_path_trace_pipeline;
    vk::PipelineLayout::Ptr     m_path_trace_pipeline_layout;
    vk::ShaderBindingTable::Ptr m_path_trace_sbt;
};
} // namespace lumen