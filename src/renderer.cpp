#include <renderer.h>
#include <macros.h>
#include <vk_mem_alloc.h>

namespace lumen
{
// -----------------------------------------------------------------------------------------------------------------------------------

Renderer::Renderer(vk::Backend::Ptr backend) :
    m_backend(backend)
{
    m_tlas_instance_buffer_device = vk::Buffer::create(backend, VK_BUFFER_USAGE_RAY_TRACING_BIT_NV | VK_BUFFER_USAGE_TRANSFER_DST_BIT, sizeof(RTGeometryInstance) * MAX_SCENE_MESH_COUNT, VMA_MEMORY_USAGE_GPU_ONLY, 0);
}

// -----------------------------------------------------------------------------------------------------------------------------------

Renderer::~Renderer()
{
    m_tlas_instance_buffer_device.reset();
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Renderer::render(vk::CommandBuffer::Ptr cmd_buffer, Scene::Ptr scene, RenderState& render_state)
{
    if (render_state.scene_state == SCENE_STATE_HIERARCHY_UPDATED)
    {
        auto& tlas_data = scene->acceleration_structure_data();

        VkBufferCopy copy_region;
        LUMEN_ZERO_MEMORY(copy_region);

        copy_region.dstOffset = 0;
        copy_region.size      = sizeof(RTGeometryInstance) * render_state.meshes.size();

        vkCmdCopyBuffer(cmd_buffer->handle(), tlas_data.instance_buffer_host->handle(), m_tlas_instance_buffer_device->handle(), 1, &copy_region);

        VkMemoryBarrier memory_barrier;
        memory_barrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        memory_barrier.pNext         = nullptr;
        memory_barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV;
        memory_barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV;
        
        vkCmdBuildAccelerationStructureNV(cmd_buffer->handle(), &tlas_data.tlas->info(), m_tlas_instance_buffer_device->handle(), 0, VK_FALSE, tlas_data.tlas->handle(), VK_NULL_HANDLE, m_tlas_scratch_buffer->handle(), 0);

        vkCmdPipelineBarrier(cmd_buffer->handle(), VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, 0, 1, &memory_barrier, 0, 0, 0, 0);
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------
} // namespace lumen