#include <gfx/path_integrator.h>
#include <utility/profiler.h>
#include <vk_mem_alloc.h>

namespace helios
{
#define TILE_SIZE 128

// -----------------------------------------------------------------------------------------------------------------------------------

struct PushConstants
{
    glm::mat4  view_proj_inverse;
    glm::vec4  camera_pos;
    glm::vec4  up_direction;
    glm::vec4  right_direction;
    glm::vec4  focal_plane;
    glm::ivec4 ray_debug_pixel_coord;
    glm::uvec4 launch_id_size;
    float      accumulation;
    uint32_t   num_lights;
    uint32_t   num_frames;
    uint32_t   debug_vis;
    uint32_t   max_ray_bounces;
    float      shadow_ray_bias;
    float      focal_length;
    float      aperture_radius;
};

// -----------------------------------------------------------------------------------------------------------------------------------

PathIntegrator::PathIntegrator(vk::Backend::Ptr backend) :
    m_backend(backend)
{
    create_pipeline();
    create_ray_debug_pipeline();
    compute_tile_coords();
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
    {
        m_tile_idx                = 0;
        m_num_accumulated_samples = 0;
    }

    if (m_tile_idx < m_tile_coords.size())
    {
        auto backend = m_backend.lock();

        auto extents = backend->swap_chain_extents();

        launch_rays(render_state,
                    m_path_trace_pipeline,
                    m_path_trace_pipeline_layout,
                    m_path_trace_sbt,
                    m_tile_size.x,
                    m_tile_size.y,
                    1,
                    render_state.camera()->view_matrix(),
                    render_state.camera()->projection_matrix(),
                    m_tile_coords[m_tile_idx],
                    glm::ivec2(0));

        m_num_accumulated_samples++;
    }

    if (m_num_accumulated_samples == m_max_samples)
    {
        m_num_accumulated_samples = 0;
        m_tile_idx++;
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
                glm::uvec2(0, 0),
                pixel_coord);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void PathIntegrator::on_window_resize()
{
    restart_bake();
    compute_tile_coords();
}

// -----------------------------------------------------------------------------------------------------------------------------------

void PathIntegrator::set_tiled(bool tiled)
{
    m_tiled = tiled;
    compute_tile_coords();
}

// -----------------------------------------------------------------------------------------------------------------------------------

void PathIntegrator::launch_rays(RenderState& render_state, vk::RayTracingPipeline::Ptr pipeline, vk::PipelineLayout::Ptr pipeline_layout, vk::ShaderBindingTable::Ptr sbt, const uint32_t& x, const uint32_t& y, const uint32_t& z, const glm::mat4& view, const glm::mat4& projection, const glm::ivec2& tile_coord, const glm::ivec2& pixel_coord)
{
    auto backend = m_backend.lock();

    auto  extents           = backend->swap_chain_extents();
    auto& rt_pipeline_props = backend->ray_tracing_pipeline_properties();

    vkCmdBindPipeline(render_state.cmd_buffer()->handle(), VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline->handle());

    int32_t push_constant_stages = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR;

    glm::vec3 right = glm::vec3(view * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
    glm::vec3 up = glm::vec3(view * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
    glm::vec3 forward = glm::vec3(view * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f));

    glm::vec3 camera_focal_plane_point = render_state.camera()->global_position() + forward * render_state.camera()->focal_length();
    glm::vec4 focal_plane = glm::vec4(-forward, 0.0f);
    focal_plane.w         = -(focal_plane.x * camera_focal_plane_point.x + focal_plane.y * camera_focal_plane_point.y + focal_plane.z * camera_focal_plane_point.z);

    PushConstants push_constants;

    push_constants.ray_debug_pixel_coord = glm::ivec4(pixel_coord.x, extents.height - pixel_coord.y, extents.width, extents.height);
    push_constants.launch_id_size        = glm::ivec4(tile_coord.x, tile_coord.y, extents.width, extents.height);
    push_constants.camera_pos            = glm::vec4(render_state.camera()->global_position(), 0.0f);
    push_constants.up_direction          = glm::vec4(up, 0.0f);
    push_constants.right_direction       = glm::vec4(right, 0.0f);
    push_constants.focal_plane           = focal_plane;
    push_constants.view_proj_inverse     = glm::inverse(projection * view);
    push_constants.num_lights            = render_state.num_lights();
    push_constants.num_frames            = m_num_accumulated_samples;
    push_constants.accumulation          = float(push_constants.num_frames) / float(push_constants.num_frames + 1);
    push_constants.max_ray_bounces       = m_max_ray_bounces;
    push_constants.shadow_ray_bias       = m_shadow_ray_bias;
    push_constants.focal_length       = render_state.camera()->focal_length();
    push_constants.aperture_radius       = render_state.camera()->aperture_radius();

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

void PathIntegrator::compute_tile_coords()
{
    auto backend = m_backend.lock();
    auto extents = backend->swap_chain_extents();

    m_tile_coords.clear();

    if (m_tiled)
    {
        glm::uvec2 num_tiles = glm::uvec2(ceilf(float(extents.width) / float(TILE_SIZE)), ceilf(float(extents.height) / float(TILE_SIZE)));

        for (int x = 0; x < num_tiles.x; x++)
        {
            for (int y = 0; y < num_tiles.y; y++)
                m_tile_coords.push_back(glm::uvec2(x * TILE_SIZE, y * TILE_SIZE));
        }

        m_tile_size = glm::uvec2(TILE_SIZE, TILE_SIZE);
    }
    else
    {
        m_tile_coords.push_back(glm::uvec2(0, 0));
        m_tile_size = glm::uvec2(extents.width, extents.height);
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------
} // namespace helios