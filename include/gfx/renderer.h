#pragma once

#include <resource/scene.h>
#include <gfx/path_integrator.h>

namespace helios
{
#define MAX_DEBUG_RAY_DRAW_COUNT 1024

class ResourceManager;

struct RayDebugView
{
    glm::ivec2 pixel_coord;
    uint32_t   num_debug_rays;
    glm::mat4  view;
    glm::mat4  projection;
};

enum ToneMapOperator
{
    TONE_MAP_OPERATOR_ACES,
    TONE_MAP_OPERATOR_REINHARD
};

class Renderer
{
private:
    std::vector<RayDebugView>         m_ray_debug_views;
    std::weak_ptr<vk::Backend>        m_backend;
    PathIntegrator::Ptr               m_path_integrator;
    vk::Image::Ptr                    m_output_images[2];
    vk::ImageView::Ptr                m_output_image_views[2];
    vk::Image::Ptr                    m_tone_map_image;
    vk::ImageView::Ptr                m_tone_map_image_view;
    vk::Image::Ptr                    m_save_to_disk_image;
    vk::DescriptorSet::Ptr            m_output_storage_image_ds[2];
    vk::DescriptorSet::Ptr            m_input_combined_sampler_ds[2];
    vk::DescriptorSet::Ptr            m_tone_map_ds;
    vk::DescriptorSet::Ptr            m_ray_debug_ds;
    vk::RenderPass::Ptr               m_tone_map_render_pass;
    vk::Framebuffer::Ptr              m_tone_map_framebuffer;
    vk::GraphicsPipeline::Ptr         m_tone_map_pipeline;
    vk::PipelineLayout::Ptr           m_tone_map_pipeline_layout;
    vk::GraphicsPipeline::Ptr         m_copy_pipeline;
    vk::PipelineLayout::Ptr           m_copy_pipeline_layout;
    vk::GraphicsPipeline::Ptr         m_ray_debug_pipeline;
    vk::PipelineLayout::Ptr           m_ray_debug_pipeline_layout;
    vk::GraphicsPipeline::Ptr         m_debug_visualization_pipeline;
    vk::GraphicsPipeline::Ptr         m_depth_prepass_pipeline;
    vk::RenderPass::Ptr               m_depth_prepass_renderpass;
    vk::Framebuffer::Ptr              m_depth_prepass_framebuffer;
    vk::PipelineLayout::Ptr           m_debug_visualization_pipeline_layout;
    vk::RenderPass::Ptr               m_swapchain_renderpass;
    std::vector<vk::Framebuffer::Ptr> m_swapchain_framebuffers;
    vk::Buffer::Ptr                   m_ray_debug_vbo;
    vk::Buffer::Ptr                   m_ray_debug_draw_cmd;
    bool                              m_output_ping_pong       = false;
    bool                              m_ray_debug_view_added   = false;
    bool                              m_output_image_recreated = true;
    bool                              m_save_image_to_disk     = false;
    bool                              m_copy_started           = false;
    std::string                       m_image_save_path        = "";
    ToneMapOperator                   m_tone_map_operator      = TONE_MAP_OPERATOR_ACES;
    float                             m_exposure               = 1.0f;

public:
    Renderer(vk::Backend::Ptr backend);
    ~Renderer();

    inline void                set_tone_map_operator(const ToneMapOperator& tone_map) { m_tone_map_operator = tone_map; }
    inline void                set_exposure(const float& exposure) { m_exposure = exposure; }
    inline PathIntegrator::Ptr path_integrator() { return m_path_integrator; }
    inline ToneMapOperator     tone_map_operator() { return m_tone_map_operator; }
    inline float               exposure() { return m_exposure; }
    inline vk::RenderPass::Ptr swapchain_renderpass() { return m_swapchain_renderpass; }

    void                             render(RenderState& render_state);
    void                             on_window_resize();
    void                             add_ray_debug_view(const glm::ivec2& pixel_coord, const uint32_t& num_debug_rays, const glm::mat4& view, const glm::mat4& projection);
    const std::vector<RayDebugView>& ray_debug_views();
    void                             clear_ray_debug_views();
    void                             save_image_to_disk(const std::string& path);

private:
    void tone_map(vk::CommandBuffer::Ptr cmd_buf, vk::DescriptorSet::Ptr read_image);
    void copy(vk::CommandBuffer::Ptr cmd_buf);
    void render_ray_debug_views(RenderState& render_state);
    void render_debug_visualization(RenderState& render_state);
    void copy_and_save_tone_mapped_image(vk::CommandBuffer::Ptr cmd_buf);
    void create_output_images();
    void create_tone_map_render_pass();
    void create_tone_map_framebuffer();
    void create_depth_prepass_render_pass();
    void create_depth_prepass_framebuffer();
    void create_swapchain_render_pass();
    void create_swapchain_framebuffers();
    void create_tone_map_pipeline();
    void create_copy_pipeline();
    void create_ray_debug_pipeline();
    void create_debug_visualization_pipeline();
    void create_depth_prepass_pipeline();
    void create_ray_debug_buffers();
    void create_static_descriptor_sets();
    void create_dynamic_descriptor_sets();
    void update_dynamic_descriptor_sets();
};
} // namespace helios