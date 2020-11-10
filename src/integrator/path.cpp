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

    auto extents = backend->swap_chain_extents();
    auto& rt_props = backend->ray_tracing_properties();

    vkCmdBindPipeline(render_state.cmd_buffer()->handle(), VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, m_path_trace_pipeline->handle());

    float num_accumulated_frames = render_state.num_accumulated_frames();
    float accumulation           = num_accumulated_frames / (num_accumulated_frames + 1);

    vkCmdPushConstants(render_state.cmd_buffer()->handle(), m_path_trace_pipeline_layout->handle(), VK_SHADER_STAGE_RAYGEN_BIT_NV, 0, sizeof(float), &accumulation);
    vkCmdPushConstants(render_state.cmd_buffer()->handle(), m_path_trace_pipeline_layout->handle(), VK_SHADER_STAGE_RAYGEN_BIT_NV, sizeof(float), sizeof(uint32_t), &num_accumulated_frames);

    const uint32_t dynamic_offset = render_state.camera_buffer_offset();

    vkCmdBindDescriptorSets(render_state.cmd_buffer()->handle(), VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, m_path_trace_pipeline_layout->handle(), 0, 1, &render_state.scene_descriptor_set()->handle(), 1, &dynamic_offset);
    vkCmdBindDescriptorSets(render_state.cmd_buffer()->handle(), VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, m_path_trace_pipeline_layout->handle(), 1, 1, &render_state.read_image_descriptor_set()->handle(), 0, VK_NULL_HANDLE);
    vkCmdBindDescriptorSets(render_state.cmd_buffer()->handle(), VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, m_path_trace_pipeline_layout->handle(), 2, 1, &render_state.write_image_descriptor_set()->handle(), 0, VK_NULL_HANDLE);

    vkCmdTraceRaysNV(render_state.cmd_buffer()->handle(),
                     m_path_trace_pipeline->shader_binding_table_buffer()->handle(),
                     0,
                     m_path_trace_pipeline->shader_binding_table_buffer()->handle(),
                     m_path_trace_sbt->miss_group_offset(),
                     rt_props.shaderGroupBaseAlignment,
                     m_path_trace_pipeline->shader_binding_table_buffer()->handle(),
                     m_path_trace_sbt->hit_group_offset(),
                     rt_props.shaderGroupBaseAlignment,
                     VK_NULL_HANDLE,
                     0,
                     0,
                     extents.width,
                     extents.height,
                     1);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void PathIntegrator::create_pipeline(vk::DescriptorSetLayout::Ptr scene_ds_layout)
{
    auto backend = m_backend.lock();

    // ---------------------------------------------------------------------------
    // Create shader modules
    // ---------------------------------------------------------------------------

    vk::ShaderModule::Ptr rgen  = vk::ShaderModule::create_from_file(backend, "shaders/path_trace.rgen.spv");
    vk::ShaderModule::Ptr rchit = vk::ShaderModule::create_from_file(backend, "shaders/path_trace.rchit.spv");
    vk::ShaderModule::Ptr rmiss = vk::ShaderModule::create_from_file(backend, "shaders/path_trace.rmiss.spv");

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

    pl_desc.add_push_constant_range(VK_SHADER_STAGE_RAYGEN_BIT_NV, 0, sizeof(float) * 2);

    pl_desc.add_descriptor_set_layout(scene_ds_layout);
    pl_desc.add_descriptor_set_layout(backend->image_descriptor_set_layout());
    pl_desc.add_descriptor_set_layout(backend->image_descriptor_set_layout());

    m_path_trace_pipeline_layout = vk::PipelineLayout::create(backend, pl_desc);

    desc.set_pipeline_layout(m_path_trace_pipeline_layout);

    m_path_trace_pipeline = vk::RayTracingPipeline::create(backend, desc);
}

// -----------------------------------------------------------------------------------------------------------------------------------
} // namespace lumen