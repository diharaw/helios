#include <renderer.h>
#include <macros.h>
#include <vk_mem_alloc.h>

namespace lumen
{
// -----------------------------------------------------------------------------------------------------------------------------------

struct Transforms
{
    LUMEN_ALIGNED(16)
    glm::mat4 view_inverse;
    LUMEN_ALIGNED(16)
    glm::mat4 proj_inverse;
    LUMEN_ALIGNED(16)
    glm::mat4 model;
    LUMEN_ALIGNED(16)
    glm::mat4 view;
    LUMEN_ALIGNED(16)
    glm::mat4 proj;
    LUMEN_ALIGNED(16)
    glm::vec4 cam_pos;
};

// -----------------------------------------------------------------------------------------------------------------------------------

Renderer::Renderer(uint32_t width, uint32_t height, vk::Backend::Ptr backend, std::shared_ptr<ResourceManager> resource_manager) :
    m_width(width), m_height(height), m_backend(backend), m_resource_manager(resource_manager)
{
    create_scene_descriptor_set_layout();
    create_output_images();
    create_buffers();
    create_path_trace_ray_tracing_pipeline();
    create_tone_map_pipeline();
}

// -----------------------------------------------------------------------------------------------------------------------------------

Renderer::~Renderer()
{
    m_tlas_instance_buffer_device.reset();
    m_tlas_scratch_buffer.reset();
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::render(vk::CommandBuffer::Ptr cmd_buffer, Scene::Ptr scene, RenderState& render_state)
{
    auto backend = m_backend.lock();

    if (render_state.scene_state == SCENE_STATE_HIERARCHY_UPDATED)
    {
        auto& tlas_data = scene->acceleration_structure_data();

        bool is_update = tlas_data.tlas ? true : false;

        if (!is_update)
        {
            // Create top-level acceleration structure
            vk::AccelerationStructure::Desc desc;

            desc.set_instance_count(MAX_SCENE_MESH_COUNT);
            desc.set_type(VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV);
            desc.set_flags(VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_NV | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_NV);

            tlas_data.tlas = vk::AccelerationStructure::create(backend, desc);
        }

        VkBufferCopy copy_region;
        LUMEN_ZERO_MEMORY(copy_region);

        copy_region.dstOffset = 0;
        copy_region.size      = sizeof(RTGeometryInstance) * render_state.meshes.size();

        vkCmdCopyBuffer(cmd_buffer->handle(), tlas_data.instance_buffer_host->handle(), m_tlas_instance_buffer_device->handle(), 1, &copy_region);

        {
            VkMemoryBarrier memory_barrier;
            memory_barrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            memory_barrier.pNext         = nullptr;
            memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            memory_barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV;

            vkCmdPipelineBarrier(cmd_buffer->handle(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &memory_barrier, 0, nullptr, 0, nullptr);
        }

        vkCmdBuildAccelerationStructureNV(cmd_buffer->handle(),
                                          &tlas_data.tlas->info(),
                                          m_tlas_instance_buffer_device->handle(),
                                          0,
                                          is_update,
                                          tlas_data.tlas->handle(),
                                          is_update ? tlas_data.tlas->handle() : VK_NULL_HANDLE,
                                          m_tlas_scratch_buffer->handle(),
                                          0);

        {
            VkMemoryBarrier memory_barrier;
            memory_barrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            memory_barrier.pNext         = nullptr;
            memory_barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV;
            memory_barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV;

            vkCmdPipelineBarrier(cmd_buffer->handle(), VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, 0, 1, &memory_barrier, 0, 0, 0, 0);
        }
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::create_scene_descriptor_set_layout()
{
    auto backend = m_backend.lock();

    vk::DescriptorSetLayout::Desc ds_layout_desc;

    // VBOs
    ds_layout_desc.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_SCENE_MESH_COUNT, VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV | VK_SHADER_STAGE_ANY_HIT_BIT_NV);
    // IBOs
    ds_layout_desc.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_SCENE_MESH_COUNT, VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV | VK_SHADER_STAGE_ANY_HIT_BIT_NV);
    // Material Data
    ds_layout_desc.add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV | VK_SHADER_STAGE_ANY_HIT_BIT_NV);
    // Material Indices
    ds_layout_desc.add_binding(3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_SCENE_MESH_COUNT, VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV | VK_SHADER_STAGE_ANY_HIT_BIT_NV);
    // Material Textures
    ds_layout_desc.add_binding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_SCENE_MATERIAL_TEXTURE_COUNT, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);

    std::vector<VkDescriptorBindingFlagsEXT> descriptor_binding_flags = {
        VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT
    };

    VkDescriptorSetLayoutBindingFlagsCreateInfoEXT set_layout_binding_flags;
    LUMEN_ZERO_MEMORY(set_layout_binding_flags);

    set_layout_binding_flags.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
    set_layout_binding_flags.bindingCount  = 1;
    set_layout_binding_flags.pBindingFlags = descriptor_binding_flags.data();

    ds_layout_desc.set_next_ptr(&set_layout_binding_flags);

    m_scene_ds_layout = vk::DescriptorSetLayout::create(backend, ds_layout_desc);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::create_path_trace_ray_tracing_pipeline()
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

    pl_desc.add_descriptor_set_layout(m_per_frame_ds_layout);
    pl_desc.add_descriptor_set_layout(m_scene_ds_layout);

    m_path_trace_pipeline_layout = vk::PipelineLayout::create(backend, pl_desc);

    desc.set_pipeline_layout(m_path_trace_pipeline_layout);

    m_path_trace_pipeline = vk::RayTracingPipeline::create(backend, desc);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::create_tone_map_pipeline()
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

void Renderer::create_output_images()
{
    auto backend = m_backend.lock();

    for (int i = 0; i < 2; i++)
    {
        m_path_trace_image_views[i].reset();
        m_path_trace_images[i].reset();

        m_path_trace_images[i]      = vk::Image::create(backend, VK_IMAGE_TYPE_2D, m_width, m_height, 1, 1, 1, VK_FORMAT_R32G32B32A32_SFLOAT, VMA_MEMORY_USAGE_GPU_ONLY, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_SAMPLE_COUNT_1_BIT);
        m_path_trace_image_views[i] = vk::ImageView::create(backend, m_path_trace_images[i], VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::create_buffers()
{
    auto backend = m_backend.lock();

    m_per_frame_ubo_size = backend->aligned_dynamic_ubo_size(sizeof(Transforms));
    m_per_frame_ubo      = vk::Buffer::create(backend, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, m_per_frame_ubo_size * vk::Backend::kMaxFramesInFlight, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT);
    m_tlas_scratch_buffer         = vk::Buffer::create(backend, VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, 1024 * 1024 * 32, VMA_MEMORY_USAGE_GPU_ONLY, 0);
    m_tlas_instance_buffer_device = vk::Buffer::create(backend, VK_BUFFER_USAGE_RAY_TRACING_BIT_NV | VK_BUFFER_USAGE_TRANSFER_DST_BIT, sizeof(RTGeometryInstance) * MAX_SCENE_MESH_COUNT, VMA_MEMORY_USAGE_GPU_ONLY, 0);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::on_window_resize(uint32_t width, uint32_t height)
{
    auto backend = m_backend.lock();

    backend->wait_idle();

    m_width  = width;
    m_height = height;

    create_output_images();
}

// -----------------------------------------------------------------------------------------------------------------------------------
} // namespace lumen