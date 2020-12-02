#include <core/renderer.h>
#include <utility/macros.h>
#include <vk_mem_alloc.h>
#include <core/integrator.h>
#include <imgui.h>
#include <examples/imgui_impl_vulkan.h>
#include <resource/scene.h>

namespace lumen
{
struct RayDebugVertex
{
    glm::vec4 position;
    glm::vec4 color;
};

// -----------------------------------------------------------------------------------------------------------------------------------

Renderer::Renderer(vk::Backend::Ptr backend) :
    m_backend(backend)
{
    create_output_images();
    create_tone_map_pipeline();
    create_buffers();
    create_ray_debug_buffers();
    create_ray_debug_pipeline();
    create_descriptor_sets();
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

    m_ray_debug_vbo.reset();
    m_ray_debug_draw_cmd.reset();
    m_tlas_instance_buffer_device.reset();
    m_ray_debug_ds.reset();
    m_tone_map_pipeline.reset();
    m_tone_map_pipeline_layout.reset();
    m_ray_debug_pipeline.reset();
    m_ray_debug_pipeline_layout.reset();
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

        geometry.sType                                 = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        geometry.geometryType                          = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        geometry.geometry.instances.sType              = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        geometry.geometry.instances.arrayOfPointers    = VK_FALSE;
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
        build_offset_info.firstVertex     = 0;
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
    render_state.m_ray_debug_ds   = m_ray_debug_ds;

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
    {
        integrator->execute(render_state);

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
            integrator->gather_debug_rays(view.pixel_coord, view.num_debug_rays, view.view, view.projection, render_state);
        }
    }

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

    // If any ray debug views were added, render them
    if (m_ray_debug_views.size() > 0)
        render_ray_debug_views(render_state);

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
    vkCmdBindPipeline(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_tone_map_pipeline->handle());

    vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_tone_map_pipeline_layout->handle(), 0, 1, &read_image->handle(), 0, nullptr);

    // Apply tonemapping
    vkCmdDraw(cmd_buf->handle(), 3, 1, 0, 0);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::render_ray_debug_views(RenderState& render_state)
{
    vkCmdBindPipeline(render_state.m_cmd_buffer->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_ray_debug_pipeline->handle());

    const VkDeviceSize offset = 0;

    vkCmdBindVertexBuffers(render_state.m_cmd_buffer->handle(), 0, 1, &m_ray_debug_vbo->handle(), &offset);

    glm::mat4 view_proj = render_state.m_camera->projection_matrix() * render_state.m_camera->view_matrix();
    vkCmdPushConstants(render_state.m_cmd_buffer->handle(), m_ray_debug_pipeline_layout->handle(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &view_proj);

    vkCmdDrawIndirect(render_state.m_cmd_buffer->handle(), m_ray_debug_draw_cmd->handle(), 0, 1, sizeof(uint32_t) * 4);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::on_window_resize()
{
    create_output_images();
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

void Renderer::create_ray_debug_pipeline()
{
    auto backend = m_backend.lock();

    // ---------------------------------------------------------------------------
    // Create shader modules
    // ---------------------------------------------------------------------------

    std::vector<char> spirv;

    vk::ShaderModule::Ptr vs = vk::ShaderModule::create_from_file(backend, "shader/debug_ray.vert.spv");
    vk::ShaderModule::Ptr fs = vk::ShaderModule::create_from_file(backend, "shader/debug_ray.frag.spv");

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

    ds_state.set_depth_test_enable(VK_FALSE)
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

    write_datas;
    image_descriptors.reserve(4);

    m_ray_debug_ds = backend->allocate_descriptor_set(backend->ray_debug_descriptor_set_layout());

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

    VkDescriptorBufferInfo ray_debug_vbo_buffer_info;

    LUMEN_ZERO_MEMORY(ray_debug_vbo_buffer_info);

    ray_debug_vbo_buffer_info.buffer = m_ray_debug_vbo->handle();
    ray_debug_vbo_buffer_info.offset = 0;
    ray_debug_vbo_buffer_info.range  = VK_WHOLE_SIZE;

    VkWriteDescriptorSet ray_debug_vbo_buffer_write_data;

    LUMEN_ZERO_MEMORY(ray_debug_vbo_buffer_write_data);

    ray_debug_vbo_buffer_write_data.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    ray_debug_vbo_buffer_write_data.descriptorCount = 1;
    ray_debug_vbo_buffer_write_data.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    ray_debug_vbo_buffer_write_data.pBufferInfo     = &ray_debug_vbo_buffer_info;
    ray_debug_vbo_buffer_write_data.dstBinding      = 0;
    ray_debug_vbo_buffer_write_data.dstSet          = m_ray_debug_ds->handle();

    write_datas.push_back(ray_debug_vbo_buffer_write_data);

    VkDescriptorBufferInfo ray_debug_draw_args_buffer_info;

    LUMEN_ZERO_MEMORY(ray_debug_draw_args_buffer_info);

    ray_debug_draw_args_buffer_info.buffer = m_ray_debug_draw_cmd->handle();
    ray_debug_draw_args_buffer_info.offset = 0;
    ray_debug_draw_args_buffer_info.range  = VK_WHOLE_SIZE;

    VkWriteDescriptorSet ray_debug_draw_args_write_data;

    LUMEN_ZERO_MEMORY(ray_debug_draw_args_write_data);

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
} // namespace lumen