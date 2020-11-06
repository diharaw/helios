#include <integrator/path.h>
#include <core/renderer.h>
#include <vk_mem_alloc.h>

namespace lumen
{
// -----------------------------------------------------------------------------------------------------------------------------------

PathIntegrator::PathIntegrator(vk::Backend::Ptr backend, std::weak_ptr<Renderer> renderer) :
    Integrator(backend, renderer)
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

PathIntegrator::~PathIntegrator()
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

void PathIntegrator::execute(vk::DescriptorSet::Ptr per_scene_ds, vk::DescriptorSet::Ptr per_frame_ds)
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

void PathIntegrator::on_window_resized()
{
    create_output_images();
}

// -----------------------------------------------------------------------------------------------------------------------------------

void PathIntegrator::create_path_trace_ray_tracing_pipeline()
{
    auto backend  = m_backend.lock();
    auto renderer = m_renderer.lock();

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

    pl_desc.add_descriptor_set_layout(renderer->per_frame_ds_layout());
    pl_desc.add_descriptor_set_layout(renderer->per_scene_ds_layout());

    m_path_trace_pipeline_layout = vk::PipelineLayout::create(backend, pl_desc);

    desc.set_pipeline_layout(m_path_trace_pipeline_layout);

    m_path_trace_pipeline = vk::RayTracingPipeline::create(backend, desc);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void PathIntegrator::create_tone_map_pipeline()
{
    auto backend = m_backend.lock();

    vk::DescriptorSetLayout::Desc dsl_desc;

    dsl_desc.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);

    m_tone_map_layout = vk::DescriptorSetLayout::create(backend, dsl_desc);

    vk::PipelineLayout::Desc ds_desc;

    ds_desc.add_descriptor_set_layout(m_tone_map_layout);

    m_tone_map_pipeline_layout = vk::PipelineLayout::create(backend, ds_desc);
    m_tone_map_pipeline        = vk::GraphicsPipeline::create_for_post_process(backend, "shaders/triangle.vert.spv", "shaders/tone_map.frag.spv", m_tone_map_pipeline_layout, backend->swapchain_render_pass());
}
// -----------------------------------------------------------------------------------------------------------------------------------

void PathIntegrator::create_output_images()
{
    auto backend = m_backend.lock();
    auto renderer = m_renderer.lock();

    for (int i = 0; i < 2; i++)
    {
        m_path_trace_image_views[i].reset();
        m_path_trace_images[i].reset();

        m_path_trace_images[i]      = vk::Image::create(backend, VK_IMAGE_TYPE_2D, renderer->width(), renderer->height(), 1, 1, 1, VK_FORMAT_R32G32B32A32_SFLOAT, VMA_MEMORY_USAGE_GPU_ONLY, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_SAMPLE_COUNT_1_BIT);
        m_path_trace_image_views[i] = vk::ImageView::create(backend, m_path_trace_images[i], VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------
} // namespace lumen