#include <core/renderer.h>
#include <utility/macros.h>
#include <vk_mem_alloc.h>

namespace lumen
{
// -----------------------------------------------------------------------------------------------------------------------------------

Renderer::Renderer(vk::Backend::Ptr backend) :
    m_backend(backend)
{
    create_output_images();
    create_tone_map_pipeline();
    create_descriptor_set_layouts();
    create_buffers();
}

// -----------------------------------------------------------------------------------------------------------------------------------

Renderer::~Renderer()
{
    m_tlas_instance_buffer_device.reset();
    m_tlas_scratch_buffer.reset();
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::render(RenderState& render_state, std::shared_ptr<Integrator> integrator)
{
    auto backend = m_backend.lock();

    if (render_state.scene_state == SCENE_STATE_HIERARCHY_UPDATED)
    {
        auto& tlas_data = render_state.scene->acceleration_structure_data();

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

        vkCmdCopyBuffer(render_state.cmd_buffer->handle(), tlas_data.instance_buffer_host->handle(), m_tlas_instance_buffer_device->handle(), 1, &copy_region);

        {
            VkMemoryBarrier memory_barrier;
            memory_barrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            memory_barrier.pNext         = nullptr;
            memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            memory_barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV;

            vkCmdPipelineBarrier(render_state.cmd_buffer->handle(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &memory_barrier, 0, nullptr, 0, nullptr);
        }

        vkCmdBuildAccelerationStructureNV(render_state.cmd_buffer->handle(),
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

            vkCmdPipelineBarrier(render_state.cmd_buffer->handle(), VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, 0, 1, &memory_barrier, 0, 0, 0, 0);
        }
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::on_window_resize()
{
    create_output_images();
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::create_descriptor_set_layouts()
{
    auto backend = m_backend.lock();

    {
        vk::DescriptorSetLayout::Desc ds_layout_desc;

        ds_layout_desc.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_RAYGEN_BIT_NV);

        m_ouput_image_ds_layout = vk::DescriptorSetLayout::create(backend, ds_layout_desc);
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::create_buffers()
{
    auto backend = m_backend.lock();

    m_tlas_scratch_buffer         = vk::Buffer::create(backend, VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, 1024 * 1024 * 32, VMA_MEMORY_USAGE_GPU_ONLY, 0);
    m_tlas_instance_buffer_device = vk::Buffer::create(backend, VK_BUFFER_USAGE_RAY_TRACING_BIT_NV | VK_BUFFER_USAGE_TRANSFER_DST_BIT, sizeof(RTGeometryInstance) * MAX_SCENE_MESH_COUNT, VMA_MEMORY_USAGE_GPU_ONLY, 0);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::create_tone_map_pipeline()
{
    auto backend = m_backend.lock();

    vk::PipelineLayout::Desc ds_desc;

    ds_desc.add_descriptor_set_layout(m_ouput_image_ds_layout);

    m_tone_map_pipeline_layout = vk::PipelineLayout::create(backend, ds_desc);
    m_tone_map_pipeline        = vk::GraphicsPipeline::create_for_post_process(backend, "shaders/triangle.vert.spv", "shaders/tone_map.frag.spv", m_tone_map_pipeline_layout, backend->swapchain_render_pass());
}
// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::create_output_images()
{
    auto backend = m_backend.lock();
    auto extents = backend->swap_chain_extents();

    for (int i = 0; i < 2; i++)
    {
        m_output_image_views[i].reset();
        m_output_images[i].reset();

        m_output_images[i]      = vk::Image::create(backend, VK_IMAGE_TYPE_2D, extents.width, extents.height, 1, 1, 1, VK_FORMAT_R32G32B32A32_SFLOAT, VMA_MEMORY_USAGE_GPU_ONLY, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_SAMPLE_COUNT_1_BIT);
        m_output_image_views[i] = vk::ImageView::create(backend, m_output_images[i], VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------
} // namespace lumen