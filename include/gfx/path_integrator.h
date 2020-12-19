#pragma once

#include <gfx/vk.h>
#include <resource/scene.h>
#include <vector>

namespace helios
{
class PathIntegrator
{
public:
    using Ptr = std::shared_ptr<PathIntegrator>;

public:
    PathIntegrator(vk::Backend::Ptr backend);
    ~PathIntegrator();

    inline uint32_t max_ray_bounces() { return m_max_ray_bounces; }
    inline uint32_t max_samples() { return m_max_samples; }
    inline uint32_t num_accumulated_samples() { return m_max_samples * m_tile_idx + m_num_accumulated_samples; }
    inline uint32_t num_target_samples() { return m_max_samples * m_tile_coords.size(); }
    inline uint32_t tile_idx() { return m_tile_idx; }
    inline bool     is_tiled() { return m_tiled; }
    inline void     restart_bake()
    {
        m_num_accumulated_samples = 0;
        m_tile_idx                = 0;
    }
    inline void set_max_ray_bounces(const uint32_t& n) { m_max_ray_bounces = n; }
    inline void set_max_samples(const uint32_t& n) { m_max_samples = n; }

    void render(RenderState& render_state);
    void gather_debug_rays(const glm::ivec2& pixel_coord, const uint32_t& num_debug_rays, const glm::mat4& view, const glm::mat4& projection, RenderState& render_state);
    void on_window_resize();
    void set_tiled(bool tiled);

private:
    void launch_rays(RenderState& render_state, vk::RayTracingPipeline::Ptr pipeline, vk::PipelineLayout::Ptr pipeline_layout, vk::ShaderBindingTable::Ptr sbt, const uint32_t& x, const uint32_t& y, const uint32_t& z, const glm::mat4& view, const glm::mat4& projection, const glm::ivec2& tile_coord, const glm::ivec2& pixel_coord);
    void create_pipeline();
    void create_ray_debug_pipeline();
    void compute_tile_coords();

private:
    bool                        m_tiled                   = false;
    uint32_t                    m_max_ray_bounces         = 7;
    uint32_t                    m_max_samples             = 5000;
    uint32_t                    m_num_accumulated_samples = 0;
    uint32_t                    m_tile_idx                = 0;
    glm::uvec2                  m_tile_size;
    std::vector<glm::uvec2>     m_tile_coords;
    std::weak_ptr<vk::Backend>  m_backend;
    vk::DescriptorSet::Ptr      m_path_trace_ds[2];
    vk::RayTracingPipeline::Ptr m_path_trace_pipeline;
    vk::PipelineLayout::Ptr     m_path_trace_pipeline_layout;
    vk::ShaderBindingTable::Ptr m_path_trace_sbt;
    vk::RayTracingPipeline::Ptr m_ray_debug_pipeline;
    vk::PipelineLayout::Ptr     m_ray_debug_pipeline_layout;
    vk::ShaderBindingTable::Ptr m_ray_debug_sbt;
};
} // namespace helios