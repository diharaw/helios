#include <gfx/path_integrator.h>
#include <utility/profiler.h>
#include <vk_mem_alloc.h>

namespace helios
{
// -----------------------------------------------------------------------------------------------------------------------------------

struct PushConstants
{
    glm::mat4  view_inverse;
    glm::mat4  proj_inverse;
    glm::ivec4 ray_debug_pixel_coord;
    float      accumulation;
    uint32_t   num_lights;
    uint32_t   num_frames;
    uint32_t   debug_vis;
    uint32_t   max_ray_bounces;
};

// -----------------------------------------------------------------------------------------------------------------------------------

PathIntegrator::PathIntegrator(vk::Backend::Ptr backend) :
    m_backend(backend)
{
    create_pipeline();
    create_ray_debug_pipeline();
}

// -----------------------------------------------------------------------------------------------------------------------------------

PathIntegrator::~PathIntegrator()
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

void PathIntegrator::render(RenderState& render_state)
{
    HELIOS_SCOPED_SAMPLE("Path Trace");

    if (render_state.scene_state() != SCENE_STATE_READY)
        m_num_accumulated_samples = 0;

    if (m_num_accumulated_samples < m_max_samples)
    {
        auto backend = m_backend.lock();

        auto extents = backend->swap_chain_extents();

        launch_rays(render_state,
                    m_path_trace_pipeline,
                    m_path_trace_pipeline_layout,
                    m_path_trace_sbt,
                    extents.width,
                    extents.height,
                    1,
                    render_state.camera()->view_matrix(),
                    render_state.camera()->projection_matrix(),
                    glm::ivec2(0));

        m_num_accumulated_samples++;
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

void PathIntegrator::gather_debug_rays(const glm::ivec2& pixel_coord, const uint32_t& num_debug_rays, const glm::mat4& view, const glm::mat4& projection, RenderState& render_state)
{
    auto backend = m_backend.lock();

    auto extents = backend->swap_chain_extents();

    launch_rays(render_state,
                m_ray_debug_pipeline,
                m_ray_debug_pipeline_layout,
                m_ray_debug_sbt,
                num_debug_rays,
                1,
                1,
                view,
                projection,
                pixel_coord);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void PathIntegrator::launch_rays(RenderState& render_state, vk::RayTracingPipeline::Ptr pipeline, vk::PipelineLayout::Ptr pipeline_layout, vk::ShaderBindingTable::Ptr sbt, const uint32_t& x, const uint32_t& y, const uint32_t& z, const glm::mat4& view, const glm::mat4& projection, const glm::ivec2& pixel_coord)
{
    auto backend = m_backend.lock();

    auto  extents           = backend->swap_chain_extents();
    auto& rt_pipeline_props = backend->ray_tracing_pipeline_properties();

    vkCmdBindPipeline(render_state.cmd_buffer()->handle(), VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline->handle());

    int32_t push_constant_stages = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR;

    PushConstants push_constants;

    push_constants.ray_debug_pixel_coord = glm::ivec4(pixel_coord.x, extents.height - pixel_coord.y, extents.width, extents.height);
    push_constants.view_inverse          = glm::inverse(view);
    push_constants.proj_inverse          = glm::inverse(projection);
    push_constants.num_lights            = render_state.num_lights();
    push_constants.num_frames            = m_num_accumulated_samples;
    push_constants.accumulation          = float(push_constants.num_frames) / float(push_constants.num_frames + 1);
    push_constants.max_ray_bounces       = m_max_ray_bounces;

    vkCmdPushConstants(render_state.cmd_buffer()->handle(), pipeline_layout->handle(), push_constant_stages, 0, sizeof(PushConstants), &push_constants);

    if (y == 1)
    {
        VkDescriptorSet descriptor_sets[] = {
            render_state.scene_descriptor_set()->handle(),
            render_state.vbo_descriptor_set()->handle(),
            render_state.ibo_descriptor_set()->handle(),
            render_state.material_indices_descriptor_set()->handle(),
            render_state.texture_descriptor_set()->handle(),
            render_state.ray_debug_descriptor_set()->handle()
        };

        vkCmdBindDescriptorSets(render_state.cmd_buffer()->handle(), VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline_layout->handle(), 0, 6, descriptor_sets, 0, nullptr);
    }
    else
    {
        VkDescriptorSet descriptor_sets[] = {
            render_state.scene_descriptor_set()->handle(),
            render_state.vbo_descriptor_set()->handle(),
            render_state.ibo_descriptor_set()->handle(),
            render_state.material_indices_descriptor_set()->handle(),
            render_state.texture_descriptor_set()->handle(),
            render_state.read_image_descriptor_set()->handle(),
            render_state.write_image_descriptor_set()->handle()
        };

        vkCmdBindDescriptorSets(render_state.cmd_buffer()->handle(), VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline_layout->handle(), 0, 7, descriptor_sets, 0, nullptr);
    }

    VkDeviceSize group_size   = vk::utilities::aligned_size(rt_pipeline_props.shaderGroupHandleSize, rt_pipeline_props.shaderGroupBaseAlignment);
    VkDeviceSize group_stride = group_size;

    const VkStridedDeviceAddressRegionKHR raygen_sbt   = { pipeline->shader_binding_table_buffer()->device_address(), group_stride, group_size };
    const VkStridedDeviceAddressRegionKHR miss_sbt     = { pipeline->shader_binding_table_buffer()->device_address() + sbt->miss_group_offset(), group_stride, group_size * 2 };
    const VkStridedDeviceAddressRegionKHR hit_sbt      = { pipeline->shader_binding_table_buffer()->device_address() + sbt->hit_group_offset(), group_stride, group_size * 2 };
    const VkStridedDeviceAddressRegionKHR callable_sbt = { VK_NULL_HANDLE, 0, 0 };

    vkCmdTraceRaysKHR(render_state.cmd_buffer()->handle(), &raygen_sbt, &miss_sbt, &hit_sbt, &callable_sbt, x, y, z);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void PathIntegrator::create_pipeline()
{
    auto backend = m_backend.lock();

    // ---------------------------------------------------------------------------
    // Create shader modules
    // ---------------------------------------------------------------------------

    vk::ShaderModule::Ptr rgen             = vk::ShaderModule::create_from_file(backend, "assets/shader/path_trace.rgen.spv");
    vk::ShaderModule::Ptr rchit            = vk::ShaderModule::create_from_file(backend, "assets/shader/path_trace.rchit.spv");
    vk::ShaderModule::Ptr rahit            = vk::ShaderModule::create_from_file(backend, "assets/shader/path_trace.rahit.spv");
    vk::ShaderModule::Ptr rmiss            = vk::ShaderModule::create_from_file(backend, "assets/shader/path_trace.rmiss.spv");
    vk::ShaderModule::Ptr rchit_visibility = vk::ShaderModule::create_from_file(backend, "assets/shader/path_trace_shadow.rchit.spv");
    vk::ShaderModule::Ptr rmiss_visibility = vk::ShaderModule::create_from_file(backend, "assets/shader/path_trace_shadow.rmiss.spv");

    vk::ShaderBindingTable::Desc sbt_desc;

    sbt_desc.add_ray_gen_group(rgen, "main");
    sbt_desc.add_hit_group(rchit, "main", rahit, "main");
    sbt_desc.add_hit_group(rchit_visibility, "main", rahit, "main");
    sbt_desc.add_miss_group(rmiss, "main");
    sbt_desc.add_miss_group(rmiss_visibility, "main");

    m_path_trace_sbt = vk::ShaderBindingTable::create(backend, sbt_desc);

    vk::RayTracingPipeline::Desc desc;

    desc.set_max_pipeline_ray_recursion_depth(8);
    desc.set_shader_binding_table(m_path_trace_sbt);

    // ---------------------------------------------------------------------------
    // Create pipeline layout
    // ---------------------------------------------------------------------------

    vk::PipelineLayout::Desc pl_desc;

    pl_desc.add_push_constant_range(VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR, 0, sizeof(PushConstants));

    pl_desc.add_descriptor_set_layout(backend->scene_descriptor_set_layout());
    pl_desc.add_descriptor_set_layout(backend->buffer_array_descriptor_set_layout());
    pl_desc.add_descriptor_set_layout(backend->buffer_array_descriptor_set_layout());
    pl_desc.add_descriptor_set_layout(backend->buffer_array_descriptor_set_layout());
    pl_desc.add_descriptor_set_layout(backend->combined_sampler_array_descriptor_set_layout());
    pl_desc.add_descriptor_set_layout(backend->image_descriptor_set_layout());
    pl_desc.add_descriptor_set_layout(backend->image_descriptor_set_layout());

    m_path_trace_pipeline_layout = vk::PipelineLayout::create(backend, pl_desc);

    desc.set_pipeline_layout(m_path_trace_pipeline_layout);

    m_path_trace_pipeline = vk::RayTracingPipeline::create(backend, desc);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void PathIntegrator::create_ray_debug_pipeline()
{
    auto backend = m_backend.lock();

    // ---------------------------------------------------------------------------
    // Create shader modules
    // ---------------------------------------------------------------------------

    vk::ShaderModule::Ptr rgen             = vk::ShaderModule::create_from_file(backend, "assets/shader/path_trace_debug.rgen.spv");
    vk::ShaderModule::Ptr rchit            = vk::ShaderModule::create_from_file(backend, "assets/shader/path_trace_debug.rchit.spv");
    vk::ShaderModule::Ptr rmiss            = vk::ShaderModule::create_from_file(backend, "assets/shader/path_trace_debug.rmiss.spv");
    vk::ShaderModule::Ptr rchit_visibility = vk::ShaderModule::create_from_file(backend, "assets/shader/path_trace_shadow.rchit.spv");
    vk::ShaderModule::Ptr rmiss_visibility = vk::ShaderModule::create_from_file(backend, "assets/shader/path_trace_shadow.rmiss.spv");

    vk::ShaderBindingTable::Desc sbt_desc;

    sbt_desc.add_ray_gen_group(rgen, "main");
    sbt_desc.add_hit_group(rchit, "main");
    sbt_desc.add_hit_group(rchit_visibility, "main");
    sbt_desc.add_miss_group(rmiss, "main");
    sbt_desc.add_miss_group(rmiss_visibility, "main");

    m_ray_debug_sbt = vk::ShaderBindingTable::create(backend, sbt_desc);

    vk::RayTracingPipeline::Desc desc;

    desc.set_max_pipeline_ray_recursion_depth(8);
    desc.set_shader_binding_table(m_ray_debug_sbt);

    // ---------------------------------------------------------------------------
    // Create pipeline layout
    // ---------------------------------------------------------------------------

    vk::PipelineLayout::Desc pl_desc;

    pl_desc.add_push_constant_range(VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR, 0, sizeof(PushConstants));

    pl_desc.add_descriptor_set_layout(backend->scene_descriptor_set_layout());
    pl_desc.add_descriptor_set_layout(backend->buffer_array_descriptor_set_layout());
    pl_desc.add_descriptor_set_layout(backend->buffer_array_descriptor_set_layout());
    pl_desc.add_descriptor_set_layout(backend->buffer_array_descriptor_set_layout());
    pl_desc.add_descriptor_set_layout(backend->combined_sampler_array_descriptor_set_layout());
    pl_desc.add_descriptor_set_layout(backend->ray_debug_descriptor_set_layout());

    m_ray_debug_pipeline_layout = vk::PipelineLayout::create(backend, pl_desc);

    desc.set_pipeline_layout(m_ray_debug_pipeline_layout);

    m_ray_debug_pipeline = vk::RayTracingPipeline::create(backend, desc);
}

// -----------------------------------------------------------------------------------------------------------------------------------
} // namespace helios