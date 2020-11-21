#include <core/renderer.h>
#include <utility/macros.h>
#include <vk_mem_alloc.h>
#include <core/integrator.h>
#include <imgui.h>
#include <examples/imgui_impl_vulkan.h>

namespace lumen
{
// -----------------------------------------------------------------------------------------------------------------------------------

Renderer::Renderer(vk::Backend::Ptr backend) :
    m_backend(backend)
{
    create_output_images();
    create_tone_map_pipeline();
    create_buffers();
    create_descriptor_sets();
}

// -----------------------------------------------------------------------------------------------------------------------------------

Renderer::~Renderer()
{
    m_tlas_instance_buffer_device.reset();
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::render(RenderState& render_state, std::shared_ptr<Integrator> integrator)
{
    if (render_state.scene_state() != SCENE_STATE_READY)
        render_state.m_num_accumulated_frames = 0;

    auto backend = m_backend.lock();

    if (render_state.m_scene_state == SCENE_STATE_HIERARCHY_UPDATED)
    {
        auto& tlas_data = render_state.m_scene->acceleration_structure_data();

        VkBufferCopy copy_region;
        LUMEN_ZERO_MEMORY(copy_region);

        copy_region.dstOffset = 0;
        copy_region.size      = sizeof(VkAccelerationStructureInstanceKHR) * render_state.m_meshes.size();

        vkCmdCopyBuffer(render_state.m_cmd_buffer->handle(), tlas_data.instance_buffer_host->handle(), m_tlas_instance_buffer_device->handle(), 1, &copy_region);

        {
            VkMemoryBarrier memory_barrier;
            memory_barrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            memory_barrier.pNext         = nullptr;
            memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            memory_barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;

            vkCmdPipelineBarrier(render_state.m_cmd_buffer->handle(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &memory_barrier, 0, nullptr, 0, nullptr);
        }

        VkAccelerationStructureGeometryKHR geometry;
        LUMEN_ZERO_MEMORY(geometry);

        geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        geometry.geometry.instances.arrayOfPointers = VK_FALSE;
        geometry.geometry.instances.data.deviceAddress = m_tlas_instance_buffer_device->device_address();

        VkAccelerationStructureGeometryKHR* ptr_geometry = &geometry;

        VkAccelerationStructureBuildGeometryInfoKHR build_info;
        LUMEN_ZERO_MEMORY(build_info);

        build_info.sType                     = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        build_info.type                      = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        build_info.flags                     = tlas_data.tlas->info().flags;
        build_info.update                    = tlas_data.is_built;
        build_info.srcAccelerationStructure  = tlas_data.is_built ? tlas_data.tlas->handle() : VK_NULL_HANDLE;
        build_info.dstAccelerationStructure  = tlas_data.tlas->handle();
        build_info.geometryArrayOfPointers   = VK_FALSE;
        build_info.geometryCount             = 1;
        build_info.ppGeometries              = &ptr_geometry;
        build_info.scratchData.deviceAddress = tlas_data.scratch_buffer->device_address();

        VkAccelerationStructureBuildOffsetInfoKHR build_offset_info;

        build_offset_info.primitiveCount  = render_state.m_meshes.size();
        build_offset_info.primitiveOffset = 0;
        build_offset_info.firstVertex = 0;
        build_offset_info.transformOffset = 0;

        const VkAccelerationStructureBuildOffsetInfoKHR* ptr_build_offset_info = &build_offset_info;

        vkCmdBuildAccelerationStructureKHR(render_state.m_cmd_buffer->handle(), 1, &build_info, &ptr_build_offset_info);

        {
            VkMemoryBarrier memory_barrier;
            memory_barrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            memory_barrier.pNext         = nullptr;
            memory_barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
            memory_barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

            vkCmdPipelineBarrier(render_state.m_cmd_buffer->handle(), VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &memory_barrier, 0, 0, 0, 0);
        }

        tlas_data.is_built = true;
    }

    const uint32_t write_index = (uint32_t)m_output_ping_pong;
    const uint32_t read_index  = (uint32_t)!m_output_ping_pong;

    render_state.m_write_image_ds = m_output_storage_image_ds[write_index];
    render_state.m_read_image_ds  = m_output_storage_image_ds[read_index];

    VkImageSubresourceRange subresource_range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    // Transition write image to general layout during the first frame
    if (backend->current_frame_idx() == 0)
    {
        vk::utilities::set_image_layout(
            render_state.m_cmd_buffer->handle(),
            m_output_images[write_index]->handle(),
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_GENERAL,
            subresource_range);
    }

    // Transition the read image to general layout
    vk::utilities::set_image_layout(
        render_state.m_cmd_buffer->handle(),
        m_output_images[read_index]->handle(),
        backend->current_frame_idx() == 0 ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_IMAGE_LAYOUT_GENERAL,
        subresource_range);

    // Execute integrator
    if (integrator)
        integrator->execute(render_state);

    // Transition the output image from general to as shader read-only layout
    vk::utilities::set_image_layout(
        render_state.m_cmd_buffer->handle(),
        m_output_images[write_index]->handle(),
        VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        subresource_range);

    // Render final onscreen passes
    auto extents = backend->swap_chain_extents();

    VkClearValue clear_values[2];

    clear_values[0].color.float32[0] = 0.0f;
    clear_values[0].color.float32[1] = 0.0f;
    clear_values[0].color.float32[2] = 0.0f;
    clear_values[0].color.float32[3] = 1.0f;

    clear_values[1].color.float32[0] = 1.0f;
    clear_values[1].color.float32[1] = 1.0f;
    clear_values[1].color.float32[2] = 1.0f;
    clear_values[1].color.float32[3] = 1.0f;

    VkRenderPassBeginInfo info    = {};
    info.sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass               = backend->swapchain_render_pass()->handle();
    info.framebuffer              = backend->swapchain_framebuffer()->handle();
    info.renderArea.extent.width  = extents.width;
    info.renderArea.extent.height = extents.height;
    info.clearValueCount          = 2;
    info.pClearValues             = &clear_values[0];

    vkCmdBeginRenderPass(render_state.m_cmd_buffer->handle(), &info, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport vp;

    vp.x        = 0.0f;
    vp.y        = (float)extents.height;
    vp.width    = (float)extents.width;
    vp.height   = -(float)extents.height;
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;

    vkCmdSetViewport(render_state.m_cmd_buffer->handle(), 0, 1, &vp);

    VkRect2D scissor_rect;

    scissor_rect.extent.width  = extents.width;
    scissor_rect.extent.height = extents.height;
    scissor_rect.offset.x      = 0;
    scissor_rect.offset.y      = 0;

    vkCmdSetScissor(render_state.m_cmd_buffer->handle(), 0, 1, &scissor_rect);

    // Perform tone mapping and render ImGui
    tone_map(render_state.m_cmd_buffer, m_input_combined_sampler_ds[write_index]);

    // Render ImGui
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), render_state.m_cmd_buffer->handle());

    vkCmdEndRenderPass(render_state.m_cmd_buffer->handle());

    render_state.m_num_accumulated_frames++;
    m_output_ping_pong = !m_output_ping_pong;

    render_state.clear();
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::tone_map(vk::CommandBuffer::Ptr cmd_buf, vk::DescriptorSet::Ptr read_image)
{
    auto backend = m_backend.lock();
    auto extents = backend->swap_chain_extents();

    vkCmdBindPipeline(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_tone_map_pipeline->handle());

    vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_tone_map_pipeline_layout->handle(), 0, 1, &read_image->handle(), 0, nullptr);

    // Apply tonemapping
    vkCmdDraw(cmd_buf->handle(), 3, 1, 0, 0);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::on_window_resize()
{
    create_output_images();
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::create_buffers()
{
    auto backend = m_backend.lock();

    m_tlas_instance_buffer_device = vk::Buffer::create(backend, VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, sizeof(VkAccelerationStructureInstanceKHR) * MAX_SCENE_MESH_INSTANCE_COUNT, VMA_MEMORY_USAGE_GPU_ONLY, 0);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::create_tone_map_pipeline()
{
    auto backend = m_backend.lock();

    vk::PipelineLayout::Desc ds_desc;

    ds_desc.add_descriptor_set_layout(backend->combined_sampler_descriptor_set_layout());

    m_tone_map_pipeline_layout = vk::PipelineLayout::create(backend, ds_desc);
    m_tone_map_pipeline        = vk::GraphicsPipeline::create_for_post_process(backend, "shader/triangle.vert.spv", "shader/tone_map.frag.spv", m_tone_map_pipeline_layout, backend->swapchain_render_pass());
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

void Renderer::create_descriptor_sets()
{
    auto backend = m_backend.lock();

    int idx = 0;

    std::vector<VkWriteDescriptorSet>  write_datas;
    std::vector<VkDescriptorImageInfo> image_descriptors;

    write_datas.reserve(4);
    image_descriptors.reserve(4);

    for (int i = 0; i < 2; i++)
    {
        m_output_storage_image_ds[i]   = backend->allocate_descriptor_set(backend->image_descriptor_set_layout());
        m_input_combined_sampler_ds[i] = backend->allocate_descriptor_set(backend->combined_sampler_descriptor_set_layout());

        {
            VkDescriptorImageInfo image_info;

            LUMEN_ZERO_MEMORY(image_info);

            image_info.sampler     = nullptr;
            image_info.imageView   = m_output_image_views[i]->handle();
            image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            image_descriptors.push_back(image_info);

            VkWriteDescriptorSet write_data;

            LUMEN_ZERO_MEMORY(write_data);

            write_data.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_data.descriptorCount = 1;
            write_data.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            write_data.pImageInfo      = &image_descriptors[idx++];
            write_data.dstBinding      = 0;
            write_data.dstSet          = m_output_storage_image_ds[i]->handle();

            write_datas.push_back(write_data);
        }

        {
            VkDescriptorImageInfo image_info;

            LUMEN_ZERO_MEMORY(image_info);

            image_info.sampler     = backend->bilinear_sampler()->handle();
            image_info.imageView   = m_output_image_views[i]->handle();
            image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            image_descriptors.push_back(image_info);

            VkWriteDescriptorSet write_data;

            LUMEN_ZERO_MEMORY(write_data);

            write_data.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_data.descriptorCount = 1;
            write_data.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write_data.pImageInfo      = &image_descriptors[idx++];
            write_data.dstBinding      = 0;
            write_data.dstSet          = m_input_combined_sampler_ds[i]->handle();

            write_datas.push_back(write_data);
        }
    }

    vkUpdateDescriptorSets(backend->device(), write_datas.size(), &write_datas[0], 0, nullptr);
}

// -----------------------------------------------------------------------------------------------------------------------------------
} // namespace lumen