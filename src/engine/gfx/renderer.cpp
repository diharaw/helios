#include <gfx/renderer.h>
#include <utility/macros.h>
#include <utility/profiler.h>
#include <utility/logger.h>
#include <resource/mesh.h>
#include <vk_mem_alloc.h>
#include <imgui.h>
#include <examples/imgui_impl_vulkan.h>
#include <resource/scene.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace helios
{
// -----------------------------------------------------------------------------------------------------------------------------------

struct RayDebugVertex
{
    glm::vec4 position;
    glm::vec4 color;
};

// -----------------------------------------------------------------------------------------------------------------------------------

struct ToneMapPushConstants
{
    float    exposure;
    uint32_t tone_map_operator;
};

// -----------------------------------------------------------------------------------------------------------------------------------

struct DebugVisualizationPushConstants
{
    glm::mat4 view_proj;
    uint32_t  instance_id;
    uint32_t  submesh_id;
    uint32_t  current_output_buffer;
};

// -----------------------------------------------------------------------------------------------------------------------------------

Renderer::Renderer(vk::Backend::Ptr backend) :
    m_backend(backend)
{
    m_path_integrator = std::shared_ptr<PathIntegrator>(new PathIntegrator(backend));

    create_output_images();
    create_tone_map_render_pass();
    create_swapchain_render_pass();
    create_depth_prepass_render_pass();
    create_tone_map_framebuffer();
    create_depth_prepass_framebuffer();
    create_swapchain_framebuffers();
    create_tone_map_pipeline();
    create_copy_pipeline();
    create_ray_debug_buffers();
    create_ray_debug_pipeline();
    create_debug_visualization_pipeline();
    create_depth_prepass_pipeline();
    create_static_descriptor_sets();
    create_dynamic_descriptor_sets();
    update_dynamic_descriptor_sets();
}

// -----------------------------------------------------------------------------------------------------------------------------------

Renderer::~Renderer()
{
    for (int i = 0; i < 2; i++)
    {
        m_output_images[i].reset();
        m_output_image_views[i].reset();
        m_output_storage_image_ds[i].reset();
        m_input_combined_sampler_ds[i].reset();
    }

    m_swapchain_framebuffers.clear();
    m_swapchain_renderpass.reset();
    m_debug_visualization_pipeline.reset();
    m_depth_prepass_pipeline.reset();
    m_depth_prepass_framebuffer.reset();
    m_depth_prepass_renderpass.reset();
    m_debug_visualization_pipeline_layout.reset();
    m_save_to_disk_image.reset();
    m_tone_map_ds.reset();
    m_tone_map_image_view.reset();
    m_tone_map_image.reset();
    m_ray_debug_vbo.reset();
    m_ray_debug_draw_cmd.reset();
    m_ray_debug_ds.reset();
    m_tone_map_pipeline.reset();
    m_tone_map_pipeline_layout.reset();
    m_ray_debug_pipeline.reset();
    m_ray_debug_pipeline_layout.reset();
    m_tone_map_framebuffer.reset();
    m_tone_map_render_pass.reset();
    m_tone_map_pipeline_layout.reset();
    m_tone_map_pipeline.reset();
    m_copy_pipeline_layout.reset();
    m_copy_pipeline.reset();
    m_path_integrator.reset();
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::render(RenderState& render_state)
{
    HELIOS_SCOPED_SAMPLE("Render");

    auto backend = m_backend.lock();

    if (render_state.m_scene && render_state.m_scene_state == SCENE_STATE_HIERARCHY_UPDATED)
    {
        auto& tlas_data = render_state.m_scene->acceleration_structure_data();

        if (render_state.m_meshes.size() > 0)
        {
            VkBufferCopy copy_region;
            HELIOS_ZERO_MEMORY(copy_region);

            copy_region.dstOffset = 0;
            copy_region.size      = sizeof(VkAccelerationStructureInstanceKHR) * render_state.m_meshes.size();

            vkCmdCopyBuffer(render_state.m_cmd_buffer->handle(), tlas_data.instance_buffer_host->handle(), tlas_data.instance_buffer_device->handle(), 1, &copy_region);
        }

        {
            VkMemoryBarrier memory_barrier;
            memory_barrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            memory_barrier.pNext         = nullptr;
            memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            memory_barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;

            vkCmdPipelineBarrier(render_state.m_cmd_buffer->handle(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &memory_barrier, 0, nullptr, 0, nullptr);
        }

        VkAccelerationStructureGeometryKHR geometry;
        HELIOS_ZERO_MEMORY(geometry);

        geometry.sType                                 = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        geometry.geometryType                          = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        geometry.geometry.instances.sType              = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        geometry.geometry.instances.arrayOfPointers    = VK_FALSE;
        geometry.geometry.instances.data.deviceAddress = tlas_data.instance_buffer_device->device_address();

        VkAccelerationStructureBuildGeometryInfoKHR build_info;
        HELIOS_ZERO_MEMORY(build_info);

        build_info.sType                     = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        build_info.type                      = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        build_info.flags                     = tlas_data.tlas->flags();
        build_info.srcAccelerationStructure  = tlas_data.is_built ? tlas_data.tlas->handle() : VK_NULL_HANDLE;
        build_info.dstAccelerationStructure  = tlas_data.tlas->handle();
        build_info.geometryCount             = 1;
        build_info.pGeometries               = &geometry;
        build_info.scratchData.deviceAddress = tlas_data.scratch_buffer->device_address();

        VkAccelerationStructureBuildRangeInfoKHR build_range_info;

        build_range_info.primitiveCount  = render_state.m_meshes.size();
        build_range_info.primitiveOffset = 0;
        build_range_info.firstVertex     = 0;
        build_range_info.transformOffset = 0;

        const VkAccelerationStructureBuildRangeInfoKHR* ptr_build_range_info = &build_range_info;

        vkCmdBuildAccelerationStructuresKHR(render_state.m_cmd_buffer->handle(), 1, &build_info, &ptr_build_range_info);

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
    render_state.m_ray_debug_ds   = m_ray_debug_ds;

    VkImageSubresourceRange color_subresource_range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    VkImageSubresourceRange depth_subresource_range = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };

    // Transition write image to general layout during the first frame
    if (m_output_image_recreated)
    {
        vk::utilities::set_image_layout(
            render_state.m_cmd_buffer->handle(),
            m_output_images[write_index]->handle(),
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_GENERAL,
            color_subresource_range);
    }

    // Transition the read image to general layout
    vk::utilities::set_image_layout(
        render_state.m_cmd_buffer->handle(),
        m_output_images[read_index]->handle(),
        m_output_image_recreated ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_IMAGE_LAYOUT_GENERAL,
        color_subresource_range);

    // Begin path trace iteration
    if (render_state.m_scene)
        m_path_integrator->render(render_state);

    if (m_ray_debug_view_added)
    {
        m_ray_debug_view_added = false;

        // If the ray debug view was just added in the current frame, reset the draw cmd data.
        if (m_ray_debug_views.size() == 1)
        {
            uint32_t draw_args[4] = { 0, 1, 0, 0 };
            vkCmdUpdateBuffer(render_state.m_cmd_buffer->handle(), m_ray_debug_draw_cmd->handle(), 0, sizeof(uint32_t) * 4, &draw_args[0]);

            VkMemoryBarrier memory_barrier;
            memory_barrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            memory_barrier.pNext         = nullptr;
            memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            memory_barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

            vkCmdPipelineBarrier(render_state.m_cmd_buffer->handle(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 1, &memory_barrier, 0, nullptr, 0, nullptr);
        }

        const auto& view = m_ray_debug_views.back();
        m_path_integrator->gather_debug_rays(view.pixel_coord, view.num_debug_rays, view.view, view.projection, render_state);
    }

    // Transition the output image from general to as shader read-only layout
    vk::utilities::set_image_layout(
        render_state.m_cmd_buffer->handle(),
        m_output_images[write_index]->handle(),
        VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        color_subresource_range);

    // Tone map output
    tone_map(render_state.m_cmd_buffer, m_input_combined_sampler_ds[write_index]);

    // Copy screenshot
    if (m_save_image_to_disk)
        copy_and_save_tone_mapped_image(render_state.m_cmd_buffer);

    if (m_ray_debug_views.size() > 0)
        render_depth_prepass(render_state);
    else
    {
        vk::utilities::set_image_layout(
            render_state.m_cmd_buffer->handle(),
            backend->swapchain_depth_image()->handle(),
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            depth_subresource_range);
    }

    // Render final onscreen passes
    auto extents = backend->swap_chain_extents();

    VkClearValue clear_value;

    clear_value.color.float32[0] = 0.0f;
    clear_value.color.float32[1] = 0.0f;
    clear_value.color.float32[2] = 0.0f;
    clear_value.color.float32[3] = 1.0f;

    VkRenderPassBeginInfo info    = {};
    info.sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass               = m_swapchain_renderpass->handle();
    info.framebuffer              = m_swapchain_framebuffers[backend->current_frame_idx()]->handle();
    info.renderArea.extent.width  = extents.width;
    info.renderArea.extent.height = extents.height;
    info.clearValueCount          = 1;
    info.pClearValues             = &clear_value;

    vkCmdBeginRenderPass(render_state.m_cmd_buffer->handle(), &info, VK_SUBPASS_CONTENTS_INLINE);

    if (m_ray_debug_views.size() == 0)
    {
        VkClearAttachment attachment;

        attachment.aspectMask                      = VK_IMAGE_ASPECT_DEPTH_BIT;
        attachment.clearValue.depthStencil.depth   = 1.0f;
        attachment.clearValue.depthStencil.stencil = 255;

        VkClearRect rect;

        rect.baseArrayLayer = 0;
        rect.layerCount     = 1;
        rect.rect.extent    = info.renderArea.extent;
        rect.rect.offset.x  = 0;
        rect.rect.offset.y  = 0;

        vkCmdClearAttachments(render_state.m_cmd_buffer->handle(), 1, &attachment, 1, &rect);
    }

    VkRect2D scissor_rect;

    scissor_rect.extent.width  = extents.width;
    scissor_rect.extent.height = extents.height;
    scissor_rect.offset.x      = 0;
    scissor_rect.offset.y      = 0;

    vkCmdSetScissor(render_state.m_cmd_buffer->handle(), 0, 1, &scissor_rect);

    // Copy tone mapped image to swapchain image
    if (m_current_output_buffer == OUTPUT_BUFFER_FINAL)
        copy(render_state.m_cmd_buffer);
    else
        render_debug_visualization(render_state);

    // If any ray debug views were added, render them
    if (m_ray_debug_views.size() > 0)
        render_ray_debug_views(render_state);

    {
        HELIOS_SCOPED_SAMPLE("UI");

        VkViewport vp;

        vp.x        = 0.0f;
        vp.y        = 0.0f;
        vp.width    = (float)extents.width;
        vp.height   = (float)extents.height;
        vp.minDepth = 0.0f;
        vp.maxDepth = 1.0f;

        vkCmdSetViewport(render_state.m_cmd_buffer->handle(), 0, 1, &vp);

        // Render ImGui
        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), render_state.m_cmd_buffer->handle());
    }

    vkCmdEndRenderPass(render_state.m_cmd_buffer->handle());

    m_output_ping_pong = !m_output_ping_pong;

    render_state.clear();

    if (m_output_image_recreated)
        m_output_image_recreated = false;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::tone_map(vk::CommandBuffer::Ptr cmd_buf, vk::DescriptorSet::Ptr read_image)
{
    HELIOS_SCOPED_SAMPLE("Tone Map");

    auto backend = m_backend.lock();
    auto extents = backend->swap_chain_extents();

    VkClearValue clear_value;

    clear_value.color.float32[0] = 0.0f;
    clear_value.color.float32[1] = 0.0f;
    clear_value.color.float32[2] = 0.0f;
    clear_value.color.float32[3] = 1.0f;

    VkRenderPassBeginInfo info    = {};
    info.sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass               = m_tone_map_render_pass->handle();
    info.framebuffer              = m_tone_map_framebuffer->handle();
    info.renderArea.extent.width  = extents.width;
    info.renderArea.extent.height = extents.height;
    info.clearValueCount          = 1;
    info.pClearValues             = &clear_value;

    vkCmdBeginRenderPass(cmd_buf->handle(), &info, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport vp;

    vp.x        = 0.0f;
    vp.y        = (float)extents.height;
    vp.width    = (float)extents.width;
    vp.height   = -(float)extents.height;
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;

    vkCmdSetViewport(cmd_buf->handle(), 0, 1, &vp);

    VkRect2D scissor_rect;

    scissor_rect.extent.width  = extents.width;
    scissor_rect.extent.height = extents.height;
    scissor_rect.offset.x      = 0;
    scissor_rect.offset.y      = 0;

    vkCmdSetScissor(cmd_buf->handle(), 0, 1, &scissor_rect);

    vkCmdBindPipeline(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_tone_map_pipeline->handle());

    vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_tone_map_pipeline_layout->handle(), 0, 1, &read_image->handle(), 0, nullptr);

    ToneMapPushConstants pc;

    pc.exposure          = m_exposure;
    pc.tone_map_operator = m_tone_map_operator;

    vkCmdPushConstants(cmd_buf->handle(), m_tone_map_pipeline_layout->handle(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ToneMapPushConstants), &pc);

    vkCmdDraw(cmd_buf->handle(), 3, 1, 0, 0);

    vkCmdEndRenderPass(cmd_buf->handle());
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::copy(vk::CommandBuffer::Ptr cmd_buf)
{
    HELIOS_SCOPED_SAMPLE("Copy");

    auto backend = m_backend.lock();
    auto extents = backend->swap_chain_extents();

    VkViewport vp;

    vp.x        = 0.0f;
    vp.y        = 0.0f;
    vp.width    = (float)extents.width;
    vp.height   = (float)extents.height;
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;

    vkCmdSetViewport(cmd_buf->handle(), 0, 1, &vp);

    vkCmdBindPipeline(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_copy_pipeline->handle());

    vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_copy_pipeline_layout->handle(), 0, 1, &m_tone_map_ds->handle(), 0, nullptr);

    vkCmdDraw(cmd_buf->handle(), 3, 1, 0, 0);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::render_ray_debug_views(RenderState& render_state)
{
    HELIOS_SCOPED_SAMPLE("Ray Debug View");

    auto backend = m_backend.lock();
    auto extents = backend->swap_chain_extents();

    VkViewport vp;

    vp.x        = 0.0f;
    vp.y        = (float)extents.height;
    vp.width    = (float)extents.width;
    vp.height   = -(float)extents.height;
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;

    vkCmdSetViewport(render_state.m_cmd_buffer->handle(), 0, 1, &vp);

    vkCmdBindPipeline(render_state.m_cmd_buffer->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_ray_debug_pipeline->handle());

    const VkDeviceSize offset = 0;

    vkCmdBindVertexBuffers(render_state.m_cmd_buffer->handle(), 0, 1, &m_ray_debug_vbo->handle(), &offset);

    glm::mat4 view_proj = render_state.m_camera->projection_matrix() * render_state.m_camera->view_matrix();
    vkCmdPushConstants(render_state.m_cmd_buffer->handle(), m_ray_debug_pipeline_layout->handle(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &view_proj);

    vkCmdDrawIndirect(render_state.m_cmd_buffer->handle(), m_ray_debug_draw_cmd->handle(), 0, 1, sizeof(uint32_t) * 4);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::render_debug_visualization(RenderState& render_state)
{
    HELIOS_SCOPED_SAMPLE("Debug Visualization");

    auto backend = m_backend.lock();
    auto extents = backend->swap_chain_extents();

    VkViewport vp;

    vp.x        = 0.0f;
    vp.y        = (float)extents.height;
    vp.width    = (float)extents.width;
    vp.height   = -(float)extents.height;
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;

    vkCmdSetViewport(render_state.m_cmd_buffer->handle(), 0, 1, &vp);

    vkCmdBindPipeline(render_state.m_cmd_buffer->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_debug_visualization_pipeline->handle());

    VkDescriptorSet descriptor_sets[] = {
        render_state.scene_descriptor_set()->handle(),
        render_state.material_indices_descriptor_set()->handle(),
        render_state.texture_descriptor_set()->handle()
    };

    vkCmdBindDescriptorSets(render_state.cmd_buffer()->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_debug_visualization_pipeline_layout->handle(), 0, 3, descriptor_sets, 0, nullptr);

    const auto& meshes = render_state.meshes();

    for (int mesh_idx = 0; mesh_idx < meshes.size(); mesh_idx++)
    {
        const auto& mesh = meshes[mesh_idx]->mesh();

        const VkBuffer     buffer = mesh->vertex_buffer()->handle();
        const VkDeviceSize size   = 0;
        vkCmdBindVertexBuffers(render_state.cmd_buffer()->handle(), 0, 1, &buffer, &size);
        vkCmdBindIndexBuffer(render_state.cmd_buffer()->handle(), mesh->index_buffer()->handle(), 0, VK_INDEX_TYPE_UINT32);

        const auto& submeshes = mesh->sub_meshes();

        for (int submesh_idx = 0; submesh_idx < submeshes.size(); submesh_idx++)
        {
            const auto& submesh = submeshes[submesh_idx];

            DebugVisualizationPushConstants push_constants;
            push_constants.view_proj             = render_state.camera()->projection_matrix() * render_state.camera()->view_matrix();
            push_constants.instance_id           = mesh_idx;
            push_constants.submesh_id            = submesh_idx;
            push_constants.current_output_buffer = m_current_output_buffer;

            vkCmdPushConstants(render_state.cmd_buffer()->handle(), m_debug_visualization_pipeline_layout->handle(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(DebugVisualizationPushConstants), &push_constants);

            vkCmdDrawIndexed(render_state.cmd_buffer()->handle(), submesh.index_count, 1, submesh.base_index, submesh.base_vertex, 0);
        }
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::render_depth_prepass(RenderState& render_state)
{
    HELIOS_SCOPED_SAMPLE("Depth Prepass");

    auto backend = m_backend.lock();
    auto extents = backend->swap_chain_extents();

    VkClearValue clear_value;

    clear_value.depthStencil.depth   = 1.0f;
    clear_value.depthStencil.stencil = 255;

    VkRenderPassBeginInfo info    = {};
    info.sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass               = m_depth_prepass_renderpass->handle();
    info.framebuffer              = m_depth_prepass_framebuffer->handle();
    info.renderArea.extent.width  = extents.width;
    info.renderArea.extent.height = extents.height;
    info.clearValueCount          = 1;
    info.pClearValues             = &clear_value;

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

    vkCmdBindPipeline(render_state.m_cmd_buffer->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_depth_prepass_pipeline->handle());

    VkDescriptorSet descriptor_sets[] = {
        render_state.scene_descriptor_set()->handle(),
        render_state.material_indices_descriptor_set()->handle(),
        render_state.texture_descriptor_set()->handle()
    };

    vkCmdBindDescriptorSets(render_state.cmd_buffer()->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_debug_visualization_pipeline_layout->handle(), 0, 3, descriptor_sets, 0, nullptr);

    const auto& meshes = render_state.meshes();

    for (int mesh_idx = 0; mesh_idx < meshes.size(); mesh_idx++)
    {
        const auto& mesh = meshes[mesh_idx]->mesh();

        const VkBuffer     buffer = mesh->vertex_buffer()->handle();
        const VkDeviceSize size   = 0;
        vkCmdBindVertexBuffers(render_state.cmd_buffer()->handle(), 0, 1, &buffer, &size);
        vkCmdBindIndexBuffer(render_state.cmd_buffer()->handle(), mesh->index_buffer()->handle(), 0, VK_INDEX_TYPE_UINT32);

        const auto& submeshes = mesh->sub_meshes();

        for (int submesh_idx = 0; submesh_idx < submeshes.size(); submesh_idx++)
        {
            const auto& submesh = submeshes[submesh_idx];

            DebugVisualizationPushConstants push_constants;
            push_constants.view_proj   = render_state.camera()->projection_matrix() * render_state.camera()->view_matrix();
            push_constants.instance_id = mesh_idx;
            push_constants.submesh_id  = submesh_idx;

            vkCmdPushConstants(render_state.cmd_buffer()->handle(), m_debug_visualization_pipeline_layout->handle(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(DebugVisualizationPushConstants), &push_constants);

            vkCmdDrawIndexed(render_state.cmd_buffer()->handle(), submesh.index_count, 1, submesh.base_index, submesh.base_vertex, 0);
        }
    }

    vkCmdEndRenderPass(render_state.m_cmd_buffer->handle());
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::copy_and_save_tone_mapped_image(vk::CommandBuffer::Ptr cmd_buf)
{
    auto backend = m_backend.lock();
    auto extents = backend->swap_chain_extents();

    if (m_copy_started)
    {
        backend->wait_idle();

        // Get layout of the image (including row pitch)
        VkImageSubresource  subResource { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
        VkSubresourceLayout subResourceLayout;
        vkGetImageSubresourceLayout(backend->device(), m_save_to_disk_image->handle(), &subResource, &subResourceLayout);

        if (stbi_write_png(m_image_save_path.c_str(), extents.width, extents.height, 4, m_save_to_disk_image->mapped_ptr(), sizeof(char) * 4 * extents.width) == 0)
            HELIOS_LOG_ERROR("Failed to write image to disk.");

        m_copy_started       = false;
        m_save_image_to_disk = false;
        m_image_save_path    = "";
    }
    else
    {
        VkImageSubresourceRange subresource_range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        vk::utilities::set_image_layout(
            cmd_buf->handle(),
            m_tone_map_image->handle(),
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            subresource_range);

        vk::utilities::set_image_layout(
            cmd_buf->handle(),
            m_save_to_disk_image->handle(),
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            subresource_range);

        VkImageCopy image_copy_region {};
        image_copy_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_copy_region.srcSubresource.layerCount = 1;
        image_copy_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_copy_region.dstSubresource.layerCount = 1;
        image_copy_region.extent.width              = extents.width;
        image_copy_region.extent.height             = extents.height;
        image_copy_region.extent.depth              = 1;

        // Issue the copy command
        vkCmdCopyImage(
            cmd_buf->handle(),
            m_tone_map_image->handle(),
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            m_save_to_disk_image->handle(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &image_copy_region);

        vk::utilities::set_image_layout(
            cmd_buf->handle(),
            m_tone_map_image->handle(),
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            subresource_range);

        vk::utilities::set_image_layout(
            cmd_buf->handle(),
            m_save_to_disk_image->handle(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_GENERAL,
            subresource_range);

        m_copy_started = true;
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::on_window_resize()
{
    m_output_image_recreated = true;

    auto backend = m_backend.lock();

    backend->wait_idle();

    create_output_images();
    create_tone_map_framebuffer();
    create_swapchain_framebuffers();
    create_depth_prepass_framebuffer();
    update_dynamic_descriptor_sets();
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::add_ray_debug_view(const glm::ivec2& pixel_coord, const uint32_t& num_debug_rays, const glm::mat4& view, const glm::mat4& projection)
{
    m_ray_debug_views.push_back({ pixel_coord, num_debug_rays, view, projection });
    m_ray_debug_view_added = true;
}

// -----------------------------------------------------------------------------------------------------------------------------------

const std::vector<RayDebugView>& Renderer::ray_debug_views()
{
    return m_ray_debug_views;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::clear_ray_debug_views()
{
    m_ray_debug_views.clear();
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::save_image_to_disk(const std::string& path)
{
    if (path.length() == 0)
    {
        HELIOS_LOG_ERROR("A valid path is required to save an image to disk");
        return;
    }

    m_save_image_to_disk = true;
    m_image_save_path    = path;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::create_tone_map_render_pass()
{
    auto backend = m_backend.lock();

    VkAttachmentDescription attachment;
    HELIOS_ZERO_MEMORY(attachment);

    // Color attachment
    attachment.format         = VK_FORMAT_R8G8B8A8_UNORM;
    attachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference color_reference;
    color_reference.attachment = 0;
    color_reference.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    std::vector<VkSubpassDescription> subpass_description(1);

    subpass_description[0].pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description[0].colorAttachmentCount    = 1;
    subpass_description[0].pColorAttachments       = &color_reference;
    subpass_description[0].pDepthStencilAttachment = nullptr;
    subpass_description[0].inputAttachmentCount    = 0;
    subpass_description[0].pInputAttachments       = nullptr;
    subpass_description[0].preserveAttachmentCount = 0;
    subpass_description[0].pPreserveAttachments    = nullptr;
    subpass_description[0].pResolveAttachments     = nullptr;

    // Subpass dependencies for layout transitions
    std::vector<VkSubpassDependency> dependencies(2);

    dependencies[0].srcSubpass      = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass      = 0;
    dependencies[0].srcStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask   = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass      = 0;
    dependencies[1].dstSubpass      = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask   = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    m_tone_map_render_pass = vk::RenderPass::create(backend, { attachment }, subpass_description, dependencies);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::create_tone_map_framebuffer()
{
    auto backend = m_backend.lock();
    auto extents = backend->swap_chain_extents();

    backend->queue_object_deletion(m_tone_map_framebuffer);

    m_tone_map_framebuffer = vk::Framebuffer::create(backend, m_tone_map_render_pass, { m_tone_map_image_view }, extents.width, extents.height, 1);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::create_depth_prepass_render_pass()
{
    auto backend = m_backend.lock();

    std::vector<VkAttachmentDescription> attachments(1);

    // Depth attachment
    attachments[0].format         = backend->swap_chain_depth_format();
    attachments[0].samples        = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_reference;
    depth_reference.attachment = 0;
    depth_reference.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    std::vector<VkSubpassDescription> subpass_description(1);

    subpass_description[0].pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description[0].colorAttachmentCount    = 0;
    subpass_description[0].pColorAttachments       = nullptr;
    subpass_description[0].pDepthStencilAttachment = &depth_reference;
    subpass_description[0].inputAttachmentCount    = 0;
    subpass_description[0].pInputAttachments       = nullptr;
    subpass_description[0].preserveAttachmentCount = 0;
    subpass_description[0].pPreserveAttachments    = nullptr;
    subpass_description[0].pResolveAttachments     = nullptr;

    // Subpass dependencies for layout transitions
    std::vector<VkSubpassDependency> dependencies(2);

    dependencies[0].srcSubpass      = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass      = 0;
    dependencies[0].srcStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask    = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask   = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass      = 0;
    dependencies[1].dstSubpass      = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask    = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask   = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    m_depth_prepass_renderpass = vk::RenderPass::create(backend, attachments, subpass_description, dependencies);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::create_depth_prepass_framebuffer()
{
    auto backend = m_backend.lock();
    auto extents = backend->swap_chain_extents();

    backend->queue_object_deletion(m_depth_prepass_framebuffer);

    m_depth_prepass_framebuffer = vk::Framebuffer::create(backend, m_depth_prepass_renderpass, { backend->swapchain_depth_image_view() }, extents.width, extents.height, 1);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::create_swapchain_render_pass()
{
    auto backend = m_backend.lock();
    auto extents = backend->swap_chain_extents();

    std::vector<VkAttachmentDescription> attachments(2);

    // Color attachment
    attachments[0].format         = backend->swap_chain_image_format();
    attachments[0].samples        = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // Depth attachment
    attachments[1].format         = backend->swap_chain_depth_format();
    attachments[1].samples        = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachments[1].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments[1].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference color_reference;
    color_reference.attachment = 0;
    color_reference.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_reference;
    depth_reference.attachment = 1;
    depth_reference.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    std::vector<VkSubpassDescription> subpass_description(1);

    subpass_description[0].pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description[0].colorAttachmentCount    = 1;
    subpass_description[0].pColorAttachments       = &color_reference;
    subpass_description[0].pDepthStencilAttachment = &depth_reference;
    subpass_description[0].inputAttachmentCount    = 0;
    subpass_description[0].pInputAttachments       = nullptr;
    subpass_description[0].preserveAttachmentCount = 0;
    subpass_description[0].pPreserveAttachments    = nullptr;
    subpass_description[0].pResolveAttachments     = nullptr;

    // Subpass dependencies for layout transitions
    std::vector<VkSubpassDependency> dependencies(2);

    dependencies[0].srcSubpass      = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass      = 0;
    dependencies[0].srcStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask   = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass      = 0;
    dependencies[1].dstSubpass      = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask   = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    m_swapchain_renderpass = vk::RenderPass::create(backend, attachments, subpass_description, dependencies);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::create_swapchain_framebuffers()
{
    auto backend     = m_backend.lock();
    auto extents     = backend->swap_chain_extents();
    auto image_views = backend->swapchain_image_views();

    m_swapchain_framebuffers.resize(image_views.size());

    std::vector<vk::ImageView::Ptr> views(2);

    views[1] = backend->swapchain_depth_image_view();

    for (int i = 0; i < image_views.size(); i++)
    {
        backend->queue_object_deletion(m_swapchain_framebuffers[i]);

        views[0]                    = image_views[i];
        m_swapchain_framebuffers[i] = vk::Framebuffer::create(backend, m_swapchain_renderpass, views, extents.width, extents.height, 1);
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::create_tone_map_pipeline()
{
    auto backend = m_backend.lock();

    vk::PipelineLayout::Desc pl_desc;

    pl_desc.add_descriptor_set_layout(backend->combined_sampler_descriptor_set_layout());

    pl_desc.add_push_constant_range(VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ToneMapPushConstants));

    m_tone_map_pipeline_layout = vk::PipelineLayout::create(backend, pl_desc);
    m_tone_map_pipeline        = vk::GraphicsPipeline::create_for_post_process(backend, "assets/shader/triangle.vert.spv", "assets/shader/tone_map.frag.spv", m_tone_map_pipeline_layout, m_tone_map_render_pass);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::create_copy_pipeline()
{
    auto backend = m_backend.lock();

    vk::PipelineLayout::Desc ds_desc;

    ds_desc.add_descriptor_set_layout(backend->combined_sampler_descriptor_set_layout());

    m_copy_pipeline_layout = vk::PipelineLayout::create(backend, ds_desc);
    m_copy_pipeline        = vk::GraphicsPipeline::create_for_post_process(backend, "assets/shader/triangle.vert.spv", "assets/shader/copy.frag.spv", m_copy_pipeline_layout, backend->swapchain_render_pass());
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::create_ray_debug_pipeline()
{
    auto backend = m_backend.lock();

    // ---------------------------------------------------------------------------
    // Create shader modules
    // ---------------------------------------------------------------------------

    std::vector<char> spirv;

    vk::ShaderModule::Ptr vs = vk::ShaderModule::create_from_file(backend, "assets/shader/debug_ray.vert.spv");
    vk::ShaderModule::Ptr fs = vk::ShaderModule::create_from_file(backend, "assets/shader/debug_ray.frag.spv");

    vk::GraphicsPipeline::Desc pso_desc;

    pso_desc.add_shader_stage(VK_SHADER_STAGE_VERTEX_BIT, vs, "main")
        .add_shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT, fs, "main");

    // ---------------------------------------------------------------------------
    // Create vertex input state
    // ---------------------------------------------------------------------------

    vk::VertexInputStateDesc vertex_input_state_desc;

    vertex_input_state_desc.add_binding_desc(0, sizeof(RayDebugVertex), VK_VERTEX_INPUT_RATE_VERTEX);

    vertex_input_state_desc.add_attribute_desc(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0);
    vertex_input_state_desc.add_attribute_desc(1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(RayDebugVertex, color));

    pso_desc.set_vertex_input_state(vertex_input_state_desc);

    // ---------------------------------------------------------------------------
    // Create pipeline input assembly state
    // ---------------------------------------------------------------------------

    vk::InputAssemblyStateDesc input_assembly_state_desc;

    input_assembly_state_desc.set_primitive_restart_enable(false);

    // ---------------------------------------------------------------------------
    // Create viewport state
    // ---------------------------------------------------------------------------

    vk::ViewportStateDesc vp_desc;

    vp_desc.add_viewport(0.0f, 0.0f, 1024, 1024, 0.0f, 1.0f)
        .add_scissor(0, 0, 1024, 1024);

    pso_desc.set_viewport_state(vp_desc);

    // ---------------------------------------------------------------------------
    // Create rasterization state
    // ---------------------------------------------------------------------------

    vk::RasterizationStateDesc rs_state;

    rs_state.set_depth_clamp(VK_FALSE)
        .set_rasterizer_discard_enable(VK_FALSE)
        .set_polygon_mode(VK_POLYGON_MODE_FILL)
        .set_line_width(1.0f)
        .set_cull_mode(VK_CULL_MODE_NONE)
        .set_front_face(VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .set_depth_bias(VK_FALSE);

    pso_desc.set_rasterization_state(rs_state);

    // ---------------------------------------------------------------------------
    // Create multisample state
    // ---------------------------------------------------------------------------

    vk::MultisampleStateDesc ms_state;

    ms_state.set_sample_shading_enable(VK_FALSE)
        .set_rasterization_samples(VK_SAMPLE_COUNT_1_BIT);

    pso_desc.set_multisample_state(ms_state);

    // ---------------------------------------------------------------------------
    // Create depth stencil state
    // ---------------------------------------------------------------------------

    vk::DepthStencilStateDesc ds_state;

    ds_state.set_depth_test_enable(VK_TRUE)
        .set_depth_write_enable(VK_TRUE)
        .set_depth_compare_op(VK_COMPARE_OP_LESS)
        .set_depth_bounds_test_enable(VK_FALSE)
        .set_stencil_test_enable(VK_FALSE);

    pso_desc.set_depth_stencil_state(ds_state);

    // ---------------------------------------------------------------------------
    // Create color blend state
    // ---------------------------------------------------------------------------

    vk::ColorBlendAttachmentStateDesc blend_att_desc;

    blend_att_desc.set_color_write_mask(VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT)
        .set_src_color_blend_factor(VK_BLEND_FACTOR_SRC_ALPHA)
        .set_dst_color_blend_Factor(VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA)
        .set_src_alpha_blend_factor(VK_BLEND_FACTOR_ONE)
        .set_dst_alpha_blend_factor(VK_BLEND_FACTOR_ZERO)
        .set_color_blend_op(VK_BLEND_OP_ADD)
        .set_blend_enable(VK_FALSE);

    vk::ColorBlendStateDesc blend_state;

    blend_state.set_logic_op_enable(VK_FALSE)
        .set_logic_op(VK_LOGIC_OP_COPY)
        .set_blend_constants(0.0f, 0.0f, 0.0f, 0.0f)
        .add_attachment(blend_att_desc);

    pso_desc.set_color_blend_state(blend_state);

    // ---------------------------------------------------------------------------
    // Create pipeline layout
    // ---------------------------------------------------------------------------

    vk::PipelineLayout::Desc pl_desc;

    pl_desc.add_push_constant_range(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4));

    m_ray_debug_pipeline_layout = vk::PipelineLayout::create(backend, pl_desc);

    pso_desc.set_pipeline_layout(m_ray_debug_pipeline_layout);

    // ---------------------------------------------------------------------------
    // Create dynamic state
    // ---------------------------------------------------------------------------

    pso_desc.add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
        .add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR);

    pso_desc.set_render_pass(backend->swapchain_render_pass());

    // ---------------------------------------------------------------------------
    // Create line list pipeline
    // ---------------------------------------------------------------------------

    input_assembly_state_desc.set_topology(VK_PRIMITIVE_TOPOLOGY_LINE_LIST);

    pso_desc.set_input_assembly_state(input_assembly_state_desc);

    m_ray_debug_pipeline = vk::GraphicsPipeline::create(backend, pso_desc);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::create_debug_visualization_pipeline()
{
    auto backend = m_backend.lock();

    // ---------------------------------------------------------------------------
    // Create shader modules
    // ---------------------------------------------------------------------------

    std::vector<char> spirv;

    vk::ShaderModule::Ptr vs = vk::ShaderModule::create_from_file(backend, "assets/shader/debug_visualization.vert.spv");
    vk::ShaderModule::Ptr fs = vk::ShaderModule::create_from_file(backend, "assets/shader/debug_visualization.frag.spv");

    vk::GraphicsPipeline::Desc pso_desc;

    pso_desc.add_shader_stage(VK_SHADER_STAGE_VERTEX_BIT, vs, "main")
        .add_shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT, fs, "main");

    // ---------------------------------------------------------------------------
    // Create vertex input state
    // ---------------------------------------------------------------------------

    vk::VertexInputStateDesc vertex_input_state_desc;

    vertex_input_state_desc.add_binding_desc(0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX);

    vertex_input_state_desc.add_attribute_desc(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0);
    vertex_input_state_desc.add_attribute_desc(1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, tex_coord));
    vertex_input_state_desc.add_attribute_desc(2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, normal));
    vertex_input_state_desc.add_attribute_desc(3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, tangent));
    vertex_input_state_desc.add_attribute_desc(4, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, bitangent));

    pso_desc.set_vertex_input_state(vertex_input_state_desc);

    // ---------------------------------------------------------------------------
    // Create pipeline input assembly state
    // ---------------------------------------------------------------------------

    vk::InputAssemblyStateDesc input_assembly_state_desc;

    input_assembly_state_desc.set_primitive_restart_enable(false);

    // ---------------------------------------------------------------------------
    // Create viewport state
    // ---------------------------------------------------------------------------

    vk::ViewportStateDesc vp_desc;

    vp_desc.add_viewport(0.0f, 0.0f, 1024, 1024, 0.0f, 1.0f)
        .add_scissor(0, 0, 1024, 1024);

    pso_desc.set_viewport_state(vp_desc);

    // ---------------------------------------------------------------------------
    // Create rasterization state
    // ---------------------------------------------------------------------------

    vk::RasterizationStateDesc rs_state;

    rs_state.set_depth_clamp(VK_FALSE)
        .set_rasterizer_discard_enable(VK_FALSE)
        .set_polygon_mode(VK_POLYGON_MODE_FILL)
        .set_line_width(1.0f)
        .set_cull_mode(VK_CULL_MODE_NONE)
        .set_front_face(VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .set_depth_bias(VK_FALSE);

    pso_desc.set_rasterization_state(rs_state);

    // ---------------------------------------------------------------------------
    // Create multisample state
    // ---------------------------------------------------------------------------

    vk::MultisampleStateDesc ms_state;

    ms_state.set_sample_shading_enable(VK_FALSE)
        .set_rasterization_samples(VK_SAMPLE_COUNT_1_BIT);

    pso_desc.set_multisample_state(ms_state);

    // ---------------------------------------------------------------------------
    // Create depth stencil state
    // ---------------------------------------------------------------------------

    vk::DepthStencilStateDesc ds_state;

    ds_state.set_depth_test_enable(VK_TRUE)
        .set_depth_write_enable(VK_TRUE)
        .set_depth_compare_op(VK_COMPARE_OP_LESS_OR_EQUAL)
        .set_depth_bounds_test_enable(VK_FALSE)
        .set_stencil_test_enable(VK_FALSE);

    pso_desc.set_depth_stencil_state(ds_state);

    // ---------------------------------------------------------------------------
    // Create color blend state
    // ---------------------------------------------------------------------------

    vk::ColorBlendAttachmentStateDesc blend_att_desc;

    blend_att_desc.set_color_write_mask(VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT)
        .set_src_color_blend_factor(VK_BLEND_FACTOR_SRC_ALPHA)
        .set_dst_color_blend_Factor(VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA)
        .set_src_alpha_blend_factor(VK_BLEND_FACTOR_ONE)
        .set_dst_alpha_blend_factor(VK_BLEND_FACTOR_ZERO)
        .set_color_blend_op(VK_BLEND_OP_ADD)
        .set_blend_enable(VK_FALSE);

    vk::ColorBlendStateDesc blend_state;

    blend_state.set_logic_op_enable(VK_FALSE)
        .set_logic_op(VK_LOGIC_OP_COPY)
        .set_blend_constants(0.0f, 0.0f, 0.0f, 0.0f)
        .add_attachment(blend_att_desc);

    pso_desc.set_color_blend_state(blend_state);

    // ---------------------------------------------------------------------------
    // Create pipeline layout
    // ---------------------------------------------------------------------------

    vk::PipelineLayout::Desc pl_desc;

    pl_desc.add_descriptor_set_layout(backend->scene_descriptor_set_layout());
    pl_desc.add_descriptor_set_layout(backend->buffer_array_descriptor_set_layout());
    pl_desc.add_descriptor_set_layout(backend->combined_sampler_array_descriptor_set_layout());
    pl_desc.add_push_constant_range(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(DebugVisualizationPushConstants));

    m_debug_visualization_pipeline_layout = vk::PipelineLayout::create(backend, pl_desc);

    pso_desc.set_pipeline_layout(m_debug_visualization_pipeline_layout);

    // ---------------------------------------------------------------------------
    // Create dynamic state
    // ---------------------------------------------------------------------------

    pso_desc.add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
        .add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR);

    pso_desc.set_render_pass(backend->swapchain_render_pass());

    // ---------------------------------------------------------------------------
    // Create line list pipeline
    // ---------------------------------------------------------------------------

    input_assembly_state_desc.set_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    pso_desc.set_input_assembly_state(input_assembly_state_desc);

    m_debug_visualization_pipeline = vk::GraphicsPipeline::create(backend, pso_desc);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::create_depth_prepass_pipeline()
{
    auto backend = m_backend.lock();

    // ---------------------------------------------------------------------------
    // Create shader modules
    // ---------------------------------------------------------------------------

    std::vector<char> spirv;

    vk::ShaderModule::Ptr vs = vk::ShaderModule::create_from_file(backend, "assets/shader/depth_prepass.vert.spv");
    vk::ShaderModule::Ptr fs = vk::ShaderModule::create_from_file(backend, "assets/shader/empty.frag.spv");

    vk::GraphicsPipeline::Desc pso_desc;

    pso_desc.add_shader_stage(VK_SHADER_STAGE_VERTEX_BIT, vs, "main")
        .add_shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT, fs, "main");

    // ---------------------------------------------------------------------------
    // Create vertex input state
    // ---------------------------------------------------------------------------

    vk::VertexInputStateDesc vertex_input_state_desc;

    vertex_input_state_desc.add_binding_desc(0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX);

    vertex_input_state_desc.add_attribute_desc(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0);
    vertex_input_state_desc.add_attribute_desc(1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, tex_coord));
    vertex_input_state_desc.add_attribute_desc(2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, normal));
    vertex_input_state_desc.add_attribute_desc(3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, tangent));
    vertex_input_state_desc.add_attribute_desc(4, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, bitangent));

    pso_desc.set_vertex_input_state(vertex_input_state_desc);

    // ---------------------------------------------------------------------------
    // Create pipeline input assembly state
    // ---------------------------------------------------------------------------

    vk::InputAssemblyStateDesc input_assembly_state_desc;

    input_assembly_state_desc.set_primitive_restart_enable(false);

    // ---------------------------------------------------------------------------
    // Create viewport state
    // ---------------------------------------------------------------------------

    vk::ViewportStateDesc vp_desc;

    vp_desc.add_viewport(0.0f, 0.0f, 1024, 1024, 0.0f, 1.0f)
        .add_scissor(0, 0, 1024, 1024);

    pso_desc.set_viewport_state(vp_desc);

    // ---------------------------------------------------------------------------
    // Create rasterization state
    // ---------------------------------------------------------------------------

    vk::RasterizationStateDesc rs_state;

    rs_state.set_depth_clamp(VK_FALSE)
        .set_rasterizer_discard_enable(VK_FALSE)
        .set_polygon_mode(VK_POLYGON_MODE_FILL)
        .set_line_width(1.0f)
        .set_cull_mode(VK_CULL_MODE_NONE)
        .set_front_face(VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .set_depth_bias(VK_FALSE);

    pso_desc.set_rasterization_state(rs_state);

    // ---------------------------------------------------------------------------
    // Create multisample state
    // ---------------------------------------------------------------------------

    vk::MultisampleStateDesc ms_state;

    ms_state.set_sample_shading_enable(VK_FALSE)
        .set_rasterization_samples(VK_SAMPLE_COUNT_1_BIT);

    pso_desc.set_multisample_state(ms_state);

    // ---------------------------------------------------------------------------
    // Create depth stencil state
    // ---------------------------------------------------------------------------

    vk::DepthStencilStateDesc ds_state;

    ds_state.set_depth_test_enable(VK_TRUE)
        .set_depth_write_enable(VK_TRUE)
        .set_depth_compare_op(VK_COMPARE_OP_LESS)
        .set_depth_bounds_test_enable(VK_FALSE)
        .set_stencil_test_enable(VK_FALSE);

    pso_desc.set_depth_stencil_state(ds_state);

    // ---------------------------------------------------------------------------
    // Create color blend state
    // ---------------------------------------------------------------------------

    vk::ColorBlendAttachmentStateDesc blend_att_desc;

    blend_att_desc.set_color_write_mask(VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT)
        .set_src_color_blend_factor(VK_BLEND_FACTOR_SRC_ALPHA)
        .set_dst_color_blend_Factor(VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA)
        .set_src_alpha_blend_factor(VK_BLEND_FACTOR_ONE)
        .set_dst_alpha_blend_factor(VK_BLEND_FACTOR_ZERO)
        .set_color_blend_op(VK_BLEND_OP_ADD)
        .set_blend_enable(VK_FALSE);

    vk::ColorBlendStateDesc blend_state;

    blend_state.set_logic_op_enable(VK_FALSE)
        .set_logic_op(VK_LOGIC_OP_COPY)
        .set_blend_constants(0.0f, 0.0f, 0.0f, 0.0f)
        .add_attachment(blend_att_desc);

    pso_desc.set_color_blend_state(blend_state);

    pso_desc.set_pipeline_layout(m_debug_visualization_pipeline_layout);

    // ---------------------------------------------------------------------------
    // Create dynamic state
    // ---------------------------------------------------------------------------

    pso_desc.add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
        .add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR);

    pso_desc.set_render_pass(m_depth_prepass_renderpass);

    // ---------------------------------------------------------------------------
    // Create line list pipeline
    // ---------------------------------------------------------------------------

    input_assembly_state_desc.set_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    pso_desc.set_input_assembly_state(input_assembly_state_desc);

    m_depth_prepass_pipeline = vk::GraphicsPipeline::create(backend, pso_desc);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::create_ray_debug_buffers()
{
    auto backend = m_backend.lock();

    m_ray_debug_vbo      = vk::Buffer::create(backend, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(RayDebugVertex) * MAX_DEBUG_RAY_DRAW_COUNT * 2, VMA_MEMORY_USAGE_GPU_ONLY, 0);
    m_ray_debug_draw_cmd = vk::Buffer::create(backend, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, sizeof(int32_t) * 4, VMA_MEMORY_USAGE_GPU_ONLY, 0);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::create_output_images()
{
    auto backend = m_backend.lock();
    auto extents = backend->swap_chain_extents();

    for (int i = 0; i < 2; i++)
    {
        backend->queue_object_deletion(m_output_image_views[i]);
        backend->queue_object_deletion(m_output_images[i]);

        m_output_images[i]      = vk::Image::create(backend, VK_IMAGE_TYPE_2D, extents.width, extents.height, 1, 1, 1, VK_FORMAT_R32G32B32A32_SFLOAT, VMA_MEMORY_USAGE_GPU_ONLY, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_SAMPLE_COUNT_1_BIT);
        m_output_image_views[i] = vk::ImageView::create(backend, m_output_images[i], VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT);
    }

    backend->queue_object_deletion(m_tone_map_image_view);
    backend->queue_object_deletion(m_tone_map_image);
    backend->queue_object_deletion(m_save_to_disk_image);

    m_tone_map_image      = vk::Image::create(backend, VK_IMAGE_TYPE_2D, extents.width, extents.height, 1, 1, 1, VK_FORMAT_R8G8B8A8_UNORM, VMA_MEMORY_USAGE_GPU_ONLY, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_SAMPLE_COUNT_1_BIT);
    m_tone_map_image_view = vk::ImageView::create(backend, m_tone_map_image, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT);
    m_save_to_disk_image  = vk::Image::create(backend, VK_IMAGE_TYPE_2D, extents.width, extents.height, 1, 1, 1, VK_FORMAT_R8G8B8A8_UNORM, VMA_MEMORY_USAGE_GPU_TO_CPU, VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_UNDEFINED, 0, nullptr, 0, VK_IMAGE_TILING_LINEAR);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::create_static_descriptor_sets()
{
    auto backend = m_backend.lock();

    std::vector<VkWriteDescriptorSet> write_datas;

    write_datas;

    m_ray_debug_ds = backend->allocate_descriptor_set(backend->ray_debug_descriptor_set_layout());

    VkDescriptorBufferInfo ray_debug_vbo_buffer_info;

    HELIOS_ZERO_MEMORY(ray_debug_vbo_buffer_info);

    ray_debug_vbo_buffer_info.buffer = m_ray_debug_vbo->handle();
    ray_debug_vbo_buffer_info.offset = 0;
    ray_debug_vbo_buffer_info.range  = VK_WHOLE_SIZE;

    VkWriteDescriptorSet ray_debug_vbo_buffer_write_data;

    HELIOS_ZERO_MEMORY(ray_debug_vbo_buffer_write_data);

    ray_debug_vbo_buffer_write_data.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    ray_debug_vbo_buffer_write_data.descriptorCount = 1;
    ray_debug_vbo_buffer_write_data.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    ray_debug_vbo_buffer_write_data.pBufferInfo     = &ray_debug_vbo_buffer_info;
    ray_debug_vbo_buffer_write_data.dstBinding      = 0;
    ray_debug_vbo_buffer_write_data.dstSet          = m_ray_debug_ds->handle();

    write_datas.push_back(ray_debug_vbo_buffer_write_data);

    VkDescriptorBufferInfo ray_debug_draw_args_buffer_info;

    HELIOS_ZERO_MEMORY(ray_debug_draw_args_buffer_info);

    ray_debug_draw_args_buffer_info.buffer = m_ray_debug_draw_cmd->handle();
    ray_debug_draw_args_buffer_info.offset = 0;
    ray_debug_draw_args_buffer_info.range  = VK_WHOLE_SIZE;

    VkWriteDescriptorSet ray_debug_draw_args_write_data;

    HELIOS_ZERO_MEMORY(ray_debug_draw_args_write_data);

    ray_debug_draw_args_write_data.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    ray_debug_draw_args_write_data.descriptorCount = 1;
    ray_debug_draw_args_write_data.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    ray_debug_draw_args_write_data.pBufferInfo     = &ray_debug_draw_args_buffer_info;
    ray_debug_draw_args_write_data.dstBinding      = 1;
    ray_debug_draw_args_write_data.dstSet          = m_ray_debug_ds->handle();

    write_datas.push_back(ray_debug_draw_args_write_data);

    vkUpdateDescriptorSets(backend->device(), write_datas.size(), &write_datas[0], 0, nullptr);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::create_dynamic_descriptor_sets()
{
    auto backend = m_backend.lock();

    for (int i = 0; i < 2; i++)
    {
        m_output_storage_image_ds[i]   = backend->allocate_descriptor_set(backend->image_descriptor_set_layout());
        m_input_combined_sampler_ds[i] = backend->allocate_descriptor_set(backend->combined_sampler_descriptor_set_layout());
    }

    m_tone_map_ds = backend->allocate_descriptor_set(backend->combined_sampler_descriptor_set_layout());
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::update_dynamic_descriptor_sets()
{
    auto backend = m_backend.lock();

    int idx = 0;

    std::vector<VkWriteDescriptorSet>  write_datas;
    std::vector<VkDescriptorImageInfo> image_descriptors;

    write_datas;
    image_descriptors.reserve(5);

    for (int i = 0; i < 2; i++)
    {
        {
            VkDescriptorImageInfo image_info;

            HELIOS_ZERO_MEMORY(image_info);

            image_info.sampler     = nullptr;
            image_info.imageView   = m_output_image_views[i]->handle();
            image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            image_descriptors.push_back(image_info);

            VkWriteDescriptorSet write_data;

            HELIOS_ZERO_MEMORY(write_data);

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

            HELIOS_ZERO_MEMORY(image_info);

            image_info.sampler     = backend->bilinear_sampler()->handle();
            image_info.imageView   = m_output_image_views[i]->handle();
            image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            image_descriptors.push_back(image_info);

            VkWriteDescriptorSet write_data;

            HELIOS_ZERO_MEMORY(write_data);

            write_data.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_data.descriptorCount = 1;
            write_data.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write_data.pImageInfo      = &image_descriptors[idx++];
            write_data.dstBinding      = 0;
            write_data.dstSet          = m_input_combined_sampler_ds[i]->handle();

            write_datas.push_back(write_data);
        }
    }

    VkDescriptorImageInfo image_info;

    HELIOS_ZERO_MEMORY(image_info);

    image_info.sampler     = backend->bilinear_sampler()->handle();
    image_info.imageView   = m_tone_map_image_view->handle();
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    image_descriptors.push_back(image_info);

    VkWriteDescriptorSet write_data;

    HELIOS_ZERO_MEMORY(write_data);

    write_data.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_data.descriptorCount = 1;
    write_data.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write_data.pImageInfo      = &image_descriptors[idx++];
    write_data.dstBinding      = 0;
    write_data.dstSet          = m_tone_map_ds->handle();

    write_datas.push_back(write_data);

    vkUpdateDescriptorSets(backend->device(), write_datas.size(), &write_datas[0], 0, nullptr);
}

// -----------------------------------------------------------------------------------------------------------------------------------
} // namespace helios