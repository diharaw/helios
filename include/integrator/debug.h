#pragma once

#include <core/integrator.h>

namespace helios
{
class DebugIntegrator : public Integrator
{
public:
    using Ptr = std::shared_ptr<DebugIntegrator>;

    enum View
    {
        VIEW_BASE_COLOR = 0,
        VIEW_NORMAL     = 1,
        VIEW_ROUGHNESS  = 2,
        VIEW_METALLIC   = 3,
        VIEW_EMISSIVE   = 4
    };

public:
    DebugIntegrator(vk::Backend::Ptr backend);
    ~DebugIntegrator();

    inline View current_view() { return m_current_view; }
    inline void set_current_view(const View& view) { m_current_view = view; }

protected:
    void execute(RenderState& render_state) override;
    void gather_debug_rays(const glm::ivec2& pixel_coord, const uint32_t& num_debug_rays, const glm::mat4& view, const glm::mat4& projection, RenderState& render_state) override;
    void create_pipeline();
    void create_ray_debug_pipeline();

private:
    View                        m_current_view = VIEW_BASE_COLOR;
    vk::DescriptorSet::Ptr      m_ds[2];
    vk::RayTracingPipeline::Ptr m_pipeline;
    vk::PipelineLayout::Ptr     m_pipeline_layout;
    vk::ShaderBindingTable::Ptr m_sbt;

    vk::RayTracingPipeline::Ptr m_ray_debug_pipeline;
    vk::PipelineLayout::Ptr     m_ray_debug_pipeline_layout;
    vk::ShaderBindingTable::Ptr m_ray_debug_sbt;
};
} // namespace helios