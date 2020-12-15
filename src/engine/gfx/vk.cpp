#include <gfx/vk.h>
#include <utility/logger.h>
#include <utility/macros.h>
#include <fstream>
#include <gfx/extensions_vk.h>
#include <resource/scene.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

//#define ENABLE_GPU_ASSISTED_VALIDATION

namespace helios
{
namespace vk
{
// -----------------------------------------------------------------------------------------------------------------------------------

const std::vector<const char*> kValidationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

// -----------------------------------------------------------------------------------------------------------------------------------

const char* kDeviceTypes[] = {
    "VK_PHYSICAL_DEVICE_TYPE_OTHER",
    "VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU",
    "VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU",
    "VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU",
    "VK_PHYSICAL_DEVICE_TYPE_CPU"
};

// -----------------------------------------------------------------------------------------------------------------------------------

const char* kVendorNames[] = {
    "Unknown",
    "AMD",
    "IMAGINATION",
    "NVIDIA",
    "ARM",
    "QUALCOMM",
    "INTEL"
};

// -----------------------------------------------------------------------------------------------------------------------------------

const char* get_vendor_name(uint32_t id)
{
    switch (id)
    {
        case 0x1002:
            return kVendorNames[1];
        case 0x1010:
            return kVendorNames[2];
        case 0x10DE:
            return kVendorNames[3];
        case 0x13B5:
            return kVendorNames[4];
        case 0x5143:
            return kVendorNames[5];
        case 0x8086:
            return kVendorNames[6];
        default:
            return kVendorNames[0];
    }
}

#define MAX_DESCRIPTOR_POOL_THREADS 32
#define MAX_COMMAND_THREADS 32
#define MAX_THREAD_LOCAL_COMMAND_BUFFERS 8

struct ThreadLocalCommandBuffers
{
    CommandPool::Ptr   command_pool[Backend::kMaxFramesInFlight];
    CommandBuffer::Ptr command_buffers[Backend::kMaxFramesInFlight][MAX_THREAD_LOCAL_COMMAND_BUFFERS];
    uint32_t           allocated_buffers = 0;

    ThreadLocalCommandBuffers(Backend::Ptr backend, uint32_t queue_family)
    {
        for (int i = 0; i < Backend::kMaxFramesInFlight; i++)
        {
            command_pool[i] = CommandPool::create(backend, queue_family);

            for (int j = 0; j < MAX_THREAD_LOCAL_COMMAND_BUFFERS; j++)
                command_buffers[i][j] = CommandBuffer::create(backend, command_pool[i]);
        }
    }

    ~ThreadLocalCommandBuffers()
    {
    }

    void reset(uint32_t frame_index)
    {
        allocated_buffers = 0;
        command_pool[frame_index]->reset();
    }

    CommandBuffer::Ptr allocate(uint32_t frame_index, bool begin)
    {
        if (allocated_buffers >= MAX_THREAD_LOCAL_COMMAND_BUFFERS)
        {
            HELIOS_LOG_FATAL("(Vulkan) Max thread local command buffer count reached!");
            throw std::runtime_error("(Vulkan) Max thread local command buffer count reached!");
        }

        auto cmd_buf = command_buffers[frame_index][allocated_buffers++];

        if (begin)
        {
            VkCommandBufferBeginInfo begin_info;
            HELIOS_ZERO_MEMORY(begin_info);

            begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

            vkBeginCommandBuffer(cmd_buf->handle(), &begin_info);
        }

        return cmd_buf;
    }
};

std::atomic<uint32_t>                                   g_thread_counter = 0;
thread_local uint32_t                                   g_thread_idx     = g_thread_counter++;
thread_local std::shared_ptr<ThreadLocalCommandBuffers> g_graphics_command_buffers[MAX_COMMAND_THREADS];
thread_local std::shared_ptr<ThreadLocalCommandBuffers> g_compute_command_buffers[MAX_COMMAND_THREADS];
thread_local std::shared_ptr<ThreadLocalCommandBuffers> g_transfer_command_buffers[MAX_COMMAND_THREADS];
thread_local DescriptorPool::Ptr                        g_descriptor_pools[MAX_DESCRIPTOR_POOL_THREADS];

// -----------------------------------------------------------------------------------------------------------------------------------

VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    std::string message_type_str = "General";

    if (messageType == VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
        message_type_str = "Validation";
    else if (messageType == VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
        message_type_str = "Performance";

    if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT || messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
        HELIOS_LOG_INFO("Vulkan - " + message_type_str + " : " + std::string(pCallbackData->pMessage));
    if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        HELIOS_LOG_WARNING("Vulkan -" + message_type_str + " : " + std::string(pCallbackData->pMessage));
    if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        HELIOS_LOG_ERROR("Vulkan - " + message_type_str + " : " + std::string(pCallbackData->pMessage));

    return VK_FALSE;
}

// -----------------------------------------------------------------------------------------------------------------------------------

bool QueueInfos::asynchronous_compute()
{
    return compute_queue_index != graphics_queue_index;
}

// -----------------------------------------------------------------------------------------------------------------------------------

bool QueueInfos::transfer()
{
    return transfer_queue_index != compute_queue_index && transfer_queue_index != graphics_queue_index;
}

// -----------------------------------------------------------------------------------------------------------------------------------

Object::Object(Backend::Ptr backend) :
    m_vk_backend(backend)
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

Image::Ptr Image::create(Backend::Ptr backend, VkImageType type, uint32_t width, uint32_t height, uint32_t depth, uint32_t mip_levels, uint32_t array_size, VkFormat format, VmaMemoryUsage memory_usage, VkImageUsageFlags usage, VkSampleCountFlagBits sample_count, VkImageLayout initial_layout, size_t size, void* data, VkImageCreateFlags flags, VkImageTiling tiling)
{
    return std::shared_ptr<Image>(new Image(backend, type, width, height, depth, mip_levels, array_size, format, memory_usage, usage, sample_count, initial_layout, size, data, flags, tiling));
}

// -----------------------------------------------------------------------------------------------------------------------------------

Image::Ptr Image::create_from_swapchain(Backend::Ptr backend, VkImage image, VkImageType type, uint32_t width, uint32_t height, uint32_t depth, uint32_t mip_levels, uint32_t array_size, VkFormat format, VmaMemoryUsage memory_usage, VkImageUsageFlags usage, VkSampleCountFlagBits sample_count)
{
    return std::shared_ptr<Image>(new Image(backend, image, type, width, height, depth, mip_levels, array_size, format, memory_usage, usage, sample_count));
}

// -----------------------------------------------------------------------------------------------------------------------------------

Image::Image(Backend::Ptr backend, VkImageType type, uint32_t width, uint32_t height, uint32_t depth, uint32_t mip_levels, uint32_t array_size, VkFormat format, VmaMemoryUsage memory_usage, VkImageUsageFlags usage, VkSampleCountFlagBits sample_count, VkImageLayout initial_layout, size_t size, void* data, VkImageCreateFlags flags, VkImageTiling tiling) :
    Object(backend), m_type(type), m_width(width), m_height(height), m_depth(depth), m_mip_levels(mip_levels), m_array_size(array_size), m_format(format), m_memory_usage(memory_usage), m_sample_count(sample_count), m_usage(usage), m_flags(flags), m_tiling(tiling)
{
    m_vma_allocator = backend->allocator();

    if (mip_levels == 0)
        m_mip_levels = static_cast<uint32_t>(std::floor(std::log2(std::max(m_width, m_height)))) + 1;

    VkImageCreateInfo image_info;
    HELIOS_ZERO_MEMORY(image_info);

    image_info.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType     = m_type;
    image_info.extent.width  = m_width;
    image_info.extent.height = m_height;
    image_info.extent.depth  = m_depth;
    image_info.mipLevels     = m_mip_levels;
    image_info.arrayLayers   = m_array_size;
    image_info.format        = m_format;
    image_info.tiling        = tiling;
    image_info.initialLayout = initial_layout;
    image_info.usage         = m_usage;
    image_info.samples       = m_sample_count;
    image_info.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    image_info.flags         = flags;

    VmaAllocationInfo       alloc_info;
    VmaAllocationCreateInfo alloc_create_info;
    HELIOS_ZERO_MEMORY(alloc_create_info);

    alloc_create_info.usage = memory_usage;
    alloc_create_info.flags = (memory_usage == VMA_MEMORY_USAGE_CPU_ONLY || memory_usage == VMA_MEMORY_USAGE_GPU_TO_CPU) ? VMA_ALLOCATION_CREATE_MAPPED_BIT : 0;

    if (vmaCreateImage(m_vma_allocator, &image_info, &alloc_create_info, &m_vk_image, &m_vma_allocation, &alloc_info) != VK_SUCCESS)
    {
        HELIOS_LOG_FATAL("(Vulkan) Failed to create Image.");
        throw std::runtime_error("(Vulkan) Failed to create Image.");
    }

    m_vk_device_memory = alloc_info.deviceMemory;
    m_mapped_ptr       = alloc_info.pMappedData;

    if (data)
    {
        CommandBuffer::Ptr cmd_buf = backend->allocate_graphics_command_buffer(true);

        VkImageSubresourceRange subresource_range;
        HELIOS_ZERO_MEMORY(subresource_range);

        subresource_range.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        subresource_range.baseMipLevel   = 0;
        subresource_range.levelCount     = m_mip_levels;
        subresource_range.layerCount     = m_array_size;
        subresource_range.baseArrayLayer = 0;

        // Image barrier for optimal image (target)
        // Optimal image will be used as destination for the copy
        utilities::set_image_layout(cmd_buf->handle(),
                                    m_vk_image,
                                    VK_IMAGE_LAYOUT_UNDEFINED,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    subresource_range);

        vkEndCommandBuffer(cmd_buf->handle());

        backend->flush_graphics({ cmd_buf });

        upload_data(0, 0, data, size, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        if (m_mip_levels > 1)
            generate_mipmaps();
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

Image::Image(Backend::Ptr backend, VkImage image, VkImageType type, uint32_t width, uint32_t height, uint32_t depth, uint32_t mip_levels, uint32_t array_size, VkFormat format, VmaMemoryUsage memory_usage, VkImageUsageFlags usage, VkSampleCountFlagBits sample_count) :
    Object(backend), m_vk_image(image), m_type(type), m_width(width), m_height(height), m_depth(depth), m_mip_levels(mip_levels), m_array_size(array_size), m_format(format), m_memory_usage(memory_usage), m_sample_count(sample_count)
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

Image::~Image()
{
    if (m_vk_backend.expired())
    {
        HELIOS_LOG_FATAL("(Vulkan) Destructing after Device.");
        throw std::runtime_error("(Vulkan) Destructing after Device.");
    }

    if (m_vma_allocator && m_vma_allocation)
        vmaDestroyImage(m_vma_allocator, m_vk_image, m_vma_allocation);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Image::upload_data(int array_index, int mip_level, void* data, size_t size, VkImageLayout src_layout, VkImageLayout dst_layout)
{
    auto backend = m_vk_backend.lock();

    Buffer::Ptr staging = Buffer::create(backend, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, size, VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT, data);

    VkBufferImageCopy buffer_copy_region;
    HELIOS_ZERO_MEMORY(buffer_copy_region);

    buffer_copy_region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    buffer_copy_region.imageSubresource.mipLevel       = mip_level;
    buffer_copy_region.imageSubresource.baseArrayLayer = array_index;
    buffer_copy_region.imageSubresource.layerCount     = 1;
    buffer_copy_region.imageExtent.width               = m_width;
    buffer_copy_region.imageExtent.height              = m_height;
    buffer_copy_region.imageExtent.depth               = 1;
    buffer_copy_region.bufferOffset                    = 0;

    VkImageSubresourceRange subresource_range;
    HELIOS_ZERO_MEMORY(subresource_range);

    subresource_range.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    subresource_range.baseMipLevel   = mip_level;
    subresource_range.levelCount     = 1;
    subresource_range.layerCount     = 1;
    subresource_range.baseArrayLayer = array_index;

    CommandBuffer::Ptr cmd_buf = backend->allocate_graphics_command_buffer(true);

    if (src_layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        // Image barrier for optimal image (target)
        // Optimal image will be used as destination for the copy
        utilities::set_image_layout(cmd_buf->handle(),
                                    m_vk_image,
                                    src_layout,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    subresource_range);
    }

    // Copy mip levels from staging buffer
    vkCmdCopyBufferToImage(cmd_buf->handle(),
                           staging->handle(),
                           m_vk_image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1,
                           &buffer_copy_region);

    if (dst_layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        // Change texture image layout to shader read after all mip levels have been copied
        utilities::set_image_layout(cmd_buf->handle(),
                                    m_vk_image,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    dst_layout,
                                    subresource_range);
    }

    vkEndCommandBuffer(cmd_buf->handle());

    backend->flush_graphics({ cmd_buf });
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Image::generate_mipmaps(VkImageLayout src_layout, VkImageLayout dst_layout)
{
    auto backend = m_vk_backend.lock();

    CommandBuffer::Ptr cmd_buf = backend->allocate_graphics_command_buffer(true);

    VkImageSubresourceRange subresource_range;
    HELIOS_ZERO_MEMORY(subresource_range);

    subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresource_range.levelCount = 1;
    subresource_range.layerCount = 1;

    int32_t mip_width  = m_width;
    int32_t mip_height = m_height;

    for (int arr_idx = 0; arr_idx < m_array_size; arr_idx++)
    {
        for (int mip_idx = 1; mip_idx < m_mip_levels; mip_idx++)
        {
            subresource_range.baseMipLevel   = mip_idx - 1;
            subresource_range.baseArrayLayer = arr_idx;

            VkImageLayout layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

            if (mip_idx == 1)
                layout = src_layout;

            utilities::set_image_layout(cmd_buf->handle(),
                                        m_vk_image,
                                        layout,
                                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                        subresource_range);

            VkImageBlit blit                   = {};
            blit.srcOffsets[0]                 = { 0, 0, 0 };
            blit.srcOffsets[1]                 = { mip_width, mip_height, 1 };
            blit.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel       = mip_idx - 1;
            blit.srcSubresource.baseArrayLayer = arr_idx;
            blit.srcSubresource.layerCount     = 1;
            blit.dstOffsets[0]                 = { 0, 0, 0 };
            blit.dstOffsets[1]                 = { mip_width > 1 ? mip_width / 2 : 1, mip_height > 1 ? mip_height / 2 : 1, 1 };
            blit.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel       = mip_idx;
            blit.dstSubresource.baseArrayLayer = arr_idx;
            blit.dstSubresource.layerCount     = 1;

            vkCmdBlitImage(cmd_buf->handle(),
                           m_vk_image,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           m_vk_image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1,
                           &blit,
                           VK_FILTER_LINEAR);

            utilities::set_image_layout(cmd_buf->handle(),
                                        m_vk_image,
                                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                        dst_layout,
                                        subresource_range);

            if (mip_width > 1) mip_width /= 2;
            if (mip_height > 1) mip_height /= 2;
        }

        subresource_range.baseMipLevel = m_mip_levels - 1;

        utilities::set_image_layout(cmd_buf->handle(),
                                    m_vk_image,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    dst_layout,
                                    subresource_range);
    }

    vkEndCommandBuffer(cmd_buf->handle());

    backend->flush_graphics({ cmd_buf });
}

// -----------------------------------------------------------------------------------------------------------------------------------

ImageView::Ptr ImageView::create(Backend::Ptr backend, Image::Ptr image, VkImageViewType view_type, VkImageAspectFlags aspect_flags, uint32_t base_mip_level, uint32_t level_count, uint32_t base_array_layer, uint32_t layer_count)
{
    return std::shared_ptr<ImageView>(new ImageView(backend, image, view_type, aspect_flags, base_mip_level, level_count, base_array_layer, layer_count));
}

// -----------------------------------------------------------------------------------------------------------------------------------

ImageView::ImageView(Backend::Ptr backend, Image::Ptr image, VkImageViewType view_type, VkImageAspectFlags aspect_flags, uint32_t base_mip_level, uint32_t level_count, uint32_t base_array_layer, uint32_t layer_count) :
    Object(backend)
{
    VkImageViewCreateInfo info;
    HELIOS_ZERO_MEMORY(info);

    info.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.image                           = image->handle();
    info.viewType                        = view_type;
    info.format                          = image->format();
    info.subresourceRange.aspectMask     = aspect_flags;
    info.subresourceRange.baseMipLevel   = base_mip_level;
    info.subresourceRange.levelCount     = level_count;
    info.subresourceRange.baseArrayLayer = base_array_layer;
    info.subresourceRange.layerCount     = layer_count;

    if (vkCreateImageView(backend->device(), &info, nullptr, &m_vk_image_view) != VK_SUCCESS)
    {
        HELIOS_LOG_FATAL("(Vulkan) Failed to create Image View.");
        throw std::runtime_error("(Vulkan) Failed to create Image View.");
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

ImageView::~ImageView()
{
    if (m_vk_backend.expired())
    {
        HELIOS_LOG_FATAL("(Vulkan) Destructing after Device.");
        throw std::runtime_error("(Vulkan) Destructing after Device.");
    }

    auto backend = m_vk_backend.lock();

    vkDestroyImageView(backend->device(), m_vk_image_view, nullptr);
}

// -----------------------------------------------------------------------------------------------------------------------------------

RenderPass::Ptr RenderPass::create(Backend::Ptr backend, std::vector<VkAttachmentDescription> attachment_descs, std::vector<VkSubpassDescription> subpass_descs, std::vector<VkSubpassDependency> subpass_deps)
{
    return std::shared_ptr<RenderPass>(new RenderPass(backend, attachment_descs, subpass_descs, subpass_deps));
}

// -----------------------------------------------------------------------------------------------------------------------------------

RenderPass::RenderPass(Backend::Ptr backend, std::vector<VkAttachmentDescription> attachment_descs, std::vector<VkSubpassDescription> subpass_descs, std::vector<VkSubpassDependency> subpass_deps) :
    Object(backend)
{
    VkRenderPassCreateInfo render_pass_info;
    HELIOS_ZERO_MEMORY(render_pass_info);

    render_pass_info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = attachment_descs.size();
    render_pass_info.pAttachments    = attachment_descs.data();
    render_pass_info.subpassCount    = subpass_descs.size();
    render_pass_info.pSubpasses      = subpass_descs.data();
    render_pass_info.dependencyCount = subpass_deps.size();
    render_pass_info.pDependencies   = subpass_deps.data();

    if (vkCreateRenderPass(backend->device(), &render_pass_info, nullptr, &m_vk_render_pass) != VK_SUCCESS)
    {
        HELIOS_LOG_FATAL("(Vulkan) Failed to create Render Pass.");
        throw std::runtime_error("(Vulkan) Failed to create Render Pass.");
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

RenderPass::~RenderPass()
{
    if (m_vk_backend.expired())
    {
        HELIOS_LOG_FATAL("(Vulkan) Destructing after Device.");
        throw std::runtime_error("(Vulkan) Destructing after Device.");
    }

    auto backend = m_vk_backend.lock();

    vkDestroyRenderPass(backend->device(), m_vk_render_pass, nullptr);
}

// -----------------------------------------------------------------------------------------------------------------------------------

Framebuffer::Ptr Framebuffer::create(Backend::Ptr backend, RenderPass::Ptr render_pass, std::vector<ImageView::Ptr> views, uint32_t width, uint32_t height, uint32_t layers)
{
    return std::shared_ptr<Framebuffer>(new Framebuffer(backend, render_pass, views, width, height, layers));
}

// -----------------------------------------------------------------------------------------------------------------------------------

Framebuffer::Framebuffer(Backend::Ptr backend, RenderPass::Ptr render_pass, std::vector<ImageView::Ptr> views, uint32_t width, uint32_t height, uint32_t layers) :
    Object(backend)
{
    std::vector<VkImageView> attachments(views.size());

    for (int i = 0; i < attachments.size(); i++)
        attachments[i] = views[i]->handle();

    VkFramebufferCreateInfo frameBuffer_create_info;
    HELIOS_ZERO_MEMORY(frameBuffer_create_info);

    frameBuffer_create_info.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    frameBuffer_create_info.pNext           = NULL;
    frameBuffer_create_info.renderPass      = render_pass->handle();
    frameBuffer_create_info.attachmentCount = views.size();
    frameBuffer_create_info.pAttachments    = attachments.data();
    frameBuffer_create_info.width           = width;
    frameBuffer_create_info.height          = height;
    frameBuffer_create_info.layers          = layers;

    if (vkCreateFramebuffer(backend->device(), &frameBuffer_create_info, nullptr, &m_vk_framebuffer) != VK_SUCCESS)
    {
        HELIOS_LOG_FATAL("(Vulkan) Failed to create Framebuffer.");
        throw std::runtime_error("(Vulkan) Failed to create Framebuffer.");
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

Framebuffer::~Framebuffer()
{
    if (m_vk_backend.expired())
    {
        HELIOS_LOG_FATAL("(Vulkan) Destructing after Device.");
        throw std::runtime_error("(Vulkan) Destructing after Device.");
    }

    auto backend = m_vk_backend.lock();

    vkDestroyFramebuffer(backend->device(), m_vk_framebuffer, nullptr);
}

// -----------------------------------------------------------------------------------------------------------------------------------

Buffer::Ptr Buffer::create(Backend::Ptr backend, VkBufferUsageFlags usage, size_t size, VmaMemoryUsage memory_usage, VkFlags create_flags, void* data)
{
    return std::shared_ptr<Buffer>(new Buffer(backend, usage, size, memory_usage, create_flags, data));
}

// -----------------------------------------------------------------------------------------------------------------------------------

Buffer::Buffer(Backend::Ptr backend, VkBufferUsageFlags usage, size_t size, VmaMemoryUsage memory_usage, VkFlags create_flags, void* data) :
    Object(backend), m_size(size), m_vma_memory_usage(memory_usage)
{
    m_vma_allocator = backend->allocator();

    VkBufferCreateInfo buffer_info;
    HELIOS_ZERO_MEMORY(buffer_info);

    buffer_info.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size                  = size;
    buffer_info.usage                 = usage;
    buffer_info.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    buffer_info.queueFamilyIndexCount = 0;
    buffer_info.pQueueFamilyIndices   = nullptr;

    VkMemoryPropertyFlags memory_prop_flags = 0;
    VkBufferUsageFlags    usage_flags       = usage;

    if (memory_usage == VMA_MEMORY_USAGE_CPU_ONLY)
    {
        memory_prop_flags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        memory_prop_flags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        usage_flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    }
    else if (memory_usage == VMA_MEMORY_USAGE_GPU_ONLY)
    {
        memory_prop_flags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        usage_flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }
    else if (memory_usage == VMA_MEMORY_USAGE_CPU_TO_GPU)
    {
        memory_prop_flags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        memory_prop_flags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }
    else if (memory_usage == VMA_MEMORY_USAGE_GPU_TO_CPU)
        memory_prop_flags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

    m_vk_memory_property = memory_prop_flags;
    m_vk_usage_flags     = usage_flags;

    buffer_info.usage = m_vk_usage_flags;

    VmaAllocationInfo vma_alloc_info;

    VmaAllocationCreateInfo alloc_create_info;
    HELIOS_ZERO_MEMORY(alloc_create_info);

    alloc_create_info.usage          = memory_usage;
    alloc_create_info.flags          = create_flags;
    alloc_create_info.requiredFlags  = memory_prop_flags;
    alloc_create_info.preferredFlags = 0;
    alloc_create_info.memoryTypeBits = 0;
    alloc_create_info.pool           = VK_NULL_HANDLE;

    if (vmaCreateBuffer(m_vma_allocator, &buffer_info, &alloc_create_info, &m_vk_buffer, &m_vma_allocation, &vma_alloc_info) != VK_SUCCESS)
    {
        HELIOS_LOG_FATAL("(Vulkan) Failed to create Buffer.");
        throw std::runtime_error("(Vulkan) Failed to create Buffer.");
    }

    m_vk_device_memory = vma_alloc_info.deviceMemory;

    if (create_flags & VMA_ALLOCATION_CREATE_MAPPED_BIT)
        m_mapped_ptr = vma_alloc_info.pMappedData;

    if (data)
        upload_data(data, size, 0);

    VkBufferDeviceAddressInfoKHR address_info;
    HELIOS_ZERO_MEMORY(address_info);

    address_info.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR;
    address_info.buffer = m_vk_buffer;

    if ((usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) == VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
        m_device_address = vkGetBufferDeviceAddress(backend->device(), &address_info);
}

// -----------------------------------------------------------------------------------------------------------------------------------

Buffer::~Buffer()
{
    vmaDestroyBuffer(m_vma_allocator, m_vk_buffer, m_vma_allocation);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Buffer::upload_data(void* data, size_t size, size_t offset)
{
    auto backend = m_vk_backend.lock();

    if (m_vma_memory_usage == VMA_MEMORY_USAGE_GPU_ONLY)
    {
        // Create VMA_MEMORY_USAGE_CPU_ONLY staging buffer and perfom Buffer-to-Buffer copy
        Buffer::Ptr staging = Buffer::create(backend, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, size, VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT, data);

        CommandBuffer::Ptr cmd_buf = backend->allocate_graphics_command_buffer();

        VkCommandBufferBeginInfo begin_info;
        HELIOS_ZERO_MEMORY(begin_info);

        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        vkBeginCommandBuffer(cmd_buf->handle(), &begin_info);

        VkBufferCopy copy_region;
        HELIOS_ZERO_MEMORY(copy_region);

        copy_region.dstOffset = offset;
        copy_region.size      = size;

        vkCmdCopyBuffer(cmd_buf->handle(), staging->handle(), m_vk_buffer, 1, &copy_region);

        vkEndCommandBuffer(cmd_buf->handle());

        backend->flush_graphics({ cmd_buf });
    }
    else
    {
        if (!m_mapped_ptr)
            vkMapMemory(backend->device(), m_vk_device_memory, 0, size, 0, &m_mapped_ptr);

        memcpy(m_mapped_ptr, data, size);

        // If host coherency hasn't been requested, do a manual flush to make writes visible
        if ((m_vk_memory_property & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
        {
            VkMappedMemoryRange mapped_range;
            HELIOS_ZERO_MEMORY(mapped_range);

            mapped_range.sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
            mapped_range.memory = m_vk_device_memory;
            mapped_range.offset = 0;
            mapped_range.size   = VK_WHOLE_SIZE;

            vkFlushMappedMemoryRanges(backend->device(), 1, &mapped_range);
        }
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

CommandPool::Ptr CommandPool::create(Backend::Ptr backend, uint32_t queue_family_index)
{
    return std::shared_ptr<CommandPool>(new CommandPool(backend, queue_family_index));
}

// -----------------------------------------------------------------------------------------------------------------------------------

CommandPool::CommandPool(Backend::Ptr backend, uint32_t queue_family_index) :
    Object(backend)
{
    VkCommandPoolCreateInfo pool_info;
    HELIOS_ZERO_MEMORY(pool_info);

    pool_info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = queue_family_index;

    if (vkCreateCommandPool(backend->device(), &pool_info, nullptr, &m_vk_pool) != VK_SUCCESS)
    {
        HELIOS_LOG_FATAL("(Vulkan) Failed to create Command Pool.");
        throw std::runtime_error("(Vulkan) Failed to create Command Pool.");
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

CommandPool::~CommandPool()
{
    if (m_vk_backend.expired())
    {
        HELIOS_LOG_FATAL("(Vulkan) Destructing after Device.");
        throw std::runtime_error("(Vulkan) Destructing after Device.");
    }

    auto backend = m_vk_backend.lock();

    vkDestroyCommandPool(backend->device(), m_vk_pool, nullptr);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void CommandPool::reset()
{
    auto backend = m_vk_backend.lock();

    if (vkResetCommandPool(backend->device(), m_vk_pool, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT) != VK_SUCCESS)
    {
        HELIOS_LOG_FATAL("(Vulkan) Failed to reset Command Pool.");
        throw std::runtime_error("(Vulkan) Failed to reset Command Pool.");
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

CommandBuffer::CommandBuffer(Backend::Ptr backend, CommandPool::Ptr pool) :
    Object(backend)
{
    m_vk_pool = pool;

    VkCommandBufferAllocateInfo alloc_info;
    HELIOS_ZERO_MEMORY(alloc_info);

    alloc_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool        = pool->handle();
    alloc_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(backend->device(), &alloc_info, &m_vk_command_buffer) != VK_SUCCESS)
    {
        HELIOS_LOG_FATAL("(Vulkan) Failed to allocate Command Buffer.");
        throw std::runtime_error("(Vulkan) Failed to allocate Command Buffer.");
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

CommandBuffer::Ptr CommandBuffer::create(Backend::Ptr backend, CommandPool::Ptr pool)
{
    return std::shared_ptr<CommandBuffer>(new CommandBuffer(backend, pool));
}

// -----------------------------------------------------------------------------------------------------------------------------------

CommandBuffer::~CommandBuffer()
{
    if (m_vk_backend.expired() || m_vk_pool.expired())
    {
        HELIOS_LOG_FATAL("(Vulkan) Destructing after Device.");
        throw std::runtime_error("(Vulkan) Destructing after Device.");
    }

    auto backend = m_vk_backend.lock();
    auto pool    = m_vk_pool.lock();

    vkFreeCommandBuffers(backend->device(), pool->handle(), 1, &m_vk_command_buffer);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void CommandBuffer::reset()
{
    vkResetCommandBuffer(m_vk_command_buffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
}

// -----------------------------------------------------------------------------------------------------------------------------------

ShaderModule::Ptr ShaderModule::create_from_file(Backend::Ptr backend, std::string path)
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);

    if (!file.is_open())
        throw std::runtime_error("Failed to open SPIRV shader!");

    size_t            file_size = (size_t)file.tellg();
    std::vector<char> buffer(file_size);

    file.seekg(0);
    file.read(buffer.data(), file_size);

    file.close();

    return std::shared_ptr<ShaderModule>(new ShaderModule(backend, buffer));
}

// -----------------------------------------------------------------------------------------------------------------------------------

ShaderModule::Ptr ShaderModule::create(Backend::Ptr backend, std::vector<char> spirv)
{
    return std::shared_ptr<ShaderModule>(new ShaderModule(backend, spirv));
}

// -----------------------------------------------------------------------------------------------------------------------------------

ShaderModule::ShaderModule(Backend::Ptr backend, std::vector<char> spirv) :
    Object(backend)
{
    VkShaderModuleCreateInfo create_info;
    HELIOS_ZERO_MEMORY(create_info);

    create_info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = spirv.size();
    create_info.pCode    = reinterpret_cast<const uint32_t*>(spirv.data());

    if (vkCreateShaderModule(backend->device(), &create_info, nullptr, &m_vk_module) != VK_SUCCESS)
    {
        HELIOS_LOG_FATAL("(Vulkan) Failed to create shader module.");
        throw std::runtime_error("(Vulkan) Failed to create shader module.");
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

ShaderModule::~ShaderModule()
{
    if (m_vk_backend.expired())
    {
        HELIOS_LOG_FATAL("(Vulkan) Destructing after Device.");
        throw std::runtime_error("(Vulkan) Destructing after Device.");
    }

    auto backend = m_vk_backend.lock();

    vkDestroyShaderModule(backend->device(), m_vk_module, nullptr);
}

// -----------------------------------------------------------------------------------------------------------------------------------

VertexInputStateDesc::VertexInputStateDesc()
{
    HELIOS_ZERO_MEMORY(create_info);

    create_info.sType                        = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    create_info.pVertexAttributeDescriptions = &attribute_desc[0];
    create_info.pVertexBindingDescriptions   = &binding_desc[0];
}

// -----------------------------------------------------------------------------------------------------------------------------------

VertexInputStateDesc& VertexInputStateDesc::add_binding_desc(uint32_t binding, uint32_t stride, VkVertexInputRate input_rate)
{
    binding_desc[create_info.vertexBindingDescriptionCount++] = { binding, stride, input_rate };
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

VertexInputStateDesc& VertexInputStateDesc::add_attribute_desc(uint32_t location, uint32_t binding, VkFormat format, uint32_t offset)
{
    attribute_desc[create_info.vertexAttributeDescriptionCount++] = { location, binding, format, offset };
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

InputAssemblyStateDesc::InputAssemblyStateDesc()
{
    HELIOS_ZERO_MEMORY(create_info);

    create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
}

// -----------------------------------------------------------------------------------------------------------------------------------

InputAssemblyStateDesc& InputAssemblyStateDesc::set_flags(VkPipelineInputAssemblyStateCreateFlags flags)
{
    create_info.flags = flags;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

InputAssemblyStateDesc& InputAssemblyStateDesc::set_topology(VkPrimitiveTopology topology)
{
    create_info.topology = topology;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

InputAssemblyStateDesc& InputAssemblyStateDesc::set_primitive_restart_enable(bool primitive_restart_enable)
{
    create_info.primitiveRestartEnable = primitive_restart_enable;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

TessellationStateDesc::TessellationStateDesc()
{
    HELIOS_ZERO_MEMORY(create_info);

    create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
}

// -----------------------------------------------------------------------------------------------------------------------------------

TessellationStateDesc& TessellationStateDesc::set_flags(VkPipelineTessellationStateCreateFlags flags)
{
    create_info.flags = flags;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

TessellationStateDesc& TessellationStateDesc::set_patch_control_points(uint32_t patch_control_points)
{
    create_info.patchControlPoints = patch_control_points;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

RasterizationStateDesc::RasterizationStateDesc()
{
    HELIOS_ZERO_MEMORY(create_info);
    HELIOS_ZERO_MEMORY(conservative_raster_create_info);

    create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;

    conservative_raster_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT;
}

// -----------------------------------------------------------------------------------------------------------------------------------

RasterizationStateDesc& RasterizationStateDesc::set_depth_clamp(VkBool32 value)
{
    create_info.depthClampEnable = value;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

RasterizationStateDesc& RasterizationStateDesc::set_rasterizer_discard_enable(VkBool32 value)
{
    create_info.rasterizerDiscardEnable = value;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

RasterizationStateDesc& RasterizationStateDesc::set_polygon_mode(VkPolygonMode value)
{
    create_info.polygonMode = value;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

RasterizationStateDesc& RasterizationStateDesc::set_cull_mode(VkCullModeFlags value)
{
    create_info.cullMode = value;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

RasterizationStateDesc& RasterizationStateDesc::set_front_face(VkFrontFace value)
{
    create_info.frontFace = value;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

RasterizationStateDesc& RasterizationStateDesc::set_depth_bias(VkBool32 value)
{
    create_info.depthBiasEnable = value;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

RasterizationStateDesc& RasterizationStateDesc::set_depth_bias_constant_factor(float value)
{
    create_info.depthBiasConstantFactor = value;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

RasterizationStateDesc& RasterizationStateDesc::set_depth_bias_clamp(float value)
{
    create_info.depthBiasClamp = value;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

RasterizationStateDesc& RasterizationStateDesc::set_depth_bias_slope_factor(float value)
{
    create_info.depthBiasSlopeFactor = value;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

RasterizationStateDesc& RasterizationStateDesc::set_line_width(float value)
{
    create_info.lineWidth = value;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

RasterizationStateDesc& RasterizationStateDesc::set_conservative_raster_mode(VkConservativeRasterizationModeEXT value)
{
    if (value != VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT)
        create_info.pNext = &conservative_raster_create_info;

    conservative_raster_create_info.conservativeRasterizationMode = value;

    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

RasterizationStateDesc& RasterizationStateDesc::set_extra_primitive_overestimation_size(float value)
{
    conservative_raster_create_info.extraPrimitiveOverestimationSize = value;

    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

MultisampleStateDesc::MultisampleStateDesc()
{
    HELIOS_ZERO_MEMORY(create_info);

    create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
}

// -----------------------------------------------------------------------------------------------------------------------------------

MultisampleStateDesc& MultisampleStateDesc::set_rasterization_samples(VkSampleCountFlagBits value)
{
    create_info.rasterizationSamples = value;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

MultisampleStateDesc& MultisampleStateDesc::set_sample_shading_enable(VkBool32 value)
{
    create_info.sampleShadingEnable = value;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

MultisampleStateDesc& MultisampleStateDesc::set_min_sample_shading(float value)
{
    create_info.minSampleShading = value;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

MultisampleStateDesc& MultisampleStateDesc::set_sample_mask(VkSampleMask* value)
{
    create_info.pSampleMask = value;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

MultisampleStateDesc& MultisampleStateDesc::set_alpha_to_coverage_enable(VkBool32 value)
{
    create_info.alphaToCoverageEnable = value;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

MultisampleStateDesc& MultisampleStateDesc::set_alpha_to_one_enable(VkBool32 value)
{
    create_info.alphaToOneEnable = value;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

StencilOpStateDesc& StencilOpStateDesc::set_fail_op(VkStencilOp value)
{
    create_info.failOp = value;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

StencilOpStateDesc& StencilOpStateDesc::set_pass_op(VkStencilOp value)
{
    create_info.passOp = value;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

StencilOpStateDesc& StencilOpStateDesc::set_depth_fail_op(VkStencilOp value)
{
    create_info.depthFailOp = value;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

StencilOpStateDesc& StencilOpStateDesc::set_compare_op(VkCompareOp value)
{
    create_info.compareOp = value;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

StencilOpStateDesc& StencilOpStateDesc::set_compare_mask(uint32_t value)
{
    create_info.compareMask = value;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

StencilOpStateDesc& StencilOpStateDesc::set_write_mask(uint32_t value)
{
    create_info.writeMask = value;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

StencilOpStateDesc& StencilOpStateDesc::set_reference(uint32_t value)
{
    create_info.reference = value;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

DepthStencilStateDesc::DepthStencilStateDesc()
{
    HELIOS_ZERO_MEMORY(create_info);

    create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
}

// -----------------------------------------------------------------------------------------------------------------------------------

DepthStencilStateDesc& DepthStencilStateDesc::set_depth_test_enable(VkBool32 value)
{
    create_info.depthTestEnable = value;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

DepthStencilStateDesc& DepthStencilStateDesc::set_depth_write_enable(VkBool32 value)
{
    create_info.depthWriteEnable = value;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

DepthStencilStateDesc& DepthStencilStateDesc::set_depth_compare_op(VkCompareOp value)
{
    create_info.depthCompareOp = value;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

DepthStencilStateDesc& DepthStencilStateDesc::set_depth_bounds_test_enable(VkBool32 value)
{
    create_info.depthBoundsTestEnable = value;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

DepthStencilStateDesc& DepthStencilStateDesc::set_stencil_test_enable(VkBool32 value)
{
    create_info.stencilTestEnable = value;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

DepthStencilStateDesc& DepthStencilStateDesc::set_front(StencilOpStateDesc value)
{
    create_info.front = value.create_info;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

DepthStencilStateDesc& DepthStencilStateDesc::set_back(StencilOpStateDesc value)
{
    create_info.back = value.create_info;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

DepthStencilStateDesc& DepthStencilStateDesc::set_min_depth_bounds(float value)
{
    create_info.minDepthBounds = value;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

DepthStencilStateDesc& DepthStencilStateDesc::set_max_depth_bounds(float value)
{
    create_info.maxDepthBounds = value;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

ColorBlendAttachmentStateDesc::ColorBlendAttachmentStateDesc()
{
    HELIOS_ZERO_MEMORY(create_info);
}

// -----------------------------------------------------------------------------------------------------------------------------------

ColorBlendAttachmentStateDesc& ColorBlendAttachmentStateDesc::set_blend_enable(VkBool32 value)
{
    create_info.blendEnable = value;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

ColorBlendAttachmentStateDesc& ColorBlendAttachmentStateDesc::set_src_color_blend_factor(VkBlendFactor value)
{
    create_info.srcColorBlendFactor = value;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

ColorBlendAttachmentStateDesc& ColorBlendAttachmentStateDesc::set_dst_color_blend_Factor(VkBlendFactor value)
{
    create_info.dstColorBlendFactor = value;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

ColorBlendAttachmentStateDesc& ColorBlendAttachmentStateDesc::set_color_blend_op(VkBlendOp value)
{
    create_info.colorBlendOp = value;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

ColorBlendAttachmentStateDesc& ColorBlendAttachmentStateDesc::set_src_alpha_blend_factor(VkBlendFactor value)
{
    create_info.srcAlphaBlendFactor = value;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

ColorBlendAttachmentStateDesc& ColorBlendAttachmentStateDesc::set_dst_alpha_blend_factor(VkBlendFactor value)
{
    create_info.dstAlphaBlendFactor = value;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

ColorBlendAttachmentStateDesc& ColorBlendAttachmentStateDesc::set_alpha_blend_op(VkBlendOp value)
{
    create_info.alphaBlendOp = value;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

ColorBlendAttachmentStateDesc& ColorBlendAttachmentStateDesc::set_color_write_mask(VkColorComponentFlags value)
{
    create_info.colorWriteMask = value;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

ColorBlendStateDesc::ColorBlendStateDesc()
{
    HELIOS_ZERO_MEMORY(create_info);

    create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
}

// -----------------------------------------------------------------------------------------------------------------------------------

ColorBlendStateDesc& ColorBlendStateDesc::set_logic_op_enable(VkBool32 value)
{
    create_info.logicOpEnable = value;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

ColorBlendStateDesc& ColorBlendStateDesc::set_logic_op(VkLogicOp value)
{
    create_info.logicOp = value;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

ColorBlendStateDesc& ColorBlendStateDesc::add_attachment(ColorBlendAttachmentStateDesc att)
{
    if (create_info.attachmentCount == 0)
        create_info.pAttachments = &attachments[0];

    attachments[create_info.attachmentCount++] = att.create_info;

    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

ColorBlendStateDesc& ColorBlendStateDesc::set_blend_constants(float r, float g, float b, float a)
{
    create_info.blendConstants[0] = r;
    create_info.blendConstants[1] = g;
    create_info.blendConstants[2] = b;
    create_info.blendConstants[3] = a;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

ViewportStateDesc::ViewportStateDesc()
{
    HELIOS_ZERO_MEMORY(create_info);

    create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;

    for (int i = 0; i < 32; i++)
    {
        HELIOS_ZERO_MEMORY(viewports[i]);
        HELIOS_ZERO_MEMORY(scissors[i]);
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

ViewportStateDesc& ViewportStateDesc::add_viewport(float x,
                                                   float y,
                                                   float width,
                                                   float height,
                                                   float min_depth,
                                                   float max_depth)
{
    if (viewport_count == 32)
    {
        HELIOS_LOG_FATAL("(Vulkan) Max viewport count reached.");
        throw std::runtime_error("(Vulkan) Max viewport count reached.");
    }

    uint32_t idx = viewport_count++;

    viewports[idx].x        = x;
    viewports[idx].y        = y;
    viewports[idx].width    = width;
    viewports[idx].height   = height;
    viewports[idx].minDepth = min_depth;
    viewports[idx].maxDepth = max_depth;

    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

ViewportStateDesc& ViewportStateDesc::add_scissor(int32_t  x,
                                                  int32_t  y,
                                                  uint32_t w,
                                                  uint32_t h)
{
    if (scissor_count == 32)
    {
        HELIOS_LOG_FATAL("(Vulkan) Max scissor count reached.");
        throw std::runtime_error("(Vulkan) Max scissor count reached.");
    }

    uint32_t idx = scissor_count++;

    scissors[idx].extent.width  = w;
    scissors[idx].extent.height = h;
    scissors[idx].offset.x      = x;
    scissors[idx].offset.y      = y;

    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

GraphicsPipeline::Desc::Desc()
{
    HELIOS_ZERO_MEMORY(create_info);

    create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

    for (uint32_t i = 0; i < 6; i++)
    {
        HELIOS_ZERO_MEMORY(shader_stages[i]);
        shader_stages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    }

    HELIOS_ZERO_MEMORY(dynamic_state);

    dynamic_state.sType          = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.pDynamicStates = nullptr;
}

// -----------------------------------------------------------------------------------------------------------------------------------

GraphicsPipeline::Desc& GraphicsPipeline::Desc::add_dynamic_state(const VkDynamicState& state)
{
    if (dynamic_state_count == 32)
    {
        HELIOS_LOG_FATAL("(Vulkan) Max dynamic state count reached.");
        throw std::runtime_error("(Vulkan) Max dynamic state count reached.");
    }

    dynamic_states[dynamic_state_count++] = state;

    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

GraphicsPipeline::Desc& GraphicsPipeline::Desc::set_viewport_state(ViewportStateDesc& state)
{
    state.create_info.viewportCount = state.viewport_count;
    state.create_info.scissorCount  = state.scissor_count;
    state.create_info.pScissors     = state.scissors;
    state.create_info.pViewports    = state.viewports;
    create_info.pViewportState      = &state.create_info;

    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

GraphicsPipeline::Desc& GraphicsPipeline::Desc::add_shader_stage(const VkShaderStageFlagBits& stage, const ShaderModule::Ptr& shader_module, const std::string& name)
{
    uint32_t idx = shader_stage_count++;

    shader_entry_names[idx]   = name;
    shader_stages[idx].module = shader_module->handle();
    shader_stages[idx].pName  = shader_entry_names[idx].c_str();
    shader_stages[idx].stage  = stage;

    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

GraphicsPipeline::Desc& GraphicsPipeline::Desc::set_vertex_input_state(const VertexInputStateDesc& state)
{
    create_info.pVertexInputState = &state.create_info;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

GraphicsPipeline::Desc& GraphicsPipeline::Desc::set_input_assembly_state(const InputAssemblyStateDesc& state)
{
    create_info.pInputAssemblyState = &state.create_info;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

GraphicsPipeline::Desc& GraphicsPipeline::Desc::set_tessellation_state(const TessellationStateDesc& state)
{
    create_info.pTessellationState = &state.create_info;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

GraphicsPipeline::Desc& GraphicsPipeline::Desc::set_rasterization_state(const RasterizationStateDesc& state)
{
    create_info.pRasterizationState = &state.create_info;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

GraphicsPipeline::Desc& GraphicsPipeline::Desc::set_multisample_state(const MultisampleStateDesc& state)
{
    create_info.pMultisampleState = &state.create_info;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

GraphicsPipeline::Desc& GraphicsPipeline::Desc::set_depth_stencil_state(const DepthStencilStateDesc& state)
{
    create_info.pDepthStencilState = &state.create_info;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

GraphicsPipeline::Desc& GraphicsPipeline::Desc::set_color_blend_state(const ColorBlendStateDesc& state)
{
    create_info.pColorBlendState = &state.create_info;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

GraphicsPipeline::Desc& GraphicsPipeline::Desc::set_pipeline_layout(const std::shared_ptr<PipelineLayout>& layout)
{
    create_info.layout = layout->handle();
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

GraphicsPipeline::Desc& GraphicsPipeline::Desc::set_render_pass(const RenderPass::Ptr& render_pass)
{
    create_info.renderPass = render_pass->handle();
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

GraphicsPipeline::Desc& GraphicsPipeline::Desc::set_sub_pass(const uint32_t& subpass)
{
    create_info.subpass = subpass;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

GraphicsPipeline::Desc& GraphicsPipeline::Desc::set_base_pipeline(const GraphicsPipeline::Ptr& pipeline)
{
    create_info.basePipelineHandle = pipeline->handle();
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

GraphicsPipeline::Desc& GraphicsPipeline::Desc::set_base_pipeline_index(const int32_t& index)
{
    create_info.basePipelineIndex = index;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

GraphicsPipeline::Ptr GraphicsPipeline::create_for_post_process(Backend::Ptr backend, std::string vs, std::string fs, std::shared_ptr<PipelineLayout> pipeline_layout, RenderPass::Ptr render_pass)
{
    // ---------------------------------------------------------------------------
    // Create shader modules
    // ---------------------------------------------------------------------------

    vk::ShaderModule::Ptr vs_module = vk::ShaderModule::create_from_file(backend, vs);
    vk::ShaderModule::Ptr fs_module = vk::ShaderModule::create_from_file(backend, fs);

    vk::GraphicsPipeline::Desc pso_desc;

    pso_desc.add_shader_stage(VK_SHADER_STAGE_VERTEX_BIT, vs_module, "main")
        .add_shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT, fs_module, "main");

    // ---------------------------------------------------------------------------
    // Vertex input state
    // ---------------------------------------------------------------------------

    VertexInputStateDesc vs_desc;

    pso_desc.set_vertex_input_state(vs_desc);

    // ---------------------------------------------------------------------------
    // Create pipeline input assembly state
    // ---------------------------------------------------------------------------

    vk::InputAssemblyStateDesc input_assembly_state_desc;

    input_assembly_state_desc.set_primitive_restart_enable(false)
        .set_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    pso_desc.set_input_assembly_state(input_assembly_state_desc);

    // ---------------------------------------------------------------------------
    // Create viewport state
    // ---------------------------------------------------------------------------

    vk::ViewportStateDesc vp_desc;

    vp_desc.add_viewport(0.0f, 0.0f, 1, 1, 0.0f, 1.0f)
        .add_scissor(0, 0, 1, 1);

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
        .set_depth_write_enable(VK_FALSE)
        .set_depth_compare_op(VK_COMPARE_OP_LESS)
        .set_depth_bounds_test_enable(VK_FALSE)
        .set_stencil_test_enable(VK_FALSE);

    pso_desc.set_depth_stencil_state(ds_state);

    // ---------------------------------------------------------------------------
    // Create color blend state
    // ---------------------------------------------------------------------------

    vk::ColorBlendAttachmentStateDesc blend_att_desc;

    blend_att_desc.set_color_write_mask(VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT)
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

    pso_desc.set_pipeline_layout(pipeline_layout);

    // ---------------------------------------------------------------------------
    // Create dynamic state
    // ---------------------------------------------------------------------------

    pso_desc.add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
        .add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR);

    // ---------------------------------------------------------------------------
    // Create pipeline
    // ---------------------------------------------------------------------------

    pso_desc.set_render_pass(render_pass);

    return vk::GraphicsPipeline::create(backend, pso_desc);
}

// -----------------------------------------------------------------------------------------------------------------------------------

GraphicsPipeline::Ptr GraphicsPipeline::create(Backend::Ptr backend, Desc desc)
{
    return std::shared_ptr<GraphicsPipeline>(new GraphicsPipeline(backend, desc));
}

// -----------------------------------------------------------------------------------------------------------------------------------

GraphicsPipeline::GraphicsPipeline(Backend::Ptr backend, Desc desc) :
    Object(backend)
{
    desc.create_info.pStages             = &desc.shader_stages[0];
    desc.create_info.stageCount          = desc.shader_stage_count;
    desc.dynamic_state.dynamicStateCount = desc.dynamic_state_count;
    desc.dynamic_state.pDynamicStates    = &desc.dynamic_states[0];
    desc.create_info.pDynamicState       = &desc.dynamic_state;

    if (vkCreateGraphicsPipelines(backend->device(), nullptr, 1, &desc.create_info, nullptr, &m_vk_pipeline) != VK_SUCCESS)
    {
        HELIOS_LOG_FATAL("(Vulkan) Failed to create Graphics Pipeline.");
        throw std::runtime_error("(Vulkan) Failed to create Graphics Pipeline.");
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

GraphicsPipeline::~GraphicsPipeline()
{
    if (m_vk_backend.expired())
    {
        HELIOS_LOG_FATAL("(Vulkan) Destructing after Device.");
        throw std::runtime_error("(Vulkan) Destructing after Device.");
    }

    auto backend = m_vk_backend.lock();

    vkDestroyPipeline(backend->device(), m_vk_pipeline, nullptr);
}

// -----------------------------------------------------------------------------------------------------------------------------------

ComputePipeline::Desc::Desc()
{
    HELIOS_ZERO_MEMORY(create_info);

    create_info.sType       = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    create_info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
}

// -----------------------------------------------------------------------------------------------------------------------------------

ComputePipeline::Desc& ComputePipeline::Desc::set_shader_stage(ShaderModule::Ptr shader_module, std::string name)
{
    shader_entry_name        = name;
    create_info.stage.pName  = shader_entry_name.c_str();
    create_info.stage.module = shader_module->handle();
    create_info.stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;

    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

ComputePipeline::Desc& ComputePipeline::Desc::set_pipeline_layout(std::shared_ptr<PipelineLayout> layout)
{
    create_info.layout = layout->handle();
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

ComputePipeline::Desc& ComputePipeline::Desc::set_base_pipeline(ComputePipeline::Ptr pipeline)
{
    create_info.basePipelineHandle = pipeline->handle();
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

ComputePipeline::Desc& ComputePipeline::Desc::set_base_pipeline_index(int32_t index)
{
    create_info.basePipelineIndex = index;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

ComputePipeline::Ptr ComputePipeline::create(Backend::Ptr backend, Desc desc)
{
    return std::shared_ptr<ComputePipeline>(new ComputePipeline(backend, desc));
}

// -----------------------------------------------------------------------------------------------------------------------------------

ComputePipeline::ComputePipeline(Backend::Ptr backend, Desc desc) :
    Object(backend)
{
    if (vkCreateComputePipelines(backend->device(), nullptr, 1, &desc.create_info, nullptr, &m_vk_pipeline) != VK_SUCCESS)
    {
        HELIOS_LOG_FATAL("(Vulkan) Failed to create Compute Pipeline.");
        throw std::runtime_error("(Vulkan) Failed to create Compute Pipeline.");
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

ComputePipeline::~ComputePipeline()
{
    if (m_vk_backend.expired())
    {
        HELIOS_LOG_FATAL("(Vulkan) Destructing after Device.");
        throw std::runtime_error("(Vulkan) Destructing after Device.");
    }

    auto backend = m_vk_backend.lock();

    vkDestroyPipeline(backend->device(), m_vk_pipeline, nullptr);
}

// -----------------------------------------------------------------------------------------------------------------------------------

ShaderBindingTable::Desc::Desc()
{
    entry_point_names.reserve(32);
    ray_gen_stages.reserve(32);
    hit_stages.reserve(32);
    miss_stages.reserve(32);
    hit_groups.reserve(32);
}

// -----------------------------------------------------------------------------------------------------------------------------------

ShaderBindingTable::Desc& ShaderBindingTable::Desc::add_ray_gen_group(ShaderModule::Ptr shader, const std::string& entry_point)
{
    VkPipelineShaderStageCreateInfo stage;
    HELIOS_ZERO_MEMORY(stage);

    entry_point_names.push_back(entry_point);

    stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.module = shader->handle();
    stage.stage  = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    stage.pName  = entry_point_names.back().c_str();

    ray_gen_stages.push_back(stage);

    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

ShaderBindingTable::Desc& ShaderBindingTable::Desc::add_hit_group(ShaderModule::Ptr  closest_hit_shader,
                                                                  const std::string& closest_hit_entry_point,
                                                                  ShaderModule::Ptr  any_hit_shader,
                                                                  const std::string& any_hit_entry_point,
                                                                  ShaderModule::Ptr  intersection_shader,
                                                                  const std::string& intersection_entry_point)
{
    HitGroupDesc group_desc;

    VkPipelineShaderStageCreateInfo closest_hit_stage;
    HELIOS_ZERO_MEMORY(closest_hit_stage);

    entry_point_names.push_back(closest_hit_entry_point);

    closest_hit_stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    closest_hit_stage.module = closest_hit_shader->handle();
    closest_hit_stage.stage  = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    closest_hit_stage.pName  = entry_point_names.back().c_str();

    hit_stages.push_back(closest_hit_stage);

    group_desc.closest_hit_stage = &hit_stages.back();

    if (any_hit_shader)
    {
        VkPipelineShaderStageCreateInfo any_hit_stage;
        HELIOS_ZERO_MEMORY(any_hit_stage);

        entry_point_names.push_back(any_hit_entry_point);

        any_hit_stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        any_hit_stage.module = any_hit_shader->handle();
        any_hit_stage.stage  = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
        any_hit_stage.pName  = entry_point_names.back().c_str();

        hit_stages.push_back(any_hit_stage);

        group_desc.any_hit_stage = &hit_stages.back();
    }

    if (intersection_shader)
    {
        VkPipelineShaderStageCreateInfo intersection_stage;
        HELIOS_ZERO_MEMORY(intersection_stage);

        entry_point_names.push_back(intersection_entry_point);

        intersection_stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        intersection_stage.module = intersection_shader->handle();
        intersection_stage.stage  = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
        intersection_stage.pName  = entry_point_names.back().c_str();

        hit_stages.push_back(intersection_stage);

        group_desc.intersection_stage = &hit_stages.back();
    }

    hit_groups.push_back(group_desc);

    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

ShaderBindingTable::Desc& ShaderBindingTable::Desc::add_miss_group(ShaderModule::Ptr shader, const std::string& entry_point)
{
    VkPipelineShaderStageCreateInfo stage;
    HELIOS_ZERO_MEMORY(stage);

    entry_point_names.push_back(entry_point);

    stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.module = shader->handle();
    stage.stage  = VK_SHADER_STAGE_MISS_BIT_KHR;
    stage.pName  = entry_point_names.back().c_str();

    miss_stages.push_back(stage);

    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

ShaderBindingTable::Ptr ShaderBindingTable::create(Backend::Ptr backend, Desc desc)
{
    return std::shared_ptr<ShaderBindingTable>(new ShaderBindingTable(backend, desc));
}

// -----------------------------------------------------------------------------------------------------------------------------------

VkDeviceSize ShaderBindingTable::hit_group_offset()
{
    return m_ray_gen_size + m_miss_group_size;
}

// -----------------------------------------------------------------------------------------------------------------------------------

VkDeviceSize ShaderBindingTable::miss_group_offset()
{
    return m_ray_gen_size;
}

// -----------------------------------------------------------------------------------------------------------------------------------

ShaderBindingTable::~ShaderBindingTable()
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

ShaderBindingTable::ShaderBindingTable(Backend::Ptr backend, Desc desc) :
    Object(backend)
{
    m_entry_point_names.reserve(32);

    auto& rt_pipeline_props = backend->ray_tracing_pipeline_properties();

    // Ray gen shaders
    for (auto& stage : desc.ray_gen_stages)
    {
        VkRayTracingShaderGroupCreateInfoKHR group_info;
        HELIOS_ZERO_MEMORY(group_info);

        m_entry_point_names.push_back(std::string(stage.pName));

        stage.pName = m_entry_point_names[m_entry_point_names.size() - 1].c_str();

        group_info.sType              = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        group_info.pNext              = nullptr;
        group_info.type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        group_info.generalShader      = m_stages.size();
        group_info.closestHitShader   = VK_SHADER_UNUSED_KHR;
        group_info.anyHitShader       = VK_SHADER_UNUSED_KHR;
        group_info.intersectionShader = VK_SHADER_UNUSED_KHR;

        m_stages.push_back(stage);
        m_groups.push_back(group_info);
    }

    // Ray miss shaders
    for (auto& stage : desc.miss_stages)
    {
        VkRayTracingShaderGroupCreateInfoKHR group_info;
        HELIOS_ZERO_MEMORY(group_info);

        group_info.sType              = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        group_info.pNext              = nullptr;
        group_info.type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        group_info.generalShader      = m_stages.size();
        group_info.closestHitShader   = VK_SHADER_UNUSED_KHR;
        group_info.anyHitShader       = VK_SHADER_UNUSED_KHR;
        group_info.intersectionShader = VK_SHADER_UNUSED_KHR;

        m_entry_point_names.push_back(std::string(stage.pName));
        stage.pName = m_entry_point_names[m_entry_point_names.size() - 1].c_str();

        m_stages.push_back(stage);
        m_groups.push_back(group_info);
    }

    // Ray hit shaders
    for (auto& group : desc.hit_groups)
    {
        if (!group.closest_hit_stage)
        {
            HELIOS_LOG_FATAL("(Vulkan) Hit shader group does not have Closest Hit stage.");
            throw std::runtime_error("(Vulkan) Hit shader group does not have Closest Hit stage.");
        }

        VkRayTracingShaderGroupCreateInfoKHR group_info;
        HELIOS_ZERO_MEMORY(group_info);

        group_info.sType              = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        group_info.pNext              = nullptr;
        group_info.type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
        group_info.generalShader      = VK_SHADER_UNUSED_KHR;
        group_info.closestHitShader   = m_stages.size();
        group_info.anyHitShader       = VK_SHADER_UNUSED_KHR;
        group_info.intersectionShader = VK_SHADER_UNUSED_KHR;

        m_entry_point_names.push_back(std::string(group.closest_hit_stage->pName));
        group.closest_hit_stage->pName = m_entry_point_names[m_entry_point_names.size() - 1].c_str();

        m_stages.push_back(*group.closest_hit_stage);

        if (group.any_hit_stage)
        {
            group_info.anyHitShader = m_stages.size();

            m_entry_point_names.push_back(std::string(group.any_hit_stage->pName));
            group.any_hit_stage->pName = m_entry_point_names[m_entry_point_names.size() - 1].c_str();

            m_stages.push_back(*group.any_hit_stage);
        }

        if (group.intersection_stage)
        {
            group_info.intersectionShader = m_stages.size();

            m_entry_point_names.push_back(std::string(group.intersection_stage->pName));
            group.intersection_stage->pName = m_entry_point_names[m_entry_point_names.size() - 1].c_str();

            m_stages.push_back(*group.intersection_stage);
        }

        m_groups.push_back(group_info);
    }

    m_ray_gen_size    = desc.ray_gen_stages.size() * rt_pipeline_props.shaderGroupBaseAlignment;
    m_hit_group_size  = desc.hit_groups.size() * rt_pipeline_props.shaderGroupBaseAlignment;
    m_miss_group_size = desc.miss_stages.size() * rt_pipeline_props.shaderGroupBaseAlignment;
}

// -----------------------------------------------------------------------------------------------------------------------------------

RayTracingPipeline::Desc::Desc()
{
    HELIOS_ZERO_MEMORY(create_info);

    create_info.sType           = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    create_info.pNext           = nullptr;
}

// -----------------------------------------------------------------------------------------------------------------------------------

RayTracingPipeline::Desc& RayTracingPipeline::Desc::set_shader_binding_table(ShaderBindingTable::Ptr table)
{
    sbt                                                             = table;
    const std::vector<VkPipelineShaderStageCreateInfo>&      stages = table->stages();
    const std::vector<VkRayTracingShaderGroupCreateInfoKHR>& groups = table->groups();

    create_info.groupCount = groups.size();
    create_info.pGroups    = groups.data();

    create_info.stageCount = stages.size();
    create_info.pStages    = stages.data();

    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

RayTracingPipeline::Desc& RayTracingPipeline::Desc::set_pipeline_layout(std::shared_ptr<PipelineLayout> layout)
{
    create_info.layout = layout->handle();
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

RayTracingPipeline::Desc& RayTracingPipeline::Desc::set_max_pipeline_ray_recursion_depth(uint32_t depth)
{
    create_info.maxPipelineRayRecursionDepth = depth;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

RayTracingPipeline::Desc& RayTracingPipeline::Desc::set_base_pipeline(RayTracingPipeline::Ptr pipeline)
{
    create_info.basePipelineHandle = pipeline->handle();
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

RayTracingPipeline::Desc& RayTracingPipeline::Desc::set_base_pipeline_index(int32_t index)
{
    create_info.basePipelineIndex = index;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

RayTracingPipeline::Ptr RayTracingPipeline::create(Backend::Ptr backend, Desc desc)
{
    return std::shared_ptr<RayTracingPipeline>(new RayTracingPipeline(backend, desc));
}

// -----------------------------------------------------------------------------------------------------------------------------------

RayTracingPipeline::~RayTracingPipeline()
{
    if (m_vk_backend.expired())
    {
        HELIOS_LOG_FATAL("(Vulkan) Destructing after Device.");
        throw std::runtime_error("(Vulkan) Destructing after Device.");
    }

    auto backend = m_vk_backend.lock();

    m_vk_buffer.reset();
    m_sbt.reset();
    vkDestroyPipeline(backend->device(), m_vk_pipeline, nullptr);
}

// -----------------------------------------------------------------------------------------------------------------------------------

RayTracingPipeline::RayTracingPipeline(Backend::Ptr backend, Desc desc) :
    Object(backend)
{
    m_sbt = desc.sbt;

    desc.create_info.pGroups = m_sbt->groups().data();
    desc.create_info.pStages = m_sbt->stages().data();

    if (vkCreateRayTracingPipelinesKHR(backend->device(), VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &desc.create_info, VK_NULL_HANDLE, &m_vk_pipeline) != VK_SUCCESS)
    {
        HELIOS_LOG_FATAL("(Vulkan) Failed to create Ray Tracing Pipeline.");
        throw std::runtime_error("(Vulkan) Failed to create Ray Tracing Pipeline.");
    }

    const auto& rt_pipeline_props = backend->ray_tracing_pipeline_properties();

    size_t sbt_size = m_sbt->groups().size() * rt_pipeline_props.shaderGroupBaseAlignment;

    m_vk_buffer = vk::Buffer::create(backend, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, sbt_size, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT);

    std::vector<uint8_t> scratch_mem(sbt_size);

    if (vkGetRayTracingShaderGroupHandlesKHR(backend->device(), m_vk_pipeline, 0, m_sbt->groups().size(), sbt_size, scratch_mem.data()) != VK_SUCCESS)
    {
        HELIOS_LOG_FATAL("(Vulkan) Failed to get Shader Group handles.");
        throw std::runtime_error("(Vulkan) Failed to get Shader Group handles.");
    }

    uint8_t* src_ptr = scratch_mem.data();
    uint8_t* dst_ptr = (uint8_t*)m_vk_buffer->mapped_ptr();

    for (int i = 0; i < m_sbt->groups().size(); i++)
    {
        memcpy(dst_ptr, src_ptr, rt_pipeline_props.shaderGroupHandleSize);

        dst_ptr += rt_pipeline_props.shaderGroupBaseAlignment;
        src_ptr += rt_pipeline_props.shaderGroupHandleSize;
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

AccelerationStructure::Desc::Desc()
{
    HELIOS_ZERO_MEMORY(create_info);

    create_info.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    create_info.pNext         = nullptr;

    HELIOS_ZERO_MEMORY(build_geometry_info);

    build_geometry_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    build_geometry_info.pNext = nullptr;
}

// -----------------------------------------------------------------------------------------------------------------------------------

AccelerationStructure::Desc& AccelerationStructure::Desc::set_type(VkAccelerationStructureTypeKHR type)
{
    create_info.type = type;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

AccelerationStructure::Desc& AccelerationStructure::Desc::set_geometries(const std::vector<VkAccelerationStructureGeometryKHR>& geometry_vec)
{
    geometries                 = geometry_vec;
    build_geometry_info.pGeometries = geometries.data();
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

AccelerationStructure::Desc& AccelerationStructure::Desc::set_max_primitive_counts(const std::vector<uint32_t>& primitive_counts)
{
    max_primitive_counts = primitive_counts;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

AccelerationStructure::Desc& AccelerationStructure::Desc::set_geometry_count(uint32_t count)
{
    build_geometry_info.geometryCount = count;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

AccelerationStructure::Desc& AccelerationStructure::Desc::set_flags(VkBuildAccelerationStructureFlagsKHR flags)
{
    build_geometry_info.flags = flags;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

AccelerationStructure::Desc& AccelerationStructure::Desc::set_device_address(VkDeviceAddress address)
{
    create_info.deviceAddress = address;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

AccelerationStructure::Ptr AccelerationStructure::create(Backend::Ptr backend, Desc desc)
{
    return std::shared_ptr<AccelerationStructure>(new AccelerationStructure(backend, desc));
}

// -----------------------------------------------------------------------------------------------------------------------------------

AccelerationStructure::~AccelerationStructure()
{
    if (m_vk_backend.expired())
    {
        HELIOS_LOG_FATAL("(Vulkan) Destructing after Device.");
        throw std::runtime_error("(Vulkan) Destructing after Device.");
    }

    auto backend = m_vk_backend.lock();

    vkDestroyAccelerationStructureKHR(backend->device(), m_vk_acceleration_structure, nullptr);
    m_buffer.reset();
}

// -----------------------------------------------------------------------------------------------------------------------------------

AccelerationStructure::AccelerationStructure(Backend::Ptr backend, Desc desc) :
    Object(backend)
{
    m_flags                          = desc.build_geometry_info.flags;
    m_vk_acceleration_structure_info = desc.create_info;

    HELIOS_ZERO_MEMORY(m_build_sizes);

    m_build_sizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    
    vkGetAccelerationStructureBuildSizesKHR(
        backend->device(),
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &desc.build_geometry_info,
        desc.max_primitive_counts.data(),
        &m_build_sizes);

    // Allocate buffer
    m_buffer = vk::Buffer::create(backend, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, m_build_sizes.accelerationStructureSize, VMA_MEMORY_USAGE_GPU_ONLY, 0);

    m_vk_acceleration_structure_info.buffer = m_buffer->handle();
    m_vk_acceleration_structure_info.size   = m_build_sizes.accelerationStructureSize;

    if (vkCreateAccelerationStructureKHR(backend->device(), &desc.create_info, nullptr, &m_vk_acceleration_structure) != VK_SUCCESS)
    {
        HELIOS_LOG_FATAL("(Vulkan) Failed to create Acceleration Structure.");
        throw std::runtime_error("(Vulkan) Failed to create Acceleration Structure.");
    }

    VkAccelerationStructureDeviceAddressInfoKHR address_info;
    HELIOS_ZERO_MEMORY(address_info);

    address_info.sType                 = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    address_info.accelerationStructure = m_vk_acceleration_structure;

    m_device_address = vkGetAccelerationStructureDeviceAddressKHR(backend->device(), &address_info);

    if (m_device_address == 0)
    {
        HELIOS_LOG_FATAL("(Vulkan) Failed to create Acceleration Structure.");
        throw std::runtime_error("(Vulkan) Failed to create Acceleration Structure.");
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

Sampler::Ptr Sampler::create(Backend::Ptr backend, Desc desc)
{
    return std::shared_ptr<Sampler>(new Sampler(backend, desc));
}

// -----------------------------------------------------------------------------------------------------------------------------------

Sampler::Sampler(Backend::Ptr backend, Desc desc) :
    Object(backend)
{
    VkSamplerCreateInfo info;
    HELIOS_ZERO_MEMORY(info);

    info.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.flags                   = desc.flags;
    info.magFilter               = desc.mag_filter;
    info.minFilter               = desc.min_filter;
    info.mipmapMode              = desc.mipmap_mode;
    info.addressModeU            = desc.address_mode_u;
    info.addressModeV            = desc.address_mode_v;
    info.addressModeW            = desc.address_mode_w;
    info.mipLodBias              = desc.mip_lod_bias;
    info.anisotropyEnable        = desc.anisotropy_enable;
    info.maxAnisotropy           = desc.max_anisotropy;
    info.compareEnable           = desc.compare_enable;
    info.compareOp               = desc.compare_op;
    info.minLod                  = desc.min_lod;
    info.maxLod                  = desc.max_lod;
    info.borderColor             = desc.border_color;
    info.unnormalizedCoordinates = desc.unnormalized_coordinates;

    if (vkCreateSampler(backend->device(), &info, nullptr, &m_vk_sampler) != VK_SUCCESS)
    {
        HELIOS_LOG_FATAL("(Vulkan) Failed to create sampler.");
        throw std::runtime_error("(Vulkan) Failed to create sampler.");
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

Sampler::~Sampler()
{
    if (m_vk_backend.expired())
    {
        HELIOS_LOG_FATAL("(Vulkan) Destructing after Device.");
        throw std::runtime_error("(Vulkan) Destructing after Device.");
    }

    auto backend = m_vk_backend.lock();

    vkDestroySampler(backend->device(), m_vk_sampler, nullptr);
}

// -----------------------------------------------------------------------------------------------------------------------------------

DescriptorSetLayout::Desc& DescriptorSetLayout::Desc::set_next_ptr(void* pnext)
{
    pnext_ptr = pnext;
    return *this;
}

DescriptorSetLayout::Desc& DescriptorSetLayout::Desc::add_binding(uint32_t binding, VkDescriptorType descriptor_type, uint32_t descriptor_count, VkShaderStageFlags stage_flags)
{
    bindings.push_back({ binding, descriptor_type, descriptor_count, stage_flags, nullptr });
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

DescriptorSetLayout::Desc& DescriptorSetLayout::Desc::add_binding(uint32_t binding, VkDescriptorType descriptor_type, uint32_t descriptor_count, VkShaderStageFlags stage_flags, Sampler::Ptr samplers[])
{
    for (int i = 0; i < descriptor_count; i++)
        binding_samplers[binding][i] = samplers[i]->handle();

    bindings.push_back({ binding, descriptor_type, descriptor_count, stage_flags, &binding_samplers[binding][0] });
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

DescriptorSetLayout::Ptr DescriptorSetLayout::create(Backend::Ptr backend, Desc desc)
{
    return std::shared_ptr<DescriptorSetLayout>(new DescriptorSetLayout(backend, desc));
}

// -----------------------------------------------------------------------------------------------------------------------------------

DescriptorSetLayout::DescriptorSetLayout(Backend::Ptr backend, Desc desc) :
    Object(backend)
{
    VkDescriptorSetLayoutCreateInfo layout_info;
    HELIOS_ZERO_MEMORY(layout_info);

    layout_info.pNext        = desc.pnext_ptr;
    layout_info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = desc.bindings.size();
    layout_info.pBindings    = desc.bindings.data();

    if (vkCreateDescriptorSetLayout(backend->device(), &layout_info, nullptr, &m_vk_ds_layout) != VK_SUCCESS)
    {
        HELIOS_LOG_FATAL("(Vulkan) Failed to create Descriptor Set Layout.");
        throw std::runtime_error("(Vulkan) Failed to create Descriptor Set Layout.");
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

DescriptorSetLayout::~DescriptorSetLayout()
{
    if (m_vk_backend.expired())
    {
        HELIOS_LOG_FATAL("(Vulkan) Destructing after Device.");
        throw std::runtime_error("(Vulkan) Destructing after Device.");
    }

    auto backend = m_vk_backend.lock();

    vkDestroyDescriptorSetLayout(backend->device(), m_vk_ds_layout, nullptr);
}

// -----------------------------------------------------------------------------------------------------------------------------------

PipelineLayout::Desc& PipelineLayout::Desc::add_descriptor_set_layout(DescriptorSetLayout::Ptr layout)
{
    layouts.push_back(layout);
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

PipelineLayout::Desc& PipelineLayout::Desc::add_push_constant_range(VkShaderStageFlags stage_flags, uint32_t offset, uint32_t size)
{
    push_constant_ranges.push_back({ stage_flags, offset, size });
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

PipelineLayout::Ptr PipelineLayout::create(Backend::Ptr backend, Desc desc)
{
    return std::shared_ptr<PipelineLayout>(new PipelineLayout(backend, desc));
}

// -----------------------------------------------------------------------------------------------------------------------------------

PipelineLayout::PipelineLayout(Backend::Ptr backend, Desc desc) :
    Object(backend)
{
    std::vector<VkDescriptorSetLayout> vk_layouts(desc.layouts.size());

    for (uint32_t i = 0; i < desc.layouts.size(); i++)
        vk_layouts[i] = desc.layouts[i]->handle();

    VkPipelineLayoutCreateInfo info;
    HELIOS_ZERO_MEMORY(info);

    info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.pushConstantRangeCount = desc.push_constant_ranges.size();
    info.pPushConstantRanges    = desc.push_constant_ranges.data();
    info.setLayoutCount         = desc.layouts.size();
    info.pSetLayouts            = vk_layouts.data();

    if (vkCreatePipelineLayout(backend->device(), &info, nullptr, &m_vk_pipeline_layout) != VK_SUCCESS)
    {
        HELIOS_LOG_FATAL("(Vulkan) Failed to create pipeline layout.");
        throw std::runtime_error("(Vulkan) Failed to create pipeline layout.");
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

PipelineLayout::~PipelineLayout()
{
    if (m_vk_backend.expired())
    {
        HELIOS_LOG_FATAL("(Vulkan) Destructing after Device.");
        throw std::runtime_error("(Vulkan) Destructing after Device.");
    }

    auto backend = m_vk_backend.lock();

    vkDestroyPipelineLayout(backend->device(), m_vk_pipeline_layout, nullptr);
}

// -----------------------------------------------------------------------------------------------------------------------------------

DescriptorPool::Desc& DescriptorPool::Desc::set_max_sets(uint32_t num)
{
    max_sets = num;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

DescriptorPool::Desc& DescriptorPool::Desc::set_create_flags(VkDescriptorPoolCreateFlags flags)
{
    create_flags = flags;
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

DescriptorPool::Desc& DescriptorPool::Desc::add_pool_size(VkDescriptorType type, uint32_t descriptor_count)
{
    pool_sizes.push_back({ type, descriptor_count });
    return *this;
}

// -----------------------------------------------------------------------------------------------------------------------------------

DescriptorPool::Ptr DescriptorPool::create(Backend::Ptr backend, Desc desc)
{
    return std::shared_ptr<DescriptorPool>(new DescriptorPool(backend, desc));
}

// -----------------------------------------------------------------------------------------------------------------------------------

DescriptorPool::DescriptorPool(Backend::Ptr backend, Desc desc) :
    Object(backend)
{
    m_vk_create_flags = desc.create_flags;

    VkDescriptorPoolCreateInfo pool_info;
    HELIOS_ZERO_MEMORY(pool_info);

    pool_info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = static_cast<uint32_t>(desc.pool_sizes.size());
    pool_info.pPoolSizes    = desc.pool_sizes.data();
    pool_info.maxSets       = desc.max_sets;
    pool_info.flags         = desc.create_flags;

    if (vkCreateDescriptorPool(backend->device(), &pool_info, nullptr, &m_vk_ds_pool) != VK_SUCCESS)
    {
        HELIOS_LOG_FATAL("(Vulkan) Failed to create descriptor pool.");
        throw std::runtime_error("(Vulkan) Failed to create descriptor pool.");
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

DescriptorPool::~DescriptorPool()
{
    if (m_vk_backend.expired())
    {
        HELIOS_LOG_FATAL("(Vulkan) Destructing after Device.");
        throw std::runtime_error("(Vulkan) Destructing after Device.");
    }

    auto backend = m_vk_backend.lock();

    vkDestroyDescriptorPool(backend->device(), m_vk_ds_pool, nullptr);
}

// -----------------------------------------------------------------------------------------------------------------------------------

DescriptorSet::Ptr DescriptorSet::create(Backend::Ptr backend, DescriptorSetLayout::Ptr layout, DescriptorPool::Ptr pool)
{
    return std::shared_ptr<DescriptorSet>(new DescriptorSet(backend, layout, pool));
}

// -----------------------------------------------------------------------------------------------------------------------------------

DescriptorSet::DescriptorSet(Backend::Ptr backend, DescriptorSetLayout::Ptr layout, DescriptorPool::Ptr pool) :
    Object(backend)
{
    m_vk_pool = pool;

    VkDescriptorSetAllocateInfo info;
    HELIOS_ZERO_MEMORY(info);

    VkDescriptorSetLayout vk_layout = layout->handle();

    info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.descriptorPool     = pool->handle();
    info.descriptorSetCount = 1;
    info.pSetLayouts        = &vk_layout;

    if ((pool->create_flags() & VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT) == VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT)
        m_should_destroy = true;

    if (vkAllocateDescriptorSets(backend->device(), &info, &m_vk_ds) != VK_SUCCESS)
    {
        HELIOS_LOG_FATAL("(Vulkan) Failed to allocate descriptor set.");
        throw std::runtime_error("(Vulkan) Failed to allocate descriptor set.");
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

DescriptorSet::~DescriptorSet()
{
    if (m_vk_backend.expired() || m_vk_pool.expired())
    {
        HELIOS_LOG_FATAL("(Vulkan) Destructing after Device.");
        throw std::runtime_error("(Vulkan) Destructing after Device.");
    }

    auto backend = m_vk_backend.lock();
    auto pool    = m_vk_pool.lock();

    if (m_should_destroy)
        vkFreeDescriptorSets(backend->device(), pool->handle(), 1, &m_vk_ds);
}

// -----------------------------------------------------------------------------------------------------------------------------------

Fence::Ptr Fence::create(Backend::Ptr backend)
{
    return std::shared_ptr<Fence>(new Fence(backend));
}

// -----------------------------------------------------------------------------------------------------------------------------------

Fence::~Fence()
{
    if (m_vk_backend.expired())
    {
        HELIOS_LOG_FATAL("(Vulkan) Destructing after Device.");
        throw std::runtime_error("(Vulkan) Destructing after Device.");
    }

    auto backend = m_vk_backend.lock();

    vkDestroyFence(backend->device(), m_vk_fence, nullptr);
}

// -----------------------------------------------------------------------------------------------------------------------------------

Fence::Fence(Backend::Ptr backend) :
    Object(backend)
{
    VkFenceCreateInfo fence_info;
    HELIOS_ZERO_MEMORY(fence_info);

    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if (vkCreateFence(backend->device(), &fence_info, nullptr, &m_vk_fence) != VK_SUCCESS)
    {
        HELIOS_LOG_FATAL("(Vulkan) Failed to create Fence.");
        throw std::runtime_error("(Vulkan) Failed to create Fence.");
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

Semaphore::Ptr Semaphore::create(Backend::Ptr backend)
{
    return std::shared_ptr<Semaphore>(new Semaphore(backend));
}

// -----------------------------------------------------------------------------------------------------------------------------------

Semaphore::~Semaphore()
{
    if (m_vk_backend.expired())
    {
        HELIOS_LOG_FATAL("(Vulkan) Destructing after Device.");
        throw std::runtime_error("(Vulkan) Destructing after Device.");
    }

    auto backend = m_vk_backend.lock();

    vkDestroySemaphore(backend->device(), m_vk_semaphore, nullptr);
}

// -----------------------------------------------------------------------------------------------------------------------------------

Semaphore::Semaphore(Backend::Ptr backend) :
    Object(backend)
{
    VkSemaphoreCreateInfo semaphore_info;
    HELIOS_ZERO_MEMORY(semaphore_info);

    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    if (vkCreateSemaphore(backend->device(), &semaphore_info, nullptr, &m_vk_semaphore) != VK_SUCCESS)
    {
        HELIOS_LOG_FATAL("(Vulkan) Failed to create Semaphore.");
        throw std::runtime_error("(Vulkan) Failed to create Semaphore.");
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

QueryPool::Ptr QueryPool::create(Backend::Ptr backend, VkQueryType query_type, uint32_t query_count, VkQueryPipelineStatisticFlags pipeline_statistics)
{
    return std::shared_ptr<QueryPool>(new QueryPool(backend, query_type, query_count, pipeline_statistics));
}

// -----------------------------------------------------------------------------------------------------------------------------------

bool QueryPool::results(uint32_t           first_query,
                        uint32_t           query_count,
                        size_t             data_size,
                        void*              ptr,
                        VkDeviceSize       stride,
                        VkQueryResultFlags flags)
{
    auto backend = m_vk_backend.lock();

    return vkGetQueryPoolResults(backend->device(), m_vk_query_pool, first_query, query_count, data_size, ptr, stride, flags) != VK_SUCCESS;
}

// -----------------------------------------------------------------------------------------------------------------------------------

QueryPool::~QueryPool()
{
    if (m_vk_backend.expired())
    {
        HELIOS_LOG_FATAL("(Vulkan) Destructing after Device.");
        throw std::runtime_error("(Vulkan) Destructing after Device.");
    }

    auto backend = m_vk_backend.lock();

    vkDestroyQueryPool(backend->device(), m_vk_query_pool, nullptr);
}

// -----------------------------------------------------------------------------------------------------------------------------------

QueryPool::QueryPool(Backend::Ptr backend, VkQueryType query_type, uint32_t query_count, VkQueryPipelineStatisticFlags pipeline_statistics) :
    Object(backend)
{
    VkQueryPoolCreateInfo query_pool_info;
    HELIOS_ZERO_MEMORY(query_pool_info);

    query_pool_info.sType              = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    query_pool_info.queryType          = query_type;
    query_pool_info.pipelineStatistics = pipeline_statistics;
    query_pool_info.queryCount         = query_count;

    if (vkCreateQueryPool(backend->device(), &query_pool_info, nullptr, &m_vk_query_pool) != VK_SUCCESS)
    {
        HELIOS_LOG_FATAL("(Vulkan) Failed to create Query Pool.");
        throw std::runtime_error("(Vulkan) Failed to create Query Pool.");
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

StagingBuffer::Ptr StagingBuffer::create(Backend::Ptr backend, const size_t& size)
{
    return std::shared_ptr<StagingBuffer>(new StagingBuffer(backend, size));
}

// -----------------------------------------------------------------------------------------------------------------------------------

StagingBuffer::StagingBuffer(Backend::Ptr backend, const size_t& size) :
    m_total_size(size)
{
    m_buffer     = Buffer::create(backend, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, size, VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT);
    m_mapped_ptr = (uint8_t*)m_buffer->mapped_ptr();
}

// -----------------------------------------------------------------------------------------------------------------------------------

StagingBuffer::~StagingBuffer()
{
    m_buffer.reset();
}

// -----------------------------------------------------------------------------------------------------------------------------------

size_t StagingBuffer::insert_data(void* data, const size_t& size)
{
    // If not enough space to insert the data, throw an error!
    if (size > remaining_size())
        throw std::runtime_error("(Vulkan) Not enough space available in Staging Buffer.");

    // Keep current size as the offset to the start of this data segment.
    size_t offset = m_current_size;

    // Copy data into the mapped buffer.
    memcpy(m_mapped_ptr, data, size);

    // Increment pointer and current size.
    m_mapped_ptr += size;
    m_current_size += size;

    // Return offset to the data segment.
    return offset;
}

// -----------------------------------------------------------------------------------------------------------------------------------

BatchUploader::BatchUploader(Backend::Ptr backend) :
    m_backend(backend)
{
    if (!m_backend.expired())
    {
        auto backend = m_backend.lock();
        m_cmd        = backend->allocate_graphics_command_buffer(true);
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

BatchUploader::~BatchUploader()
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

void BatchUploader::upload_buffer_data(Buffer::Ptr buffer, void* data, const size_t& offset, const size_t& size)
{
    if (!m_backend.expired())
    {
        auto backend = m_backend.lock();

        auto staging_buffer = insert_data(data, size);

        VkBufferCopy copy_region;
        HELIOS_ZERO_MEMORY(copy_region);

        copy_region.dstOffset = offset;
        copy_region.size      = size;

        vkCmdCopyBuffer(m_cmd->handle(), staging_buffer->handle(), buffer->handle(), 1, &copy_region);
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

void BatchUploader::upload_image_data(Image::Ptr image, void* data, const std::vector<size_t>& mip_level_sizes, VkImageLayout src_layout, VkImageLayout dst_layout)
{
    if (!m_backend.expired())
    {
        auto backend = m_backend.lock();

        size_t size = 0;

        for (const auto& region_size : mip_level_sizes)
            size += region_size;

        auto buffer = insert_data(data, size);

        std::vector<VkBufferImageCopy> copy_regions;
        size_t                         offset     = 0;
        uint32_t                       region_idx = 0;

        for (int array_idx = 0; array_idx < image->array_size(); array_idx++)
        {
            int width  = image->width();
            int height = image->height();

            for (int i = 0; i < image->mip_levels(); i++)
            {
                VkBufferImageCopy buffer_copy_region;
                HELIOS_ZERO_MEMORY(buffer_copy_region);

                buffer_copy_region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
                buffer_copy_region.imageSubresource.mipLevel       = i;
                buffer_copy_region.imageSubresource.baseArrayLayer = array_idx;
                buffer_copy_region.imageSubresource.layerCount     = 1;
                buffer_copy_region.imageExtent.width               = width;
                buffer_copy_region.imageExtent.height              = height;
                buffer_copy_region.imageExtent.depth               = 1;
                buffer_copy_region.bufferOffset                    = offset;

                copy_regions.push_back(buffer_copy_region);

                width  = std::max(1, width / 2);
                height = std::max(1, (height / 2));

                offset += mip_level_sizes[region_idx++];
            }
        }

        VkImageSubresourceRange subresource_range;
        HELIOS_ZERO_MEMORY(subresource_range);

        subresource_range.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        subresource_range.baseMipLevel   = 0;
        subresource_range.levelCount     = image->mip_levels();
        subresource_range.layerCount     = image->array_size();
        subresource_range.baseArrayLayer = 0;

        if (src_layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            // Image barrier for optimal image (target)
            // Optimal image will be used as destination for the copy
            utilities::set_image_layout(m_cmd->handle(),
                                        image->handle(),
                                        src_layout,
                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                        subresource_range);
        }

        // Copy mip levels from staging buffer
        vkCmdCopyBufferToImage(m_cmd->handle(),
                               buffer->handle(),
                               image->handle(),
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               copy_regions.size(),
                               copy_regions.data());

        if (dst_layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            // Change texture image layout to shader read after all mip levels have been copied
            utilities::set_image_layout(m_cmd->handle(),
                                        image->handle(),
                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                        dst_layout,
                                        subresource_range);
        }
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

void BatchUploader::build_blas(AccelerationStructure::Ptr acceleration_structure, const std::vector<VkAccelerationStructureGeometryKHR>& geometries, const std::vector<VkAccelerationStructureBuildRangeInfoKHR> build_ranges)
{
    if (geometries.size() > 0 || build_ranges.size() > 0)
        m_blas_build_requests.push_back({ acceleration_structure, geometries, build_ranges });
    else
    {
        HELIOS_LOG_FATAL("(Vulkan) Building a BLAS requires one or more Geometry and Build Offset structures.");
        throw std::runtime_error("(Vulkan) Building a BLAS requires one or more Geometry and Build Offset structures.");
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

Buffer::Ptr BatchUploader::insert_data(void* data, const size_t& size)
{
    add_staging_buffer(size);

    m_staging_buffers.top()->insert_data(data, size);

    return m_staging_buffers.top()->buffer();
}

// -----------------------------------------------------------------------------------------------------------------------------------

void BatchUploader::submit()
{
    if (!m_backend.expired())
    {
        auto backend = m_backend.lock();

        vk::Buffer::Ptr blas_scratch_buffer;

        if (m_blas_build_requests.size() > 0)
        {
            VkMemoryBarrier memory_barrier;
            memory_barrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            memory_barrier.pNext         = nullptr;
            memory_barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
            memory_barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

            VkAccelerationStructureMemoryRequirementsInfoKHR memory_requirements_info;
            memory_requirements_info.sType     = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_KHR;
            memory_requirements_info.pNext     = nullptr;
            memory_requirements_info.buildType = VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR;

            VkDeviceSize scratch_buffer_size = 0;

            for (int i = 0; i < m_blas_build_requests.size(); i++)
            {
                memory_requirements_info.type                  = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_KHR;
                memory_requirements_info.accelerationStructure = m_blas_build_requests[i].acceleration_structure->handle();

                VkMemoryRequirements2 mem_req_blas;
                HELIOS_ZERO_MEMORY(mem_req_blas);

                mem_req_blas.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;

                vkGetAccelerationStructureMemoryRequirementsKHR(backend->device(), &memory_requirements_info, &mem_req_blas);

                scratch_buffer_size = std::max(scratch_buffer_size, mem_req_blas.memoryRequirements.size);
            }

            blas_scratch_buffer = vk::Buffer::create(backend, VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, scratch_buffer_size, VMA_MEMORY_USAGE_GPU_ONLY, 0);

            for (int i = 0; i < m_blas_build_requests.size(); i++)
            {
                const VkAccelerationStructureGeometryKHR*        ptr_geometry     = &m_blas_build_requests[i].geometries[0];
                const VkAccelerationStructureBuildOffsetInfoKHR* ptr_build_offset = &m_blas_build_requests[i].build_offsets[0];

                VkAccelerationStructureBuildGeometryInfoKHR build_info;
                HELIOS_ZERO_MEMORY(build_info);

                build_info.sType                     = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
                build_info.type                      = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
                build_info.flags                     = m_blas_build_requests[i].acceleration_structure->info().flags;
                build_info.update                    = VK_FALSE;
                build_info.srcAccelerationStructure  = VK_NULL_HANDLE;
                build_info.dstAccelerationStructure  = m_blas_build_requests[i].acceleration_structure->handle();
                build_info.geometryArrayOfPointers   = VK_FALSE;
                build_info.geometryCount             = (uint32_t)m_blas_build_requests[i].geometries.size();
                build_info.ppGeometries              = &ptr_geometry;
                build_info.scratchData.deviceAddress = blas_scratch_buffer->device_address();

                vkCmdBuildAccelerationStructureKHR(m_cmd->handle(), 1, &build_info, &ptr_build_offset);

                vkCmdPipelineBarrier(m_cmd->handle(), VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &memory_barrier, 0, 0, 0, 0);
            }
        }

        vkEndCommandBuffer(m_cmd->handle());

        backend->flush_graphics({ m_cmd });

        blas_scratch_buffer.reset();
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

void BatchUploader::add_staging_buffer(const size_t& size)
{
    if (!m_backend.expired())
    {
        auto backend = m_backend.lock();
        m_staging_buffers.push(StagingBuffer::create(backend, size));
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

Backend::Ptr Backend::create(GLFWwindow* window, bool enable_validation_layers, bool require_ray_tracing, std::vector<const char*> additional_device_extensions)
{
    std::shared_ptr<Backend> backend = std::shared_ptr<Backend>(new Backend(window, enable_validation_layers, require_ray_tracing, additional_device_extensions));
    backend->initialize();

    return backend;
}

// -----------------------------------------------------------------------------------------------------------------------------------

Backend::Backend(GLFWwindow* window, bool enable_validation_layers, bool require_ray_tracing, std::vector<const char*> additional_device_extensions) :
    m_window(window)
{
    m_ray_tracing_enabled = require_ray_tracing;

    VkApplicationInfo appInfo;
    HELIOS_ZERO_MEMORY(appInfo);

    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "Helios";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName        = "Helios";
    appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_2;

    std::vector<const char*> extensions = required_extensions(enable_validation_layers);

    extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

    VkInstanceCreateInfo create_info;
    HELIOS_ZERO_MEMORY(create_info);

    create_info.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo        = &appInfo;
    create_info.enabledExtensionCount   = extensions.size();
    create_info.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debug_create_info;

    if (enable_validation_layers)
    {
        VkValidationFeatureEnableEXT enabled_features[] = { VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT };

        VkValidationFeaturesEXT validation_features;
        HELIOS_ZERO_MEMORY(validation_features);

        validation_features.sType                         = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
        validation_features.enabledValidationFeatureCount = 1;
        validation_features.pEnabledValidationFeatures    = enabled_features;

        HELIOS_ZERO_MEMORY(debug_create_info);
        create_info.enabledLayerCount   = static_cast<uint32_t>(kValidationLayers.size());
        create_info.ppEnabledLayerNames = kValidationLayers.data();

        debug_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
#if defined(ENABLE_GPU_ASSISTED_VALIDATION)
        debug_create_info.pNext = &validation_features;
#endif
        debug_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debug_create_info.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debug_create_info.pfnUserCallback = debug_callback;

        create_info.pNext = &debug_create_info;
    }
    else
    {
        create_info.enabledLayerCount = 0;
        create_info.pNext             = nullptr;
    }

    if (vkCreateInstance(&create_info, nullptr, &m_vk_instance) != VK_SUCCESS)
    {
        HELIOS_LOG_FATAL("(Vulkan) Failed to create Vulkan instance.");
        throw std::runtime_error("(Vulkan) Failed to create Vulkan instance.");
    }

    if (enable_validation_layers && create_debug_utils_messenger(m_vk_instance, &debug_create_info, nullptr, &m_vk_debug_messenger) != VK_SUCCESS)
        HELIOS_LOG_FATAL("(Vulkan) Failed to create Vulkan debug messenger.");

    if (!create_surface(window))
    {
        HELIOS_LOG_FATAL("(Vulkan) Failed to create Vulkan surface.");
        throw std::runtime_error("(Vulkan) Failed to create Vulkan surface.");
    }

    std::vector<const char*> device_extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    device_extensions.push_back(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME);
    device_extensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
    device_extensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
    device_extensions.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);
    device_extensions.push_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);
    device_extensions.push_back(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
    device_extensions.push_back(VK_KHR_MAINTENANCE3_EXTENSION_NAME);
    device_extensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
    device_extensions.push_back(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
    device_extensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
    device_extensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);

    for (auto ext : additional_device_extensions)
        device_extensions.push_back(ext);

    if (!find_physical_device(device_extensions))
    {
        HELIOS_LOG_FATAL("(Vulkan) Failed to find a suitable GPU.");
        throw std::runtime_error("(Vulkan) Failed to find a suitable GPU.");
    }

    if (!create_logical_device(device_extensions))
    {
        HELIOS_LOG_FATAL("(Vulkan) Failed to create logical device.");
        throw std::runtime_error("(Vulkan) Failed to create logical device.");
    }

    VmaAllocatorCreateInfo allocator_info = {};
    allocator_info.flags                  = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    allocator_info.physicalDevice         = m_vk_physical_device;
    allocator_info.device                 = m_vk_device;
    allocator_info.instance               = m_vk_instance;

    if (vmaCreateAllocator(&allocator_info, &m_vma_allocator) != VK_SUCCESS)
    {
        HELIOS_LOG_FATAL("(Vulkan) Failed to create Allocator.");
        throw std::runtime_error("(Vulkan) Failed to create Allocator.");
    }

    load_VK_EXTENSION_SUBSET(m_vk_instance, vkGetInstanceProcAddr, m_vk_device, vkGetDeviceProcAddr);
}

// -----------------------------------------------------------------------------------------------------------------------------------

Backend::~Backend()
{
    while (!m_deletion_queue.empty())
    {
        auto front = m_deletion_queue.front();
        wait_for_frame(front.second);
        m_deletion_queue.pop_front();
    }

    m_default_cubemap_image_view.reset();
    m_default_cubemap_image.reset();
    m_bilinear_sampler.reset();
    m_trilinear_sampler.reset();
    m_nearest_sampler.reset();
    m_ray_debug_descriptor_set_layout.reset();
    m_combined_sampler_array_descriptor_set_layout.reset();
    m_buffer_array_descriptor_set_layout.reset();
    m_combined_sampler_descriptor_set_layout.reset();
    m_image_descriptor_set_layout.reset();
    m_scene_descriptor_set_layout.reset();

    for (int i = 0; i < MAX_COMMAND_THREADS; i++)
    {
        g_graphics_command_buffers[i].reset();
        g_compute_command_buffers[i].reset();
        g_transfer_command_buffers[i].reset();
    }

    for (int i = 0; i < MAX_DESCRIPTOR_POOL_THREADS; i++)
        g_descriptor_pools[i].reset();

    for (int i = 0; i < m_swap_chain_images.size(); i++)
    {
        m_swap_chain_framebuffers[i].reset();
        m_swap_chain_image_views[i].reset();
    }

    for (int i = 0; i < m_in_flight_fences.size(); i++)
        m_in_flight_fences[i].reset();

    m_swap_chain_render_pass.reset();
    m_swap_chain_depth_view.reset();
    m_swap_chain_depth.reset();

    if (m_vk_debug_messenger)
    {
        destroy_debug_utils_messenger(m_vk_instance, m_vk_debug_messenger, nullptr);
        m_vk_debug_messenger = nullptr;
    }

    if (m_vk_swap_chain)
    {
        vkDestroySwapchainKHR(m_vk_device, m_vk_swap_chain, nullptr);
        m_vk_swap_chain = nullptr;
    }

    if (m_vk_surface)
    {
        vkDestroySurfaceKHR(m_vk_instance, m_vk_surface, nullptr);
        m_vk_surface = nullptr;
    }

    if (m_vma_allocator)
    {
        vmaDestroyAllocator(m_vma_allocator);
        m_vma_allocator = nullptr;
    }

    if (m_vk_device)
    {
        vkDestroyDevice(m_vk_device, nullptr);
        m_vk_device = nullptr;
    }

    if (m_vk_instance)
    {
        vkDestroyInstance(m_vk_instance, nullptr);
        m_vk_instance = nullptr;
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Backend::initialize()
{
    create_swapchain();

    // Create Descriptor Pools
    DescriptorPool::Desc dp_desc;

    dp_desc.set_max_sets(512)
        .add_pool_size(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 32)
        .add_pool_size(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 4)
        .add_pool_size(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 256)
        .add_pool_size(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 32)
        .add_pool_size(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 32)
        .add_pool_size(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 16)
        .add_pool_size(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 16);

    for (int i = 0; i < MAX_DESCRIPTOR_POOL_THREADS; i++)
        g_descriptor_pools[i] = DescriptorPool::create(shared_from_this(), dp_desc);

    for (int i = 0; i < MAX_COMMAND_THREADS; i++)
    {
        g_graphics_command_buffers[i] = std::make_shared<ThreadLocalCommandBuffers>(shared_from_this(), m_selected_queues.graphics_queue_index);
        g_compute_command_buffers[i]  = std::make_shared<ThreadLocalCommandBuffers>(shared_from_this(), m_selected_queues.compute_queue_index);
        g_transfer_command_buffers[i] = std::make_shared<ThreadLocalCommandBuffers>(shared_from_this(), m_selected_queues.transfer_queue_index);
    }

    DescriptorSetLayout::Desc scene_ds_layout_desc;

    // Material Data
    scene_ds_layout_desc.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR);
    // Instance Data
    scene_ds_layout_desc.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR);
    // Light Data
    scene_ds_layout_desc.add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR);
    // Acceleration Structures
    scene_ds_layout_desc.add_binding(3, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR);
    // Environment Map
    scene_ds_layout_desc.add_binding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR);

    m_scene_descriptor_set_layout = DescriptorSetLayout::create(shared_from_this(), scene_ds_layout_desc);

    std::vector<VkDescriptorBindingFlagsEXT> descriptor_binding_flags = {
        VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT
    };

    VkDescriptorSetLayoutBindingFlagsCreateInfoEXT set_layout_binding_flags;
    HELIOS_ZERO_MEMORY(set_layout_binding_flags);

    set_layout_binding_flags.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
    set_layout_binding_flags.bindingCount  = 1;
    set_layout_binding_flags.pBindingFlags = descriptor_binding_flags.data();

    // Buffers
    DescriptorSetLayout::Desc buffer_array_ds_layout_desc;

    buffer_array_ds_layout_desc.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_SCENE_MESH_INSTANCE_COUNT, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR);
    buffer_array_ds_layout_desc.set_next_ptr(&set_layout_binding_flags);

    m_buffer_array_descriptor_set_layout = DescriptorSetLayout::create(shared_from_this(), buffer_array_ds_layout_desc);

    // Material Textures
    DescriptorSetLayout::Desc combined_sampler_array_ds_layout_desc;

    combined_sampler_array_ds_layout_desc.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_SCENE_MATERIAL_TEXTURE_COUNT, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR);
    combined_sampler_array_ds_layout_desc.set_next_ptr(&set_layout_binding_flags);

    m_combined_sampler_array_descriptor_set_layout = DescriptorSetLayout::create(shared_from_this(), combined_sampler_array_ds_layout_desc);

    DescriptorSetLayout::Desc image_ds_layout_desc;

    image_ds_layout_desc.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR);

    m_image_descriptor_set_layout = DescriptorSetLayout::create(shared_from_this(), image_ds_layout_desc);

    DescriptorSetLayout::Desc combined_sampler_ds_layout_desc;

    combined_sampler_ds_layout_desc.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);

    m_combined_sampler_descriptor_set_layout = DescriptorSetLayout::create(shared_from_this(), combined_sampler_ds_layout_desc);

    DescriptorSetLayout::Desc ray_debug_ds_layout_desc;

    ray_debug_ds_layout_desc.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR);
    ray_debug_ds_layout_desc.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR);

    m_ray_debug_descriptor_set_layout = DescriptorSetLayout::create(shared_from_this(), ray_debug_ds_layout_desc);

    Sampler::Desc sampler_desc;

    sampler_desc.mag_filter        = VK_FILTER_LINEAR;
    sampler_desc.min_filter        = VK_FILTER_LINEAR;
    sampler_desc.mipmap_mode       = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler_desc.address_mode_u    = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_desc.address_mode_v    = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_desc.address_mode_w    = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_desc.mip_lod_bias      = 0.0f;
    sampler_desc.anisotropy_enable = VK_FALSE;
    sampler_desc.max_anisotropy    = 1.0f;
    sampler_desc.compare_enable    = false;
    sampler_desc.compare_op        = VK_COMPARE_OP_NEVER;
    sampler_desc.min_lod           = 0.0f;
    sampler_desc.max_lod           = 12.0f;
    sampler_desc.border_color      = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    sampler_desc.unnormalized_coordinates;

    m_bilinear_sampler = Sampler::create(shared_from_this(), sampler_desc);

    sampler_desc.mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    m_trilinear_sampler = Sampler::create(shared_from_this(), sampler_desc);

    sampler_desc.mag_filter = VK_FILTER_NEAREST;
    sampler_desc.min_filter = VK_FILTER_NEAREST;

    m_nearest_sampler = Sampler::create(shared_from_this(), sampler_desc);

    m_default_cubemap_image      = Image::create(shared_from_this(), VK_IMAGE_TYPE_2D, 2, 2, 1, 1, 6, VK_FORMAT_R32G32B32A32_SFLOAT, VMA_MEMORY_USAGE_GPU_ONLY, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_UNDEFINED, 0, nullptr, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);
    m_default_cubemap_image_view = ImageView::create(shared_from_this(), m_default_cubemap_image, VK_IMAGE_VIEW_TYPE_CUBE, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6);

    std::vector<glm::vec4> cubemap_data(2 * 2 * 6);
    std::vector<size_t>    cubemap_sizes(6);

    int idx = 0;

    for (int layer = 0; layer < 6; layer++)
    {
        cubemap_sizes[layer] = sizeof(glm::vec4) * 4;

        for (int i = 0; i < 4; i++)
            cubemap_data[idx++] = glm::vec4(0.0f);
    }

    BatchUploader uploader(shared_from_this());

    uploader.upload_image_data(m_default_cubemap_image, cubemap_data.data(), cubemap_sizes);

    uploader.submit();
}

// -----------------------------------------------------------------------------------------------------------------------------------

std::shared_ptr<DescriptorSet> Backend::allocate_descriptor_set(std::shared_ptr<DescriptorSetLayout> layout)
{
    return DescriptorSet::create(shared_from_this(), layout, g_descriptor_pools[g_thread_idx]);
}

// -----------------------------------------------------------------------------------------------------------------------------------

std::shared_ptr<CommandBuffer> Backend::allocate_graphics_command_buffer(bool begin)
{
    return g_graphics_command_buffers[g_thread_idx]->allocate(m_current_frame, begin);
}

// -----------------------------------------------------------------------------------------------------------------------------------

std::shared_ptr<CommandBuffer> Backend::allocate_compute_command_buffer(bool begin)
{
    return g_compute_command_buffers[g_thread_idx]->allocate(m_current_frame, begin);
}

// -----------------------------------------------------------------------------------------------------------------------------------

std::shared_ptr<CommandBuffer> Backend::allocate_transfer_command_buffer(bool begin)
{
    return g_transfer_command_buffers[g_thread_idx]->allocate(m_current_frame, begin);
}

// -----------------------------------------------------------------------------------------------------------------------------------

std::shared_ptr<CommandPool> Backend::thread_local_graphics_command_pool()
{
    return g_graphics_command_buffers[g_thread_idx]->command_pool[m_current_frame];
}

// -----------------------------------------------------------------------------------------------------------------------------------

std::shared_ptr<CommandPool> Backend::thread_local_compute_command_pool()
{
    return g_compute_command_buffers[g_thread_idx]->command_pool[m_current_frame];
}

// -----------------------------------------------------------------------------------------------------------------------------------

std::shared_ptr<CommandPool> Backend::thread_local_transfer_command_pool()
{
    return g_transfer_command_buffers[g_thread_idx]->command_pool[m_current_frame];
}

// -----------------------------------------------------------------------------------------------------------------------------------

std::shared_ptr<DescriptorPool> Backend::thread_local_descriptor_pool()
{
    return g_descriptor_pools[g_thread_idx];
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Backend::submit_graphics(const std::vector<std::shared_ptr<CommandBuffer>>& cmd_bufs,
                              const std::vector<std::shared_ptr<Semaphore>>&     wait_semaphores,
                              const std::vector<VkPipelineStageFlags>&           wait_stages,
                              const std::vector<std::shared_ptr<Semaphore>>&     signal_semaphores)
{
    submit(m_vk_graphics_queue, cmd_bufs, wait_semaphores, wait_stages, signal_semaphores);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Backend::submit_compute(const std::vector<std::shared_ptr<CommandBuffer>>& cmd_bufs,
                             const std::vector<std::shared_ptr<Semaphore>>&     wait_semaphores,
                             const std::vector<VkPipelineStageFlags>&           wait_stages,
                             const std::vector<std::shared_ptr<Semaphore>>&     signal_semaphores)
{
    submit(m_vk_compute_queue, cmd_bufs, wait_semaphores, wait_stages, signal_semaphores);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Backend::submit_transfer(const std::vector<std::shared_ptr<CommandBuffer>>& cmd_bufs,
                              const std::vector<std::shared_ptr<Semaphore>>&     wait_semaphores,
                              const std::vector<VkPipelineStageFlags>&           wait_stages,
                              const std::vector<std::shared_ptr<Semaphore>>&     signal_semaphores)
{
    submit(m_vk_transfer_queue, cmd_bufs, wait_semaphores, wait_stages, signal_semaphores);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Backend::flush_graphics(const std::vector<std::shared_ptr<CommandBuffer>>& cmd_bufs)
{
    flush(m_vk_graphics_queue, cmd_bufs);

    for (int i = 0; i < MAX_COMMAND_THREADS; i++)
        g_graphics_command_buffers[i]->reset(m_current_frame);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Backend::flush_compute(const std::vector<std::shared_ptr<CommandBuffer>>& cmd_bufs)
{
    flush(m_vk_compute_queue, cmd_bufs);

    for (int i = 0; i < MAX_COMMAND_THREADS; i++)
        g_compute_command_buffers[i]->reset(m_current_frame);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Backend::flush_transfer(const std::vector<std::shared_ptr<CommandBuffer>>& cmd_bufs)
{
    flush(m_vk_transfer_queue, cmd_bufs);

    for (int i = 0; i < MAX_COMMAND_THREADS; i++)
        g_transfer_command_buffers[i]->reset(m_current_frame);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Backend::submit(VkQueue                                            queue,
                     const std::vector<std::shared_ptr<CommandBuffer>>& cmd_bufs,
                     const std::vector<std::shared_ptr<Semaphore>>&     wait_semaphores,
                     const std::vector<VkPipelineStageFlags>&           wait_stages,
                     const std::vector<std::shared_ptr<Semaphore>>&     signal_semaphores)
{
    VkSemaphore vk_wait_semaphores[16];

    for (int i = 0; i < wait_semaphores.size(); i++)
        vk_wait_semaphores[i] = wait_semaphores[i]->handle();

    VkSemaphore vk_signal_semaphores[16];

    for (int i = 0; i < signal_semaphores.size(); i++)
        vk_signal_semaphores[i] = signal_semaphores[i]->handle();

    VkCommandBuffer vk_cmd_bufs[32];

    for (int i = 0; i < cmd_bufs.size(); i++)
        vk_cmd_bufs[i] = cmd_bufs[i]->handle();

    VkPipelineStageFlags vk_wait_stages[16];

    for (int i = 0; i < wait_semaphores.size(); i++)
        vk_wait_stages[i] = wait_stages[i];

    VkSubmitInfo submit_info;
    HELIOS_ZERO_MEMORY(submit_info);

    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    submit_info.waitSemaphoreCount = wait_semaphores.size();
    submit_info.pWaitSemaphores    = vk_wait_semaphores;
    submit_info.pWaitDstStageMask  = vk_wait_stages;

    submit_info.commandBufferCount = cmd_bufs.size();
    submit_info.pCommandBuffers    = &vk_cmd_bufs[0];

    submit_info.signalSemaphoreCount = signal_semaphores.size();
    submit_info.pSignalSemaphores    = vk_signal_semaphores;

    vkResetFences(m_vk_device, 1, &m_in_flight_fences[m_current_frame]->handle());

    if (vkQueueSubmit(queue, 1, &submit_info, m_in_flight_fences[m_current_frame]->handle()) != VK_SUCCESS)
    {
        HELIOS_LOG_FATAL("(Vulkan) Failed to submit command buffer!");
        throw std::runtime_error("(Vulkan) Failed to submit command buffer!");
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Backend::flush(VkQueue queue, const std::vector<std::shared_ptr<CommandBuffer>>& cmd_bufs)
{
    VkCommandBuffer vk_cmd_bufs[32];

    for (int i = 0; i < cmd_bufs.size(); i++)
        vk_cmd_bufs[i] = cmd_bufs[i]->handle();

    VkSubmitInfo submit_info;
    HELIOS_ZERO_MEMORY(submit_info);

    submit_info.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers    = &vk_cmd_bufs[0];

    // Create fence to ensure that the command buffer has finished executing
    VkFenceCreateInfo fence_info;
    HELIOS_ZERO_MEMORY(fence_info);

    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    VkFence fence;
    vkCreateFence(m_vk_device, &fence_info, nullptr, &fence);

    // Submit to the queue
    vkQueueSubmit(queue, 1, &submit_info, fence);

    // Wait for the fence to signal that command buffer has finished executing
    vkWaitForFences(m_vk_device, 1, &fence, VK_TRUE, 100000000000);

    vkDestroyFence(m_vk_device, fence, nullptr);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Backend::acquire_next_swap_chain_image(const std::shared_ptr<Semaphore>& semaphore)
{
    vkWaitForFences(m_vk_device, 1, &m_in_flight_fences[m_current_frame]->handle(), VK_TRUE, UINT64_MAX);

    for (int i = 0; i < MAX_COMMAND_THREADS; i++)
    {
        g_graphics_command_buffers[i]->reset(m_current_frame);
        g_compute_command_buffers[i]->reset(m_current_frame);
        g_transfer_command_buffers[i]->reset(m_current_frame);
    }

    VkResult result = vkAcquireNextImageKHR(m_vk_device, m_vk_swap_chain, UINT64_MAX, semaphore->handle(), VK_NULL_HANDLE, &m_image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        recreate_swapchain();
        return;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        HELIOS_LOG_FATAL("(Vulkan) Failed to acquire swap chain image!");
        throw std::runtime_error("(Vulkan) Failed to acquire swap chain image!");
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Backend::present(const std::vector<std::shared_ptr<Semaphore>>& semaphores)
{
    VkSemaphore signal_semaphores[16];

    for (int i = 0; i < semaphores.size(); i++)
        signal_semaphores[i] = semaphores[i]->handle();

    VkPresentInfoKHR present_info;
    HELIOS_ZERO_MEMORY(present_info);

    present_info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = semaphores.size();
    present_info.pWaitSemaphores    = signal_semaphores;

    VkSwapchainKHR swap_chains[] = { m_vk_swap_chain };
    present_info.swapchainCount  = 1;
    present_info.pSwapchains     = swap_chains;
    present_info.pImageIndices   = &m_image_index;

    if (vkQueuePresentKHR(m_vk_presentation_queue, &present_info) != VK_SUCCESS)
    {
        HELIOS_LOG_FATAL("(Vulkan) Failed to submit draw command buffer!");
        throw std::runtime_error("failed to present swap chain image!");
    }

    m_current_frame = (m_current_frame + 1) % kMaxFramesInFlight;
}

// -----------------------------------------------------------------------------------------------------------------------------------

bool Backend::is_frame_done(uint32_t idx)
{
    if (idx < kMaxFramesInFlight)
        return vkGetFenceStatus(m_vk_device, m_in_flight_fences[idx]->handle()) == VK_SUCCESS;
    else
        return false;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Backend::wait_for_frame(uint32_t idx)
{
    if (idx < kMaxFramesInFlight)
        vkWaitForFences(m_vk_device, 1, &m_in_flight_fences[idx]->handle(), VK_TRUE, 100000000000);
}

// -----------------------------------------------------------------------------------------------------------------------------------

Image::Ptr Backend::swapchain_image()
{
    return m_swap_chain_images[m_current_frame];
}

// -----------------------------------------------------------------------------------------------------------------------------------

ImageView::Ptr Backend::swapchain_image_view()
{
    return m_swap_chain_image_views[m_current_frame];
}

// -----------------------------------------------------------------------------------------------------------------------------------

Framebuffer::Ptr Backend::swapchain_framebuffer()
{
    return m_swap_chain_framebuffers[m_current_frame];
}

// -----------------------------------------------------------------------------------------------------------------------------------

RenderPass::Ptr Backend::swapchain_render_pass()
{
    return m_swap_chain_render_pass;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Backend::wait_idle()
{
    vkDeviceWaitIdle(m_vk_device);
}

// -----------------------------------------------------------------------------------------------------------------------------------

uint32_t Backend::swap_image_count()
{
    return m_swap_chain_images.size();
}

// -----------------------------------------------------------------------------------------------------------------------------------

VkInstance Backend::instance()
{
    return m_vk_instance;
}

// -----------------------------------------------------------------------------------------------------------------------------------

VkQueue Backend::graphics_queue()
{
    return m_vk_graphics_queue;
}

// -----------------------------------------------------------------------------------------------------------------------------------

VkQueue Backend::transfer_queue()
{
    return m_vk_transfer_queue;
}

// -----------------------------------------------------------------------------------------------------------------------------------

VkQueue Backend::compute_queue()
{
    return m_vk_compute_queue;
}

// -----------------------------------------------------------------------------------------------------------------------------------

VkDevice Backend::device()
{
    return m_vk_device;
}

// -----------------------------------------------------------------------------------------------------------------------------------

VkPhysicalDevice Backend::physical_device()
{
    return m_vk_physical_device;
}

// -----------------------------------------------------------------------------------------------------------------------------------

VmaAllocator_T* Backend::allocator()
{
    return m_vma_allocator;
}

// -----------------------------------------------------------------------------------------------------------------------------------

size_t Backend::min_dynamic_ubo_alignment()
{
    return m_device_properties.limits.minUniformBufferOffsetAlignment;
}

// -----------------------------------------------------------------------------------------------------------------------------------

size_t Backend::aligned_dynamic_ubo_size(size_t size)
{
    size_t min_ubo_alignment = m_device_properties.limits.minUniformBufferOffsetAlignment;
    size_t aligned_size      = size;

    if (min_ubo_alignment > 0)
        aligned_size = (aligned_size + min_ubo_alignment - 1) & ~(min_ubo_alignment - 1);

    return aligned_size;
}

// -----------------------------------------------------------------------------------------------------------------------------------

VkFormat Backend::find_supported_format(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
    for (VkFormat format : candidates)
    {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(m_vk_physical_device, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
        {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
        {
            return format;
        }
    }

    throw std::runtime_error("Failed to find supported format!");
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Backend::process_deletion_queue()
{
    while (!m_deletion_queue.empty())
    {
        auto front = m_deletion_queue.front();

        if (is_frame_done(front.second))
            m_deletion_queue.pop_front();
        else
            return;
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Backend::queue_object_deletion(std::shared_ptr<Object> object)
{
    m_deletion_queue.push_back({ object, m_current_frame });
}

// -----------------------------------------------------------------------------------------------------------------------------------

VkFormat Backend::find_depth_format()
{
    return find_supported_format({ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT }, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

// -----------------------------------------------------------------------------------------------------------------------------------

bool Backend::check_device_extension_support(VkPhysicalDevice device, std::vector<const char*> extensions)
{
    uint32_t extension_count;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);

    std::vector<VkExtensionProperties> available_extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, &available_extensions[0]);

    int unavailable_extensions = extensions.size();

    for (auto& str : extensions)
    {
        for (const auto& extension : available_extensions)
        {
            if (strcmp(str, extension.extensionName) == 0)
                unavailable_extensions--;
        }
    }

    return unavailable_extensions == 0;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Backend::query_swap_chain_support(VkPhysicalDevice device, SwapChainSupportDetails& details)
{
    // Get surface capabilities
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_vk_surface, &details.capabilities);

    uint32_t present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_vk_surface, &present_mode_count, nullptr);

    if (present_mode_count != 0)
    {
        details.present_modes.resize(present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_vk_surface, &present_mode_count, &details.present_modes[0]);
    }

    uint32_t format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_vk_surface, &format_count, nullptr);

    if (format_count != 0)
    {
        details.format.resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_vk_surface, &format_count, &details.format[0]);
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

bool Backend::check_validation_layer_support(std::vector<const char*> layers)
{
    uint32_t layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

    VkLayerProperties available_layers[32];
    vkEnumerateInstanceLayerProperties(&layer_count, &available_layers[0]);

    for (const char* layer_name : layers)
    {
        bool layer_found = false;

        for (const auto& layer_properties : available_layers)
        {
            if (std::string(layer_name) == std::string(layer_properties.layerName))
            {
                layer_found = true;
                break;
            }
        }

        if (!layer_found)
        {
            HELIOS_LOG_FATAL("(Vulkan) Validation Layer not available: " + std::string(layer_name));
            return false;
        }
    }

    return true;
}

// -----------------------------------------------------------------------------------------------------------------------------------

std::vector<const char*> Backend::required_extensions(bool enable_validation_layers)
{
    uint32_t     glfw_extension_count = 0;
    const char** glfw_extensions;
    glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    std::vector<const char*> extensions(glfw_extensions, glfw_extensions + glfw_extension_count);

    if (enable_validation_layers)
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    return extensions;
}
// -----------------------------------------------------------------------------------------------------------------------------------

VkResult Backend::create_debug_utils_messenger(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

    if (func != nullptr)
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    else
        return VK_ERROR_EXTENSION_NOT_PRESENT;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Backend::destroy_debug_utils_messenger(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

    if (func != nullptr)
        func(instance, debugMessenger, pAllocator);
}

// -----------------------------------------------------------------------------------------------------------------------------------

bool Backend::create_surface(GLFWwindow* window)
{
    return glfwCreateWindowSurface(m_vk_instance, window, nullptr, &m_vk_surface) == VK_SUCCESS;
}

// -----------------------------------------------------------------------------------------------------------------------------------

bool Backend::find_physical_device(std::vector<const char*> extensions)
{
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(m_vk_instance, &device_count, nullptr);

    if (device_count == 0)
    {
        HELIOS_LOG_FATAL("(Vulkan) Failed to find GPUs with Vulkan support!");
        throw std::runtime_error("(Vulkan) Failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(device_count);

    vkEnumeratePhysicalDevices(m_vk_instance, &device_count, devices.data());

    // Try to find a discrete GPU...
    for (const auto& device : devices)
    {
        QueueInfos              infos;
        SwapChainSupportDetails details;

        if (is_device_suitable(device, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, infos, details, extensions))
        {
            m_vk_physical_device = device;
            m_selected_queues    = infos;
            m_swapchain_details  = details;
            return true;
        }
    }

    // ...If not, try to find an integrated GPU...
    for (const auto& device : devices)
    {
        QueueInfos              infos;
        SwapChainSupportDetails details;

        if (is_device_suitable(device, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, infos, details, extensions))
        {
            m_vk_physical_device = device;
            m_selected_queues    = infos;
            m_swapchain_details  = details;
            return true;
        }
    }

    return false;
}

// -----------------------------------------------------------------------------------------------------------------------------------

bool Backend::is_device_suitable(VkPhysicalDevice device, VkPhysicalDeviceType type, QueueInfos& infos, SwapChainSupportDetails& details, std::vector<const char*> extensions)
{
    vkGetPhysicalDeviceProperties(device, &m_device_properties);

    uint32_t vendorId = m_device_properties.vendorID;

    bool requires_ray_tracing = false;

    for (auto& ext : extensions)
    {
        if (strcmp(ext, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME) == 0)
        {
            requires_ray_tracing = true;
            break;
        }
    }

    if (m_device_properties.deviceType == type)
    {
        bool extensions_supported = check_device_extension_support(device, extensions);
        query_swap_chain_support(device, details);

        if (details.format.size() > 0 && details.present_modes.size() > 0 && extensions_supported)
        {
            HELIOS_LOG_INFO("(Vulkan) Vendor : " + std::string(get_vendor_name(m_device_properties.vendorID)));
            HELIOS_LOG_INFO("(Vulkan) Name   : " + std::string(m_device_properties.deviceName));
            HELIOS_LOG_INFO("(Vulkan) Type   : " + std::string(kDeviceTypes[m_device_properties.deviceType]));
            HELIOS_LOG_INFO("(Vulkan) Driver : " + std::to_string(m_device_properties.driverVersion));

            if (requires_ray_tracing)
            {
                // Get ray tracing pipeline properties
                HELIOS_ZERO_MEMORY(m_ray_tracing_pipeline_properties);
                m_ray_tracing_pipeline_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
                
                VkPhysicalDeviceProperties2 device_properties2 {};
                device_properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
                device_properties2.pNext = &m_ray_tracing_pipeline_properties;
                vkGetPhysicalDeviceProperties2(device, &device_properties2);

                // Get acceleration structure properties
                HELIOS_ZERO_MEMORY(m_acceleration_structure_properties);
                m_acceleration_structure_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
                
                VkPhysicalDeviceFeatures2 device_features2 {};
                device_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
                device_features2.pNext = &m_acceleration_structure_properties;
                vkGetPhysicalDeviceFeatures2(device, &device_features2);
            }

            return find_queues(device, infos);
        }
    }

    return false;
}

// -----------------------------------------------------------------------------------------------------------------------------------

bool Backend::find_queues(VkPhysicalDevice device, QueueInfos& infos)
{
    uint32_t family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &family_count, nullptr);

    HELIOS_LOG_INFO("(Vulkan) Number of Queue families: " + std::to_string(family_count));

    VkQueueFamilyProperties families[32];
    vkGetPhysicalDeviceQueueFamilyProperties(device, &family_count, &families[0]);

    for (uint32_t i = 0; i < family_count; i++)
    {
        VkQueueFlags bits = families[i].queueFlags;

        HELIOS_LOG_INFO("(Vulkan) Family " + std::to_string(i));
        HELIOS_LOG_INFO("(Vulkan) Supported Bits: ");
        HELIOS_LOG_INFO("(Vulkan) VK_QUEUE_GRAPHICS_BIT: " + std::to_string((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) > 0));
        HELIOS_LOG_INFO("(Vulkan) VK_QUEUE_COMPUTE_BIT: " + std::to_string((families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) > 0));
        HELIOS_LOG_INFO("(Vulkan) VK_QUEUE_TRANSFER_BIT: " + std::to_string((families[i].queueFlags & VK_QUEUE_TRANSFER_BIT) > 0));
        HELIOS_LOG_INFO("(Vulkan) Number of Queues: " + std::to_string(families[i].queueCount));

        VkBool32 present_support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_vk_surface, &present_support);

        // Look for Presentation Queue
        if (present_support && infos.presentation_queue_index == -1)
            infos.presentation_queue_index = i;

        // Look for a graphics queue if the ideal one isn't found yet.
        if (infos.graphics_queue_quality != 3)
        {
            if (is_queue_compatible(bits, 1, 1, 1))
            {
                // Ideally, a queue that supports everything.
                infos.graphics_queue_index   = i;
                infos.graphics_queue_quality = 3;
            }
            else if (is_queue_compatible(bits, 1, -1, -1))
            {
                // If not, a queue that supports at least graphics.
                infos.graphics_queue_index   = i;
                infos.graphics_queue_quality = 1;
            }
        }

        // Look for a compute queue if the ideal one isn't found yet.
        if (infos.compute_queue_quality != 3)
        {
            if (is_queue_compatible(bits, 0, 1, 0))
            {
                // Ideally, a queue that only supports compute (for asynchronous compute).
                infos.compute_queue_index   = i;
                infos.compute_queue_quality = 3;
            }
            else if (is_queue_compatible(bits, 0, 1, 1))
            {
                // Else, a queue that supports compute and transfer only (might allow asynchronous compute. Have to check).
                infos.compute_queue_index   = i;
                infos.compute_queue_quality = 2;
            }
            else if (is_queue_compatible(bits, -1, 1, -1) && infos.compute_queue_quality == 0)
            {
                // If not, a queue that supports at least compute
                infos.compute_queue_index   = i;
                infos.compute_queue_quality = 1;
            }
        }

        // Look for a Transfer queue if the ideal one isn't found yet.
        if (infos.transfer_queue_quality != 3)
        {
            if (is_queue_compatible(bits, 0, 0, 1))
            {
                // Ideally, a queue that only supports transfer (for DMA).
                infos.transfer_queue_index   = i;
                infos.transfer_queue_quality = 3;
            }
            else if (is_queue_compatible(bits, 0, 1, 1))
            {
                // Else, a queue that supports compute and transfer only.
                infos.transfer_queue_index   = i;
                infos.transfer_queue_quality = 2;
            }
            else if (is_queue_compatible(bits, -1, -1, 1) && infos.transfer_queue_quality == 0)
            {
                // If not, a queue that supports at least graphics
                infos.transfer_queue_index   = i;
                infos.transfer_queue_quality = 1;
            }
        }
    }

    if (infos.presentation_queue_index == -1)
    {
        HELIOS_LOG_INFO("(Vulkan) No Presentation Queue Found");
        return false;
    }

    if (infos.graphics_queue_quality == 0)

    {
        HELIOS_LOG_INFO("(Vulkan) No Graphics Queue Found");
        return false;
    }

    if (infos.compute_queue_quality == 0 || infos.transfer_queue_quality == 0)
    {
        HELIOS_LOG_INFO("(Vulkan) No Queues supporting Compute or Transfer found");
        return false;
    }

    VkDeviceQueueCreateInfo presentation_queue_info;
    HELIOS_ZERO_MEMORY(presentation_queue_info);

    presentation_queue_info.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    presentation_queue_info.queueFamilyIndex = infos.presentation_queue_index;
    presentation_queue_info.queueCount       = 1;

    VkDeviceQueueCreateInfo graphics_queue_info;
    HELIOS_ZERO_MEMORY(graphics_queue_info);

    graphics_queue_info.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    graphics_queue_info.queueFamilyIndex = infos.graphics_queue_index;
    graphics_queue_info.queueCount       = 1;

    VkDeviceQueueCreateInfo compute_queue_info;
    HELIOS_ZERO_MEMORY(compute_queue_info);

    compute_queue_info.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    compute_queue_info.queueFamilyIndex = infos.compute_queue_index;
    compute_queue_info.queueCount       = 1;

    VkDeviceQueueCreateInfo transfer_queue_info;
    HELIOS_ZERO_MEMORY(transfer_queue_info);

    transfer_queue_info.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    transfer_queue_info.queueFamilyIndex = infos.transfer_queue_index;
    transfer_queue_info.queueCount       = 1;

    infos.infos[infos.queue_count++] = presentation_queue_info;

    if (infos.graphics_queue_index != infos.presentation_queue_index)
        infos.infos[infos.queue_count++] = graphics_queue_info;

    if (infos.compute_queue_index != infos.presentation_queue_index && infos.compute_queue_index != infos.graphics_queue_index)
        infos.infos[infos.queue_count++] = compute_queue_info;

    if (infos.transfer_queue_index != infos.presentation_queue_index && infos.transfer_queue_index != infos.graphics_queue_index && infos.transfer_queue_index != infos.compute_queue_index)
        infos.infos[infos.queue_count++] = transfer_queue_info;

    return true;
}

// -----------------------------------------------------------------------------------------------------------------------------------

bool Backend::is_queue_compatible(VkQueueFlags current_queue_flags, int32_t graphics, int32_t compute, int32_t transfer)
{
    if (graphics == 1)
    {
        // If you need graphics, and queue doesn't have it...
        if (!(current_queue_flags & VK_QUEUE_GRAPHICS_BIT))
            return false;
    }
    else if (graphics == 0)
    {
        // If you don't need graphics, but queue has it...
        if (current_queue_flags & VK_QUEUE_GRAPHICS_BIT)
            return false;
    }

    if (compute == 1)
    {
        // If you need compute, and queue doesn't have it...
        if (!(current_queue_flags & VK_QUEUE_COMPUTE_BIT))
            return false;
    }
    else if (compute == 0)
    {
        // If you don't need compute, but queue has it...
        if (current_queue_flags & VK_QUEUE_COMPUTE_BIT)
            return false;
    }

    if (transfer == 1)
    {
        // If you need transfer, and queue doesn't have it...
        if (!(current_queue_flags & VK_QUEUE_TRANSFER_BIT))
            return false;
    }
    else if (transfer == 0)
    {
        // If you don't need transfer, but queue has it...
        if (current_queue_flags & VK_QUEUE_TRANSFER_BIT)
            return false;
    }

    return true;
}

// -----------------------------------------------------------------------------------------------------------------------------------

bool Backend::create_logical_device(std::vector<const char*> extensions)
{
    // Acceleration Structure Features
    VkPhysicalDeviceAccelerationStructureFeaturesKHR device_acceleration_structure_features;
    HELIOS_ZERO_MEMORY(device_acceleration_structure_features);

    device_acceleration_structure_features.sType                 = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    device_acceleration_structure_features.pNext = nullptr;
    device_acceleration_structure_features.accelerationStructure = VK_TRUE;

    // Ray Tracing Features
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR device_ray_tracing_pipeline_features;
    HELIOS_ZERO_MEMORY(device_ray_tracing_pipeline_features);

    device_ray_tracing_pipeline_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    device_ray_tracing_pipeline_features.pNext              = &device_acceleration_structure_features;
    device_ray_tracing_pipeline_features.rayTracingPipeline = VK_TRUE;

    // Vulkan 1.1/1.2 Features
    VkPhysicalDeviceVulkan11Features features11;
    VkPhysicalDeviceVulkan12Features features12;

    HELIOS_ZERO_MEMORY(features11);
    HELIOS_ZERO_MEMORY(features12);

    features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    features11.pNext = &features12;

    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.pNext = &device_ray_tracing_pipeline_features;

    // Physical Device Features 2
    VkPhysicalDeviceFeatures2 physical_device_features_2;
    HELIOS_ZERO_MEMORY(physical_device_features_2);

    physical_device_features_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    physical_device_features_2.pNext = &features11;

    vkGetPhysicalDeviceFeatures2(m_vk_physical_device, &physical_device_features_2);

    physical_device_features_2.features.robustBufferAccess = VK_FALSE;

    VkDeviceCreateInfo device_info;
    HELIOS_ZERO_MEMORY(device_info);

    device_info.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.pQueueCreateInfos       = &m_selected_queues.infos[0];
    device_info.queueCreateInfoCount    = static_cast<uint32_t>(m_selected_queues.queue_count);
    device_info.enabledExtensionCount   = extensions.size();
    device_info.ppEnabledExtensionNames = extensions.data();
    device_info.pEnabledFeatures        = nullptr;
    device_info.pNext                   = &physical_device_features_2;

    if (m_vk_debug_messenger)
    {
        device_info.enabledLayerCount   = kValidationLayers.size();
        device_info.ppEnabledLayerNames = &kValidationLayers[0];
    }
    else
        device_info.enabledLayerCount = 0;

    float priority = 1.0f;

    for (int i = 0; i < m_selected_queues.queue_count; i++)
        m_selected_queues.infos[i].pQueuePriorities = &priority;

    if (vkCreateDevice(m_vk_physical_device, &device_info, nullptr, &m_vk_device) != VK_SUCCESS)
        return false;

    // Get presentation queue
    vkGetDeviceQueue(m_vk_device, m_selected_queues.presentation_queue_index, 0, &m_vk_presentation_queue);

    // Get graphics queue
    if (m_selected_queues.graphics_queue_index == m_selected_queues.presentation_queue_index)
        m_vk_graphics_queue = m_vk_presentation_queue;
    else
        vkGetDeviceQueue(m_vk_device, m_selected_queues.graphics_queue_index, 0, &m_vk_graphics_queue);

    // Get compute queue
    if (m_selected_queues.compute_queue_index == m_selected_queues.presentation_queue_index)
        m_vk_compute_queue = m_vk_presentation_queue;
    else if (m_selected_queues.compute_queue_index == m_selected_queues.graphics_queue_index)
        m_vk_compute_queue = m_vk_graphics_queue;
    else
        vkGetDeviceQueue(m_vk_device, m_selected_queues.compute_queue_index, 0, &m_vk_compute_queue);

    // Get transfer queue
    if (m_selected_queues.transfer_queue_index == m_selected_queues.presentation_queue_index)
        m_vk_transfer_queue = m_vk_presentation_queue;
    else if (m_selected_queues.transfer_queue_index == m_selected_queues.graphics_queue_index)
        m_vk_transfer_queue = m_vk_graphics_queue;
    else if (m_selected_queues.transfer_queue_index == m_selected_queues.compute_queue_index)
        m_vk_transfer_queue = m_vk_transfer_queue;
    else
        vkGetDeviceQueue(m_vk_device, m_selected_queues.transfer_queue_index, 0, &m_vk_transfer_queue);

    return true;
}

// -----------------------------------------------------------------------------------------------------------------------------------

bool Backend::create_swapchain()
{
    m_current_frame                   = 0;
    VkSurfaceFormatKHR surface_format = choose_swap_surface_format(m_swapchain_details.format);
    VkPresentModeKHR   present_mode   = choose_swap_present_mode(m_swapchain_details.present_modes);
    VkExtent2D         extent         = choose_swap_extent(m_swapchain_details.capabilities);

    uint32_t image_count = m_swapchain_details.capabilities.minImageCount + 1;

    if (m_swapchain_details.capabilities.maxImageCount > 0 && image_count > m_swapchain_details.capabilities.maxImageCount)
        image_count = m_swapchain_details.capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR create_info;
    HELIOS_ZERO_MEMORY(create_info);

    create_info.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface          = m_vk_surface;
    create_info.minImageCount    = image_count;
    create_info.imageFormat      = surface_format.format;
    create_info.imageColorSpace  = surface_format.colorSpace;
    create_info.imageExtent      = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    m_swap_chain_image_format = surface_format.format;
    m_swap_chain_extent       = extent;

    uint32_t queue_family_indices[] = { (uint32_t)m_selected_queues.graphics_queue_index, (uint32_t)m_selected_queues.presentation_queue_index };

    if (m_selected_queues.presentation_queue_index != m_selected_queues.graphics_queue_index)
    {
        create_info.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices   = queue_family_indices;
    }
    else
    {
        create_info.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
        create_info.queueFamilyIndexCount = 0;
        create_info.pQueueFamilyIndices   = nullptr;
    }

    create_info.preTransform   = m_swapchain_details.capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode    = present_mode;
    create_info.clipped        = VK_TRUE;
    create_info.oldSwapchain   = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(m_vk_device, &create_info, nullptr, &m_vk_swap_chain) != VK_SUCCESS)
        return false;

    uint32_t swap_image_count = 0;
    vkGetSwapchainImagesKHR(m_vk_device, m_vk_swap_chain, &swap_image_count, nullptr);
    m_swap_chain_images.resize(swap_image_count);
    m_swap_chain_image_views.resize(swap_image_count);
    m_swap_chain_framebuffers.resize(swap_image_count);

    VkImage images[32];

    if (vkGetSwapchainImagesKHR(m_vk_device, m_vk_swap_chain, &swap_image_count, &images[0]) != VK_SUCCESS)
        return false;

    m_swap_chain_depth_format = find_depth_format();

    m_swap_chain_depth = Image::create(shared_from_this(),
                                       VK_IMAGE_TYPE_2D,
                                       m_swap_chain_extent.width,
                                       m_swap_chain_extent.height,
                                       1,
                                       1,
                                       1,
                                       m_swap_chain_depth_format,
                                       VMA_MEMORY_USAGE_GPU_ONLY,
                                       VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                       VK_SAMPLE_COUNT_1_BIT);

    m_swap_chain_depth_view = ImageView::create(shared_from_this(), m_swap_chain_depth, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_DEPTH_BIT);

    create_render_pass();

    std::vector<ImageView::Ptr> views(2);

    views[1] = m_swap_chain_depth_view;

    for (int i = 0; i < swap_image_count; i++)
    {
        m_swap_chain_images[i]      = Image::create_from_swapchain(shared_from_this(), images[i], VK_IMAGE_TYPE_2D, m_swap_chain_extent.width, m_swap_chain_extent.height, 1, 1, 1, m_swap_chain_image_format, VMA_MEMORY_USAGE_UNKNOWN, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_SAMPLE_COUNT_1_BIT);
        m_swap_chain_image_views[i] = ImageView::create(shared_from_this(), m_swap_chain_images[i], VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT);

        views[0] = m_swap_chain_image_views[i];

        m_swap_chain_framebuffers[i] = Framebuffer::create(shared_from_this(), m_swap_chain_render_pass, views, m_swap_chain_extent.width, m_swap_chain_extent.height, 1);
    }

    m_in_flight_fences.resize(kMaxFramesInFlight);

    for (size_t i = 0; i < kMaxFramesInFlight; i++)
        m_in_flight_fences[i] = Fence::create(shared_from_this());

    return true;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Backend::recreate_swapchain()
{
    vkDeviceWaitIdle(m_vk_device);

    // Destroy existing swap chain resources
    for (int i = 0; i < m_swap_chain_images.size(); i++)
    {
        m_swap_chain_images[i].reset();
        m_swap_chain_framebuffers[i].reset();
        m_swap_chain_image_views[i].reset();
    }

    vkDestroySwapchainKHR(m_vk_device, m_vk_swap_chain, nullptr);

    if (!create_swapchain())
    {
        HELIOS_LOG_FATAL("(Vulkan) Failed to create swap chain!");
        throw std::runtime_error("(Vulkan) Failed to create swap chain!");
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Backend::create_render_pass()
{
    std::vector<VkAttachmentDescription> attachments(2);

    // Color attachment
    attachments[0].format         = m_swap_chain_image_format;
    attachments[0].samples        = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // Depth attachment
    attachments[1].format         = m_swap_chain_depth_format;
    attachments[1].samples        = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
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

    m_swap_chain_render_pass = RenderPass::create(shared_from_this(), attachments, subpass_description, dependencies);
}

// -----------------------------------------------------------------------------------------------------------------------------------

VkSurfaceFormatKHR Backend::choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats)
{
    if (available_formats.size() == 1 && available_formats[0].format == VK_FORMAT_UNDEFINED)
        return { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };

    for (const auto& available_format : available_formats)
    {
        if (available_format.format == VK_FORMAT_B8G8R8A8_SNORM && available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return available_format;
    }

    return available_formats[0];
}

// -----------------------------------------------------------------------------------------------------------------------------------

VkPresentModeKHR Backend::choose_swap_present_mode(const std::vector<VkPresentModeKHR>& available_modes)
{
    VkPresentModeKHR best_mode = VK_PRESENT_MODE_FIFO_KHR;

    for (const auto& available_mode : available_modes)
    {
        if (available_mode == VK_PRESENT_MODE_MAILBOX_KHR)
            best_mode = available_mode;
        else if (available_mode == VK_PRESENT_MODE_IMMEDIATE_KHR)
            best_mode = available_mode;
    }

    return best_mode;
}

// -----------------------------------------------------------------------------------------------------------------------------------

VkExtent2D Backend::choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities)
{
    // Causes macro issue on windows.
#ifdef max
#    undef max
#endif

#ifdef min
#    undef min
#endif

    VkSurfaceCapabilitiesKHR caps;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_vk_physical_device, m_vk_surface, &caps);

    return caps.maxImageExtent;
}

// -----------------------------------------------------------------------------------------------------------------------------------

namespace utilities
{
void set_image_layout(VkCommandBuffer         cmdbuffer,
                      VkImage                 image,
                      VkImageLayout           oldImageLayout,
                      VkImageLayout           newImageLayout,
                      VkImageSubresourceRange subresourceRange,
                      VkPipelineStageFlags    srcStageMask,
                      VkPipelineStageFlags    dstStageMask)
{
    // Create an image barrier object
    VkImageMemoryBarrier image_memory_barrier;
    HELIOS_ZERO_MEMORY(image_memory_barrier);

    image_memory_barrier.sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_memory_barrier.oldLayout        = oldImageLayout;
    image_memory_barrier.newLayout        = newImageLayout;
    image_memory_barrier.image            = image;
    image_memory_barrier.subresourceRange = subresourceRange;

    // Source layouts (old)
    // Source access mask controls actions that have to be finished on the old layout
    // before it will be transitioned to the new layout
    switch (oldImageLayout)
    {
        case VK_IMAGE_LAYOUT_UNDEFINED:
            // Image layout is undefined (or does not matter)
            // Only valid as initial layout
            // No flags required, listed only for completeness
            image_memory_barrier.srcAccessMask = 0;
            break;

        case VK_IMAGE_LAYOUT_PREINITIALIZED:
            // Image is preinitialized
            // Only valid as initial layout for linear images, preserves memory contents
            // Make sure host writes have been finished
            image_memory_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            // Image is a color attachment
            // Make sure any writes to the color buffer have been finished
            image_memory_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            // Image is a depth/stencil attachment
            // Make sure any writes to the depth/stencil buffer have been finished
            image_memory_barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            // Image is a transfer source
            // Make sure any reads from the image have been finished
            image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            // Image is a transfer destination
            // Make sure any writes to the image have been finished
            image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            // Image is read by a shader
            // Make sure any shader reads from the image have been finished
            image_memory_barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            break;
        default:
            // Other source layouts aren't handled (yet)
            break;
    }

    // Target layouts (new)
    // Destination access mask controls the dependency for the new image layout
    switch (newImageLayout)
    {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            // Image will be used as a transfer destination
            // Make sure any writes to the image have been finished
            image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            // Image will be used as a transfer source
            // Make sure any reads from the image have been finished
            image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            // Image will be used as a color attachment
            // Make sure any writes to the color buffer have been finished
            image_memory_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            // Image layout will be used as a depth/stencil attachment
            // Make sure any writes to depth/stencil buffer have been finished
            image_memory_barrier.dstAccessMask = image_memory_barrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            // Image will be read in a shader (sampler, input attachment)
            // Make sure any writes to the image have been finished
            if (image_memory_barrier.srcAccessMask == 0)
            {
                image_memory_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
            }
            image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            break;
        default:
            // Other source layouts aren't handled (yet)
            break;
    }

    // Put barrier inside setup command buffer
    vkCmdPipelineBarrier(
        cmdbuffer,
        srcStageMask,
        dstStageMask,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &image_memory_barrier);
}

uint32_t get_memory_type(VkPhysicalDevice device, uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32* memTypeFound)
{
    VkPhysicalDeviceMemoryProperties prop;

    // Memory properties are used regularly for creating all kinds of buffers
    vkGetPhysicalDeviceMemoryProperties(device, &prop);

    for (uint32_t i = 0; i < prop.memoryTypeCount; i++)
    {
        if ((typeBits & 1) == 1)
        {
            if ((prop.memoryTypes[i].propertyFlags & properties) == properties)
            {
                if (memTypeFound)
                {
                    *memTypeFound = true;
                }
                return i;
            }
        }
        typeBits >>= 1;
    }

    if (memTypeFound)
    {
        *memTypeFound = false;
        return 0;
    }
    else
    {
        throw std::runtime_error("Could not find a matching memory type");
    }
}
} // namespace utilities

// -----------------------------------------------------------------------------------------------------------------------------------
} // namespace vk
} // namespace helios