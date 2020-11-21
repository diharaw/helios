#include <integrator/path.h>
#include <core/renderer.h>
#include <vk_mem_alloc.h>

namespace lumen
{
// -----------------------------------------------------------------------------------------------------------------------------------

PathIntegrator::PathIntegrator(vk::Backend::Ptr backend) :
    Integrator(backend)
{
    create_pipeline(backend->scene_descriptor_set_layout());
}

// -----------------------------------------------------------------------------------------------------------------------------------

PathIntegrator::~PathIntegrator()
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

void PathIntegrator::execute(RenderState& render_state)
{
    auto backend = m_backend.lock();

    auto  extents  = backend->swap_chain_extents();
    auto& rt_props = backend->ray_tracing_properties();

    vkCmdBindPipeline(render_state.cmd_buffer()->handle(), VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_path_trace_pipeline->handle());

    int32_t push_constant_stages = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR;

    Integrator::PushConstants push_constants;

    push_constants.num_lights   = glm::uvec4(render_state.directional_lights().size(), render_state.point_lights().size(), render_state.spot_lights().size(), 0);
    push_constants.num_frames   = render_state.num_accumulated_frames();
    push_constants.accumulation = float(push_constants.num_frames) / float(push_constants.num_frames + 1);

    vkCmdPushConstants(render_state.cmd_buffer()->handle(), m_path_trace_pipeline_layout->handle(), push_constant_stages, 0, sizeof(Integrator::PushConstants), &push_constants);

    const uint32_t dynamic_offset = render_state.camera_buffer_offset();

    VkDescriptorSet descriptor_sets[] = {
        render_state.scene_descriptor_set()->handle(),
        render_state.vbo_descriptor_set()->handle(),
        render_state.ibo_descriptor_set()->handle(),
        render_state.instance_descriptor_set()->handle(),
        render_state.texture_descriptor_set()->handle(),
        render_state.read_image_descriptor_set()->handle(),
        render_state.write_image_descriptor_set()->handle()
    };

    vkCmdBindDescriptorSets(render_state.cmd_buffer()->handle(), VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_path_trace_pipeline_layout->handle(), 0, 7, descriptor_sets, 1, &dynamic_offset);

    VkDeviceSize prog_size = rt_props.shaderGroupBaseAlignment;
    VkDeviceSize sbt_size  = prog_size * m_path_trace_pipeline->shader_binding_table()->groups().size();

    const VkStridedBufferRegionKHR raygen_sbt   = { m_path_trace_pipeline->shader_binding_table_buffer()->handle(), 0, prog_size, sbt_size };
    const VkStridedBufferRegionKHR miss_sbt     = { m_path_trace_pipeline->shader_binding_table_buffer()->handle(), m_path_trace_sbt->miss_group_offset(), prog_size, rt_props.shaderGroupBaseAlignment };
    const VkStridedBufferRegionKHR hit_sbt      = { m_path_trace_pipeline->shader_binding_table_buffer()->handle(), m_path_trace_sbt->hit_group_offset(), prog_size, rt_props.shaderGroupBaseAlignment };
    const VkStridedBufferRegionKHR callable_sbt = { VK_NULL_HANDLE, 0, 0, 0 };

    vkCmdTraceRaysKHR(render_state.cmd_buffer()->handle(), &raygen_sbt, &miss_sbt, &hit_sbt, &callable_sbt, extents.width, extents.height, 1);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void PathIntegrator::create_pipeline(vk::DescriptorSetLayout::Ptr scene_ds_layout)
{
    auto backend = m_backend.lock();

    // ---------------------------------------------------------------------------
    // Create shader modules
    // ---------------------------------------------------------------------------

    vk::ShaderModule::Ptr rgen  = vk::ShaderModule::create_from_file(backend, "shader/path_trace.rgen.spv");
    vk::ShaderModule::Ptr rchit = vk::ShaderModule::create_from_file(backend, "shader/path_trace.rchit.spv");
    vk::ShaderModule::Ptr rmiss = vk::ShaderModule::create_from_file(backend, "shader/path_trace.rmiss.spv");

    vk::ShaderBindingTable::Desc sbt_desc;

    sbt_desc.add_ray_gen_group(rgen, "main");
    sbt_desc.add_hit_group(rchit, "main");
    sbt_desc.add_miss_group(rmiss, "main");

    m_path_trace_sbt = vk::ShaderBindingTable::create(backend, sbt_desc);

    vk::RayTracingPipeline::Desc desc;

    desc.set_recursion_depth(1);
    desc.set_shader_binding_table(m_path_trace_sbt);

    // ---------------------------------------------------------------------------
    // Create pipeline layout
    // ---------------------------------------------------------------------------

    vk::PipelineLayout::Desc pl_desc;

    pl_desc.add_push_constant_range(VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR, 0, sizeof(Integrator::PushConstants));

    pl_desc.add_descriptor_set_layout(scene_ds_layout);
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
} // namespace lumen