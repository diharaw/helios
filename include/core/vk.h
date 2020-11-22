#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <memory>
#include <stack>
#include <deque>

struct GLFWwindow;
struct VmaAllocator_T;
struct VmaAllocation_T;
enum VmaMemoryUsage;

namespace lumen
{
namespace vk
{
class Object;
class Image;
class ImageView;
class Framebuffer;
class RenderPass;
class CommandBuffer;
class PipelineLayout;
class CommandPool;
class Fence;
class Semaphore;
class Sampler;
class DescriptorSet;
class DescriptorSetLayout;
class DescriptorPool;
class PipelineLayout;

struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR        capabilities;
    std::vector<VkSurfaceFormatKHR> format;
    std::vector<VkPresentModeKHR>   present_modes;
};

struct QueueInfos
{
    // Most ideal queue = 3
    // Second most ideal queue = 2
    // Queue for minimum functionality = 1
    // Not found = 0

    int32_t                 graphics_queue_index     = -1;
    int32_t                 graphics_queue_quality   = 0;
    int32_t                 compute_queue_index      = -1;
    int32_t                 compute_queue_quality    = 0;
    int32_t                 transfer_queue_index     = -1;
    int32_t                 transfer_queue_quality   = 0;
    int32_t                 presentation_queue_index = -1;
    int32_t                 queue_count              = 0;
    VkDeviceQueueCreateInfo infos[32];

    bool asynchronous_compute();
    bool transfer();
};

class Backend : public std::enable_shared_from_this<Backend>
{
public:
    static const uint32_t kMaxFramesInFlight = 3;

    using Ptr = std::shared_ptr<Backend>;

    static Backend::Ptr create(GLFWwindow* window, bool enable_validation_layers = false, bool require_ray_tracing = false, std::vector<const char*> additional_device_extensions = std::vector<const char*>());

    ~Backend();

    std::shared_ptr<DescriptorSet>  allocate_descriptor_set(std::shared_ptr<DescriptorSetLayout> layout);
    std::shared_ptr<CommandBuffer>  allocate_graphics_command_buffer(bool begin = false);
    std::shared_ptr<CommandBuffer>  allocate_compute_command_buffer(bool begin = false);
    std::shared_ptr<CommandBuffer>  allocate_transfer_command_buffer(bool begin = false);
    std::shared_ptr<CommandPool>    thread_local_graphics_command_pool();
    std::shared_ptr<CommandPool>    thread_local_compute_command_pool();
    std::shared_ptr<CommandPool>    thread_local_transfer_command_pool();
    std::shared_ptr<DescriptorPool> thread_local_descriptor_pool();
    void                            submit_graphics(const std::vector<std::shared_ptr<CommandBuffer>>& cmd_bufs,
                                                    const std::vector<std::shared_ptr<Semaphore>>&     wait_semaphores,
                                                    const std::vector<VkPipelineStageFlags>&           wait_stages,
                                                    const std::vector<std::shared_ptr<Semaphore>>&     signal_semaphores);
    void                            submit_compute(const std::vector<std::shared_ptr<CommandBuffer>>& cmd_bufs,
                                                   const std::vector<std::shared_ptr<Semaphore>>&     wait_semaphores,
                                                   const std::vector<VkPipelineStageFlags>&           wait_stages,
                                                   const std::vector<std::shared_ptr<Semaphore>>&     signal_semaphores);
    void                            submit_transfer(const std::vector<std::shared_ptr<CommandBuffer>>& cmd_bufs,
                                                    const std::vector<std::shared_ptr<Semaphore>>&     wait_semaphores,
                                                    const std::vector<VkPipelineStageFlags>&           wait_stages,
                                                    const std::vector<std::shared_ptr<Semaphore>>&     signal_semaphores);
    void                            flush_graphics(const std::vector<std::shared_ptr<CommandBuffer>>& cmd_bufs);
    void                            flush_compute(const std::vector<std::shared_ptr<CommandBuffer>>& cmd_bufs);
    void                            flush_transfer(const std::vector<std::shared_ptr<CommandBuffer>>& cmd_bufs);
    void                            acquire_next_swap_chain_image(const std::shared_ptr<Semaphore>& semaphore);
    void                            present(const std::vector<std::shared_ptr<Semaphore>>& semaphores);
    bool                            is_frame_done(uint32_t idx);
    void                            wait_for_frame(uint32_t idx);
    std::shared_ptr<Image>          swapchain_image();
    std::shared_ptr<ImageView>      swapchain_image_view();
    std::shared_ptr<Framebuffer>    swapchain_framebuffer();
    std::shared_ptr<RenderPass>     swapchain_render_pass();
    void                            recreate_swapchain();

    void             wait_idle();
    uint32_t         swap_image_count();
    VkInstance       instance();
    VkQueue          graphics_queue();
    VkQueue          transfer_queue();
    VkQueue          compute_queue();
    VkDevice         device();
    VkPhysicalDevice physical_device();
    VmaAllocator_T*  allocator();
    size_t           min_dynamic_ubo_alignment();
    size_t           aligned_dynamic_ubo_size(size_t size);
    VkFormat         find_supported_format(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
    void             process_deletion_queue();
    void             queue_object_deletion(std::shared_ptr<Object> object);

    inline VkPhysicalDeviceRayTracingPropertiesKHR ray_tracing_properties() { return m_ray_tracing_properties; }
    inline VkFormat                                swap_chain_image_format() { return m_swap_chain_image_format; }
    inline VkFormat                                swap_chain_depth_format() { return m_swap_chain_depth_format; }
    inline VkExtent2D                              swap_chain_extents() { return m_swap_chain_extent; }
    inline uint32_t                                current_frame_idx() { return m_current_frame; }
    inline const QueueInfos&                       queue_infos() { return m_selected_queues; }
    inline std::shared_ptr<DescriptorSetLayout>    scene_descriptor_set_layout() { return m_scene_descriptor_set_layout; }
    inline std::shared_ptr<DescriptorSetLayout>    buffer_array_descriptor_set_layout() { return m_buffer_array_descriptor_set_layout; }
    inline std::shared_ptr<DescriptorSetLayout>    combined_sampler_array_descriptor_set_layout() { return m_combined_sampler_array_descriptor_set_layout; }
    inline std::shared_ptr<DescriptorSetLayout>    image_descriptor_set_layout() { return m_image_descriptor_set_layout; }
    inline std::shared_ptr<DescriptorSetLayout>    combined_sampler_descriptor_set_layout() { return m_combined_sampler_descriptor_set_layout; }
    inline std::shared_ptr<Sampler>                bilinear_sampler() { return m_bilinear_sampler; }
    inline std::shared_ptr<Sampler>                trilinear_sampler() { return m_trilinear_sampler; }
    inline std::shared_ptr<Sampler>                nearest_sampler() { return m_nearest_sampler; }

private:
    Backend(GLFWwindow* window, bool enable_validation_layers, bool require_ray_tracing, std::vector<const char*> additional_device_extensions);
    void                     initialize();
    void                     load_extensions();
    VkFormat                 find_depth_format();
    bool                     check_validation_layer_support(std::vector<const char*> layers);
    bool                     check_device_extension_support(VkPhysicalDevice device, std::vector<const char*> extensions);
    void                     query_swap_chain_support(VkPhysicalDevice device, SwapChainSupportDetails& details);
    std::vector<const char*> required_extensions(bool enable_validation_layers);
    VkResult                 create_debug_utils_messenger(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
    void                     destroy_debug_utils_messenger(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);
    bool                     create_surface(GLFWwindow* window);
    bool                     find_physical_device(std::vector<const char*> extensions);
    bool                     is_device_suitable(VkPhysicalDevice device, VkPhysicalDeviceType type, QueueInfos& infos, SwapChainSupportDetails& details, std::vector<const char*> extensions);
    bool                     find_queues(VkPhysicalDevice device, QueueInfos& infos);
    bool                     is_queue_compatible(VkQueueFlags current_queue_flags, int32_t graphics, int32_t compute, int32_t transfer);
    bool                     create_logical_device(std::vector<const char*> extensions);
    bool                     create_swapchain();
    void                     create_render_pass();
    VkSurfaceFormatKHR       choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats);
    VkPresentModeKHR         choose_swap_present_mode(const std::vector<VkPresentModeKHR>& available_modes);
    VkExtent2D               choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities);
    void                     submit(VkQueue                                            queue,
                                    const std::vector<std::shared_ptr<CommandBuffer>>& cmd_bufs,
                                    const std::vector<std::shared_ptr<Semaphore>>&     wait_semaphores,
                                    const std::vector<VkPipelineStageFlags>&           wait_stages,
                                    const std::vector<std::shared_ptr<Semaphore>>&     signal_semaphores);
    void                     flush(VkQueue queue, const std::vector<std::shared_ptr<CommandBuffer>>& cmd_bufs);

private:
    GLFWwindow*                                              m_window                = nullptr;
    VkInstance                                               m_vk_instance           = nullptr;
    VkDevice                                                 m_vk_device             = nullptr;
    VkQueue                                                  m_vk_graphics_queue     = nullptr;
    VkQueue                                                  m_vk_compute_queue      = nullptr;
    VkQueue                                                  m_vk_transfer_queue     = nullptr;
    VkQueue                                                  m_vk_presentation_queue = nullptr;
    VkPhysicalDevice                                         m_vk_physical_device    = nullptr;
    VkSurfaceKHR                                             m_vk_surface            = nullptr;
    VkSwapchainKHR                                           m_vk_swap_chain         = nullptr;
    VkDebugUtilsMessengerEXT                                 m_vk_debug_messenger    = nullptr;
    VmaAllocator_T*                                          m_vma_allocator         = nullptr;
    SwapChainSupportDetails                                  m_swapchain_details;
    QueueInfos                                               m_selected_queues;
    VkFormat                                                 m_swap_chain_image_format;
    VkFormat                                                 m_swap_chain_depth_format;
    VkExtent2D                                               m_swap_chain_extent;
    VkPhysicalDeviceRayTracingPropertiesKHR                  m_ray_tracing_properties;
    std::shared_ptr<RenderPass>                              m_swap_chain_render_pass;
    std::vector<std::shared_ptr<Image>>                      m_swap_chain_images;
    std::vector<std::shared_ptr<ImageView>>                  m_swap_chain_image_views;
    std::vector<std::shared_ptr<Framebuffer>>                m_swap_chain_framebuffers;
    std::shared_ptr<DescriptorSetLayout>                     m_scene_descriptor_set_layout;
    std::shared_ptr<DescriptorSetLayout>                     m_buffer_array_descriptor_set_layout;
    std::shared_ptr<DescriptorSetLayout>                     m_combined_sampler_array_descriptor_set_layout;
    std::shared_ptr<DescriptorSetLayout>                     m_image_descriptor_set_layout;
    std::shared_ptr<DescriptorSetLayout>                     m_combined_sampler_descriptor_set_layout;
    std::shared_ptr<Sampler>                                 m_bilinear_sampler;
    std::shared_ptr<Sampler>                                 m_trilinear_sampler;
    std::shared_ptr<Sampler>                                 m_nearest_sampler;
    uint32_t                                                 m_image_index   = 0;
    uint32_t                                                 m_current_frame = 0;
    std::vector<std::shared_ptr<Fence>>                      m_in_flight_fences;
    std::shared_ptr<Image>                                   m_swap_chain_depth      = nullptr;
    std::shared_ptr<ImageView>                               m_swap_chain_depth_view = nullptr;
    VkPhysicalDeviceProperties                               m_device_properties;
    bool                                                     m_ray_tracing_enabled = false;
    std::deque<std::pair<std::shared_ptr<Object>, uint32_t>> m_deletion_queue;
};

class Object
{
public:
    Object(Backend::Ptr backend);

    inline std::weak_ptr<Backend> backend() { return m_vk_backend; }

protected:
    std::weak_ptr<Backend> m_vk_backend;
};

class Image : public Object
{
public:
    using Ptr = std::shared_ptr<Image>;

    static Image::Ptr create(Backend::Ptr backend, VkImageType type, uint32_t width, uint32_t height, uint32_t depth, uint32_t mip_levels, uint32_t array_size, VkFormat format, VmaMemoryUsage memory_usage, VkImageUsageFlags usage, VkSampleCountFlagBits sample_count, VkImageLayout initial_layout = VK_IMAGE_LAYOUT_UNDEFINED, size_t size = 0, void* data = nullptr);
    static Image::Ptr create_from_swapchain(Backend::Ptr backend, VkImage image, VkImageType type, uint32_t width, uint32_t height, uint32_t depth, uint32_t mip_levels, uint32_t array_size, VkFormat format, VmaMemoryUsage memory_usage, VkImageUsageFlags usage, VkSampleCountFlagBits sample_count);

    ~Image();

    void upload_data(int array_index, int mip_level, void* data, size_t size, VkImageLayout src_layout = VK_IMAGE_LAYOUT_UNDEFINED, VkImageLayout dst_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    void generate_mipmaps(VkImageLayout src_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VkImageLayout dst_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    inline VkImageType        type() { return m_type; }
    inline const VkImage&     handle() { return m_vk_image; }
    inline uint32_t           width() { return m_width; }
    inline uint32_t           height() { return m_height; }
    inline uint32_t           depth() { return m_depth; }
    inline uint32_t           mip_levels() { return m_mip_levels; }
    inline uint32_t           array_size() { return m_array_size; }
    inline VkFormat           format() { return m_format; }
    inline VkImageUsageFlags  usage() { return m_usage; }
    inline VmaMemoryUsage     memory_usage() { return m_memory_usage; }
    inline VkSampleCountFlags sample_count() { return m_sample_count; }

private:
    Image(Backend::Ptr backend, VkImageType type, uint32_t width, uint32_t height, uint32_t depth, uint32_t mip_levels, uint32_t array_size, VkFormat format, VmaMemoryUsage memory_usage, VkImageUsageFlags usage, VkSampleCountFlagBits sample_count, VkImageLayout initial_layout, size_t size, void* data);
    Image(Backend::Ptr backend, VkImage image, VkImageType type, uint32_t width, uint32_t height, uint32_t depth, uint32_t mip_levels, uint32_t array_size, VkFormat format, VmaMemoryUsage memory_usage, VkImageUsageFlags usage, VkSampleCountFlagBits sample_count);

private:
    uint32_t              m_width;
    uint32_t              m_height;
    uint32_t              m_depth;
    uint32_t              m_mip_levels;
    uint32_t              m_array_size;
    VkFormat              m_format;
    VkImageUsageFlags     m_usage;
    VmaMemoryUsage        m_memory_usage;
    VkSampleCountFlagBits m_sample_count;
    VkImageType           m_type;
    VkImage               m_vk_image         = nullptr;
    VkDeviceMemory        m_vk_device_memory = nullptr;
    VmaAllocator_T*       m_vma_allocator    = nullptr;
    VmaAllocation_T*      m_vma_allocation   = nullptr;
};

class ImageView : public Object
{
public:
    using Ptr = std::shared_ptr<ImageView>;

    static ImageView::Ptr create(Backend::Ptr backend, Image::Ptr image, VkImageViewType view_type, VkImageAspectFlags aspect_flags, uint32_t base_mip_level = 0, uint32_t level_count = 1, uint32_t base_array_layer = 0, uint32_t layer_count = 1);

    ~ImageView();

    inline const VkImageView& handle() { return m_vk_image_view; }

private:
    ImageView(Backend::Ptr backend, Image::Ptr image, VkImageViewType view_type, VkImageAspectFlags aspect_flags, uint32_t base_mip_level = 0, uint32_t level_count = 1, uint32_t base_array_layer = 0, uint32_t layer_count = 1);

private:
    VkImageView m_vk_image_view;
};

class RenderPass : public Object
{
public:
    using Ptr = std::shared_ptr<RenderPass>;

    static RenderPass::Ptr create(Backend::Ptr backend, std::vector<VkAttachmentDescription> attachment_descs, std::vector<VkSubpassDescription> subpass_descs, std::vector<VkSubpassDependency> subpass_deps);
    ~RenderPass();

    inline const VkRenderPass& handle() { return m_vk_render_pass; }

private:
    RenderPass(Backend::Ptr backend, std::vector<VkAttachmentDescription> attachment_descs, std::vector<VkSubpassDescription> subpass_descs, std::vector<VkSubpassDependency> subpass_deps);

private:
    VkRenderPass m_vk_render_pass = nullptr;
};

class Framebuffer : public Object
{
public:
    using Ptr = std::shared_ptr<Framebuffer>;

    static Framebuffer::Ptr create(Backend::Ptr backend, RenderPass::Ptr render_pass, std::vector<ImageView::Ptr> views, uint32_t width, uint32_t height, uint32_t layers);

    ~Framebuffer();

    inline const VkFramebuffer& handle() { return m_vk_framebuffer; }

private:
    Framebuffer(Backend::Ptr backend, RenderPass::Ptr render_pass, std::vector<ImageView::Ptr> views, uint32_t width, uint32_t height, uint32_t layers);

private:
    VkFramebuffer m_vk_framebuffer;
};

class Buffer : public Object
{
public:
    using Ptr = std::shared_ptr<Buffer>;

    static Buffer::Ptr create(Backend::Ptr backend, VkBufferUsageFlags usage, size_t size, VmaMemoryUsage memory_usage, VkFlags create_flags, void* data = nullptr);

    ~Buffer();

    void upload_data(void* data, size_t size, size_t offset);

    inline const VkBuffer& handle() { return m_vk_buffer; }
    inline size_t          size() { return m_size; }
    inline void*           mapped_ptr() { return m_mapped_ptr; }
    inline VkDeviceAddress device_address() { return m_device_address; }

private:
    Buffer(Backend::Ptr backend, VkBufferUsageFlags usage, size_t size, VmaMemoryUsage memory_usage, VkFlags create_flags, void* data);

private:
    size_t                m_size;
    void*                 m_mapped_ptr       = nullptr;
    VkBuffer              m_vk_buffer        = nullptr;
    VkDeviceMemory        m_vk_device_memory = nullptr;
    VkDeviceAddress       m_device_address   = 0;
    VmaAllocator_T*       m_vma_allocator    = nullptr;
    VmaAllocation_T*      m_vma_allocation   = nullptr;
    VmaMemoryUsage        m_vma_memory_usage;
    VkMemoryPropertyFlags m_vk_memory_property;
    VkBufferUsageFlags    m_vk_usage_flags;
};

class CommandPool : public Object
{
public:
    using Ptr = std::shared_ptr<CommandPool>;

    static CommandPool::Ptr create(Backend::Ptr backend, uint32_t queue_family_index);

    ~CommandPool();

    void reset();

    inline const VkCommandPool& handle() { return m_vk_pool; }

private:
    CommandPool(Backend::Ptr backend, uint32_t queue_family_index);

private:
    VkCommandPool m_vk_pool = nullptr;
};

class CommandBuffer : public Object
{
public:
    using Ptr = std::shared_ptr<CommandBuffer>;

    static CommandBuffer::Ptr create(Backend::Ptr backend, CommandPool::Ptr pool);

    ~CommandBuffer();

    void                          reset();
    inline const VkCommandBuffer& handle() { return m_vk_command_buffer; }

private:
    CommandBuffer(Backend::Ptr backend, CommandPool::Ptr pool);

private:
    VkCommandBuffer            m_vk_command_buffer;
    std::weak_ptr<CommandPool> m_vk_pool;
};

class ShaderModule : public Object
{
public:
    using Ptr = std::shared_ptr<ShaderModule>;

    static ShaderModule::Ptr create_from_file(Backend::Ptr backend, std::string path);
    static ShaderModule::Ptr create(Backend::Ptr backend, std::vector<char> spirv);

    ~ShaderModule();

    inline const VkShaderModule& handle() { return m_vk_module; }

private:
    ShaderModule(Backend::Ptr backend, std::vector<char> spirv);

private:
    VkShaderModule m_vk_module;
};

struct VertexInputStateDesc
{
    VkPipelineVertexInputStateCreateInfo create_info;
    VkVertexInputBindingDescription      binding_desc[16];
    VkVertexInputAttributeDescription    attribute_desc[16];

    VertexInputStateDesc();
    VertexInputStateDesc& add_binding_desc(uint32_t binding, uint32_t stride, VkVertexInputRate input_rate);
    VertexInputStateDesc& add_attribute_desc(uint32_t location, uint32_t binding, VkFormat format, uint32_t offset);
};

struct InputAssemblyStateDesc
{
    VkPipelineInputAssemblyStateCreateInfo create_info;

    InputAssemblyStateDesc();
    InputAssemblyStateDesc& set_flags(VkPipelineInputAssemblyStateCreateFlags flags);
    InputAssemblyStateDesc& set_topology(VkPrimitiveTopology topology);
    InputAssemblyStateDesc& set_primitive_restart_enable(bool primitive_restart_enable);
};

struct TessellationStateDesc
{
    VkPipelineTessellationStateCreateInfo create_info;

    TessellationStateDesc();
    TessellationStateDesc& set_flags(VkPipelineTessellationStateCreateFlags flags);
    TessellationStateDesc& set_patch_control_points(uint32_t patch_control_points);
};

struct RasterizationStateDesc
{
    VkPipelineRasterizationStateCreateInfo                create_info;
    VkPipelineRasterizationConservativeStateCreateInfoEXT conservative_raster_create_info;

    RasterizationStateDesc();
    RasterizationStateDesc& set_depth_clamp(VkBool32 value);
    RasterizationStateDesc& set_rasterizer_discard_enable(VkBool32 value);
    RasterizationStateDesc& set_polygon_mode(VkPolygonMode value);
    RasterizationStateDesc& set_cull_mode(VkCullModeFlags value);
    RasterizationStateDesc& set_front_face(VkFrontFace value);
    RasterizationStateDesc& set_depth_bias(VkBool32 value);
    RasterizationStateDesc& set_depth_bias_constant_factor(float value);
    RasterizationStateDesc& set_depth_bias_clamp(float value);
    RasterizationStateDesc& set_depth_bias_slope_factor(float value);
    RasterizationStateDesc& set_line_width(float value);
    RasterizationStateDesc& set_conservative_raster_mode(VkConservativeRasterizationModeEXT value);
    RasterizationStateDesc& set_extra_primitive_overestimation_size(float value);
};

struct MultisampleStateDesc
{
    VkPipelineMultisampleStateCreateInfo create_info;

    MultisampleStateDesc();
    MultisampleStateDesc& set_rasterization_samples(VkSampleCountFlagBits value);
    MultisampleStateDesc& set_sample_shading_enable(VkBool32 value);
    MultisampleStateDesc& set_min_sample_shading(float value);
    MultisampleStateDesc& set_sample_mask(VkSampleMask* value);
    MultisampleStateDesc& set_alpha_to_coverage_enable(VkBool32 value);
    MultisampleStateDesc& set_alpha_to_one_enable(VkBool32 value);
};

struct StencilOpStateDesc
{
    VkStencilOpState create_info;

    StencilOpStateDesc& set_fail_op(VkStencilOp value);
    StencilOpStateDesc& set_pass_op(VkStencilOp value);
    StencilOpStateDesc& set_depth_fail_op(VkStencilOp value);
    StencilOpStateDesc& set_compare_op(VkCompareOp value);
    StencilOpStateDesc& set_compare_mask(uint32_t value);
    StencilOpStateDesc& set_write_mask(uint32_t value);
    StencilOpStateDesc& set_reference(uint32_t value);
};

struct DepthStencilStateDesc
{
    VkPipelineDepthStencilStateCreateInfo create_info;

    DepthStencilStateDesc();
    DepthStencilStateDesc& set_depth_test_enable(VkBool32 value);
    DepthStencilStateDesc& set_depth_write_enable(VkBool32 value);
    DepthStencilStateDesc& set_depth_compare_op(VkCompareOp value);
    DepthStencilStateDesc& set_depth_bounds_test_enable(VkBool32 value);
    DepthStencilStateDesc& set_stencil_test_enable(VkBool32 value);
    DepthStencilStateDesc& set_front(StencilOpStateDesc value);
    DepthStencilStateDesc& set_back(StencilOpStateDesc value);
    DepthStencilStateDesc& set_min_depth_bounds(float value);
    DepthStencilStateDesc& set_max_depth_bounds(float value);
};

struct ColorBlendAttachmentStateDesc
{
    VkPipelineColorBlendAttachmentState create_info;

    ColorBlendAttachmentStateDesc();
    ColorBlendAttachmentStateDesc& set_blend_enable(VkBool32 value);
    ColorBlendAttachmentStateDesc& set_src_color_blend_factor(VkBlendFactor value);
    ColorBlendAttachmentStateDesc& set_dst_color_blend_Factor(VkBlendFactor value);
    ColorBlendAttachmentStateDesc& set_color_blend_op(VkBlendOp value);
    ColorBlendAttachmentStateDesc& set_src_alpha_blend_factor(VkBlendFactor value);
    ColorBlendAttachmentStateDesc& set_dst_alpha_blend_factor(VkBlendFactor value);
    ColorBlendAttachmentStateDesc& set_alpha_blend_op(VkBlendOp value);
    ColorBlendAttachmentStateDesc& set_color_write_mask(VkColorComponentFlags value);
};

struct ColorBlendStateDesc
{
    VkPipelineColorBlendStateCreateInfo create_info;
    VkPipelineColorBlendAttachmentState attachments[32];

    ColorBlendStateDesc();
    ColorBlendStateDesc& set_logic_op_enable(VkBool32 value);
    ColorBlendStateDesc& set_logic_op(VkLogicOp value);
    ColorBlendStateDesc& add_attachment(ColorBlendAttachmentStateDesc att);
    ColorBlendStateDesc& set_blend_constants(float r, float g, float b, float a);
};

struct ViewportStateDesc
{
    VkPipelineViewportStateCreateInfo create_info;
    uint32_t                          viewport_count = 0;
    uint32_t                          scissor_count  = 0;
    VkViewport                        viewports[32];
    VkRect2D                          scissors[32];

    ViewportStateDesc();
    ViewportStateDesc& add_viewport(float x,
                                    float y,
                                    float width,
                                    float height,
                                    float min_depth,
                                    float max_depth);
    ViewportStateDesc& add_scissor(int32_t  x,
                                   int32_t  y,
                                   uint32_t w,
                                   uint32_t h);
};

class GraphicsPipeline : public Object
{
public:
    using Ptr = std::shared_ptr<GraphicsPipeline>;

    struct Desc
    {
        VkGraphicsPipelineCreateInfo     create_info;
        uint32_t                         shader_stage_count = 0;
        VkPipelineShaderStageCreateInfo  shader_stages[6];
        VkPipelineDynamicStateCreateInfo dynamic_state;
        std::string                      shader_entry_names[6];
        uint32_t                         dynamic_state_count = 0;
        VkDynamicState                   dynamic_states[32];

        Desc();
        Desc& add_dynamic_state(const VkDynamicState& state);
        Desc& set_viewport_state(ViewportStateDesc& state);
        Desc& add_shader_stage(const VkShaderStageFlagBits& stage, const ShaderModule::Ptr& shader_module, const std::string& name);
        Desc& set_vertex_input_state(const VertexInputStateDesc& state);
        Desc& set_input_assembly_state(const InputAssemblyStateDesc& state);
        Desc& set_tessellation_state(const TessellationStateDesc& state);
        Desc& set_rasterization_state(const RasterizationStateDesc& state);
        Desc& set_multisample_state(const MultisampleStateDesc& state);
        Desc& set_depth_stencil_state(const DepthStencilStateDesc& state);
        Desc& set_color_blend_state(const ColorBlendStateDesc& state);
        Desc& set_pipeline_layout(const std::shared_ptr<PipelineLayout>& layout);
        Desc& set_render_pass(const RenderPass::Ptr& render_pass);
        Desc& set_sub_pass(const uint32_t& subpass);
        Desc& set_base_pipeline(const GraphicsPipeline::Ptr& pipeline);
        Desc& set_base_pipeline_index(const int32_t& index);
    };

    static GraphicsPipeline::Ptr create_for_post_process(Backend::Ptr backend, std::string vs, std::string fs, std::shared_ptr<PipelineLayout> pipeline_layout, RenderPass::Ptr render_pass);
    static GraphicsPipeline::Ptr create(Backend::Ptr backend, Desc desc);

    inline const VkPipeline& handle() { return m_vk_pipeline; }

    ~GraphicsPipeline();

private:
    GraphicsPipeline(Backend::Ptr backend, Desc desc);

private:
    VkPipeline m_vk_pipeline;
};

class ComputePipeline : public Object
{
public:
    using Ptr = std::shared_ptr<ComputePipeline>;

    struct Desc
    {
        VkComputePipelineCreateInfo create_info;
        std::string                 shader_entry_name;

        Desc();
        Desc& set_shader_stage(ShaderModule::Ptr shader_module, std::string name);
        Desc& set_pipeline_layout(std::shared_ptr<PipelineLayout> layout);
        Desc& set_base_pipeline(ComputePipeline::Ptr pipeline);
        Desc& set_base_pipeline_index(int32_t index);
    };

    static ComputePipeline::Ptr create(Backend::Ptr backend, Desc desc);

    inline const VkPipeline& handle() { return m_vk_pipeline; }

    ~ComputePipeline();

private:
    ComputePipeline(Backend::Ptr backend, Desc desc);

private:
    VkPipeline m_vk_pipeline;
};

class ShaderBindingTable : public Object
{
public:
    using Ptr = std::shared_ptr<ShaderBindingTable>;

    struct Desc
    {
    private:
        struct HitGroupDesc
        {
            VkPipelineShaderStageCreateInfo* closest_hit_stage  = nullptr;
            VkPipelineShaderStageCreateInfo* any_hit_stage      = nullptr;
            VkPipelineShaderStageCreateInfo* intersection_stage = nullptr;
        };

    public:
        std::vector<VkPipelineShaderStageCreateInfo> ray_gen_stages;
        std::vector<VkPipelineShaderStageCreateInfo> hit_stages;
        std::vector<VkPipelineShaderStageCreateInfo> miss_stages;
        std::vector<HitGroupDesc>                    hit_groups;
        std::vector<std::string>                     entry_point_names;

        Desc();
        Desc& add_ray_gen_group(ShaderModule::Ptr shader, const std::string& entry_point);
        Desc& add_hit_group(ShaderModule::Ptr  closest_hit_shader,
                            const std::string& closest_hit_entry_point,
                            ShaderModule::Ptr  any_hit_shader           = nullptr,
                            const std::string& any_hit_entry_point      = "",
                            ShaderModule::Ptr  intersection_shader      = nullptr,
                            const std::string& intersection_entry_point = "");
        Desc& add_miss_group(ShaderModule::Ptr shader, const std::string& entry_point);
    };

    static ShaderBindingTable::Ptr create(Backend::Ptr backend, Desc desc);

    inline const std::vector<VkPipelineShaderStageCreateInfo>&      stages() { return m_stages; }
    inline const std::vector<VkRayTracingShaderGroupCreateInfoKHR>& groups() { return m_groups; }
    VkDeviceSize                                                    hit_group_offset();
    VkDeviceSize                                                    miss_group_offset();

    ~ShaderBindingTable();

private:
    ShaderBindingTable(Backend::Ptr backend, Desc desc);

private:
    VkDeviceSize                                      m_ray_gen_size;
    VkDeviceSize                                      m_hit_group_size;
    VkDeviceSize                                      m_miss_group_size;
    std::vector<std::string>                          m_entry_point_names;
    std::vector<VkPipelineShaderStageCreateInfo>      m_stages;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> m_groups;
};

class RayTracingPipeline : public Object
{
public:
    using Ptr = std::shared_ptr<RayTracingPipeline>;

    struct Desc
    {
        VkRayTracingPipelineCreateInfoKHR create_info;
        ShaderBindingTable::Ptr           sbt;

        Desc();
        Desc& set_shader_binding_table(ShaderBindingTable::Ptr table);
        Desc& set_pipeline_layout(std::shared_ptr<PipelineLayout> layout);
        Desc& set_recursion_depth(uint32_t depth);
        Desc& set_base_pipeline(RayTracingPipeline::Ptr pipeline);
        Desc& set_base_pipeline_index(int32_t index);
    };

    static RayTracingPipeline::Ptr create(Backend::Ptr backend, Desc desc);

    inline ShaderBindingTable::Ptr shader_binding_table() { return m_sbt; }
    inline Buffer::Ptr             shader_binding_table_buffer() { return m_vk_buffer; }
    inline const VkPipeline&       handle() { return m_vk_pipeline; }

    ~RayTracingPipeline();

private:
    RayTracingPipeline(Backend::Ptr backend, Desc desc);

private:
    VkPipeline              m_vk_pipeline;
    vk::Buffer::Ptr         m_vk_buffer;
    ShaderBindingTable::Ptr m_sbt;
};

class AccelerationStructure : public Object
{
public:
    using Ptr = std::shared_ptr<AccelerationStructure>;

    struct Desc
    {
        VkAccelerationStructureCreateInfoKHR                          create_info;
        std::vector<VkAccelerationStructureCreateGeometryTypeInfoKHR> geometries;

        Desc();
        Desc& set_type(VkAccelerationStructureTypeKHR type);
        Desc& set_geometry_type_infos(const std::vector<VkAccelerationStructureCreateGeometryTypeInfoKHR>& geometry_vec);
        Desc& set_max_geometry_count(uint32_t count);
        Desc& set_flags(VkBuildAccelerationStructureFlagsKHR flags);
        Desc& set_compacted_size(uint32_t size);
        Desc& set_device_address(VkDeviceAddress address);
    };

    static AccelerationStructure::Ptr create(Backend::Ptr backend, Desc desc);

    inline VkAccelerationStructureCreateInfoKHR& info() { return m_vk_acceleration_structure_info; };
    inline const VkAccelerationStructureKHR&     handle() { return m_vk_acceleration_structure; }
    inline VkDeviceAddress                       device_address() { return m_device_address; }

    ~AccelerationStructure();

private:
    AccelerationStructure(Backend::Ptr backend, Desc desc);

private:
    VmaAllocation_T*                     m_vma_allocation = nullptr;
    VkDeviceAddress                      m_device_address = 0;
    VkAccelerationStructureCreateInfoKHR m_vk_acceleration_structure_info;
    VkAccelerationStructureKHR           m_vk_acceleration_structure = nullptr;
};

class Sampler : public Object
{
public:
    using Ptr = std::shared_ptr<Sampler>;

    struct Desc
    {
        VkSamplerCreateFlags flags = 0;
        VkFilter             mag_filter;
        VkFilter             min_filter;
        VkSamplerMipmapMode  mipmap_mode;
        VkSamplerAddressMode address_mode_u;
        VkSamplerAddressMode address_mode_v;
        VkSamplerAddressMode address_mode_w;
        float                mip_lod_bias;
        VkBool32             anisotropy_enable;
        float                max_anisotropy;
        VkBool32             compare_enable;
        VkCompareOp          compare_op;
        float                min_lod;
        float                max_lod;
        VkBorderColor        border_color;
        VkBool32             unnormalized_coordinates = VK_FALSE;
    };

    inline const VkSampler& handle() { return m_vk_sampler; }

    static Sampler::Ptr create(Backend::Ptr backend, Desc desc);

    ~Sampler();

private:
    Sampler(Backend::Ptr backend, Desc desc);

private:
    VkSampler m_vk_sampler;
};

class DescriptorSetLayout : public Object
{
public:
    using Ptr = std::shared_ptr<DescriptorSetLayout>;

    struct Desc
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        VkSampler                                 binding_samplers[32][8];
        void*                                     pnext_ptr = nullptr;

        Desc& set_next_ptr(void* pnext);
        Desc& add_binding(uint32_t binding, VkDescriptorType descriptor_type, uint32_t descriptor_count, VkShaderStageFlags stage_flags);
        Desc& add_binding(uint32_t binding, VkDescriptorType descriptor_type, uint32_t descriptor_count, VkShaderStageFlags stage_flags, Sampler::Ptr samplers[]);
    };

    static DescriptorSetLayout::Ptr create(Backend::Ptr backend, Desc desc);

    ~DescriptorSetLayout();

    inline const VkDescriptorSetLayout& handle() { return m_vk_ds_layout; }

private:
    DescriptorSetLayout(Backend::Ptr backend, Desc desc);

private:
    VkDescriptorSetLayout m_vk_ds_layout;
};

class PipelineLayout : public Object
{
public:
    using Ptr = std::shared_ptr<PipelineLayout>;

    struct Desc
    {
        std::vector<DescriptorSetLayout::Ptr> layouts;
        std::vector<VkPushConstantRange>      push_constant_ranges;

        Desc& add_descriptor_set_layout(DescriptorSetLayout::Ptr layout);
        Desc& add_push_constant_range(VkShaderStageFlags stage_flags, uint32_t offset, uint32_t size);
    };

    static PipelineLayout::Ptr create(Backend::Ptr backend, Desc desc);

    ~PipelineLayout();

    inline const VkPipelineLayout& handle() { return m_vk_pipeline_layout; }

private:
    PipelineLayout(Backend::Ptr backend, Desc desc);

private:
    VkPipelineLayout m_vk_pipeline_layout;
};

class DescriptorPool : public Object
{
public:
    using Ptr = std::shared_ptr<DescriptorPool>;

    struct Desc
    {
        uint32_t                          max_sets;
        std::vector<VkDescriptorPoolSize> pool_sizes;
        VkDescriptorPoolCreateFlags       create_flags = 0;

        Desc& set_max_sets(uint32_t num);
        Desc& set_create_flags(VkDescriptorPoolCreateFlags flags);
        Desc& add_pool_size(VkDescriptorType type, uint32_t descriptor_count);
    };

    static DescriptorPool::Ptr create(Backend::Ptr backend, Desc desc);

    ~DescriptorPool();

    inline VkDescriptorPoolCreateFlags create_flags() { return m_vk_create_flags; }
    inline const VkDescriptorPool&     handle() { return m_vk_ds_pool; }

private:
    DescriptorPool(Backend::Ptr backend, Desc desc);

private:
    VkDescriptorPoolCreateFlags m_vk_create_flags;
    VkDescriptorPool            m_vk_ds_pool;
};

class DescriptorSet : public Object
{
public:
    using Ptr = std::shared_ptr<DescriptorSet>;

    static DescriptorSet::Ptr create(Backend::Ptr backend, DescriptorSetLayout::Ptr layout, DescriptorPool::Ptr pool);

    ~DescriptorSet();

    inline const VkDescriptorSet& handle() { return m_vk_ds; }

private:
    DescriptorSet(Backend::Ptr backend, DescriptorSetLayout::Ptr layout, DescriptorPool::Ptr pool);

private:
    bool                          m_should_destroy = false;
    VkDescriptorSet               m_vk_ds;
    std::weak_ptr<DescriptorPool> m_vk_pool;
};

class Fence : public Object
{
public:
    using Ptr = std::shared_ptr<Fence>;

    static Fence::Ptr create(Backend::Ptr backend);

    ~Fence();

    inline const VkFence& handle() { return m_vk_fence; }

private:
    Fence(Backend::Ptr backend);

private:
    VkFence m_vk_fence;
};

class Semaphore : public Object
{
public:
    using Ptr = std::shared_ptr<Semaphore>;

    static Semaphore::Ptr create(Backend::Ptr backend);

    ~Semaphore();

    inline const VkSemaphore& handle() { return m_vk_semaphore; }

private:
    Semaphore(Backend::Ptr backend);

private:
    VkSemaphore m_vk_semaphore;
};

class QueryPool : public Object
{
public:
    using Ptr = std::shared_ptr<QueryPool>;

    static QueryPool::Ptr create(Backend::Ptr backend, VkQueryType query_type, uint32_t query_count, VkQueryPipelineStatisticFlags pipeline_statistics = 0);

    void results(uint32_t           first_query,
                 uint32_t           query_count,
                 size_t             data_size,
                 void*              ptr,
                 VkDeviceSize       stride,
                 VkQueryResultFlags flags);
    ~QueryPool();

    inline const VkQueryPool& handle() { return m_vk_query_pool; }

private:
    QueryPool(Backend::Ptr backend, VkQueryType query_type, uint32_t query_count, VkQueryPipelineStatisticFlags pipeline_statistics);

private:
    VkQueryPool m_vk_query_pool;
};

class StagingBuffer
{
public:
    using Ptr = std::shared_ptr<StagingBuffer>;

    static StagingBuffer::Ptr create(Backend::Ptr backend, const size_t& size);

    // Insert the given data into the mapped staging buffer and returns the offset to said data from the start of the buffer.
    size_t insert_data(void* data, const size_t& size);
    ~StagingBuffer();

    inline size_t      remaining_size() { return m_total_size - m_current_size; }
    inline size_t      total_size() { return m_total_size; }
    inline Buffer::Ptr buffer() { return m_buffer; }

private:
    StagingBuffer(Backend::Ptr backend, const size_t& size);

private:
    uint8_t*    m_mapped_ptr;
    size_t      m_total_size   = 0;
    size_t      m_current_size = 0;
    Buffer::Ptr m_buffer;
};

class BatchUploader
{
private:
    struct BLASBuildRequest
    {
        AccelerationStructure::Ptr                             acceleration_structure;
        std::vector<VkAccelerationStructureGeometryKHR>        geometries;
        std::vector<VkAccelerationStructureBuildOffsetInfoKHR> build_offsets;
    };

public:
    BatchUploader(Backend::Ptr backend);
    ~BatchUploader();

    void upload_buffer_data(Buffer::Ptr buffer, void* data, const size_t& offset, const size_t& size);
    void upload_image_data(Image::Ptr image, void* data, const std::vector<size_t>& mip_level_sizes, VkImageLayout src_layout = VK_IMAGE_LAYOUT_UNDEFINED, VkImageLayout dst_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    void build_blas(AccelerationStructure::Ptr acceleration_structure, const std::vector<VkAccelerationStructureGeometryKHR>& geometries, const std::vector<VkAccelerationStructureBuildOffsetInfoKHR> build_offsets);
    void submit();

private:
    Buffer::Ptr insert_data(void* data, const size_t& size);
    void        add_staging_buffer(const size_t& size);

private:
    CommandBuffer::Ptr             m_cmd;
    std::weak_ptr<Backend>         m_backend;
    std::stack<StagingBuffer::Ptr> m_staging_buffers;
    std::vector<BLASBuildRequest>  m_blas_build_requests;
};

namespace utilities
{
extern void set_image_layout(VkCommandBuffer         cmdbuffer,
                             VkImage                 image,
                             VkImageLayout           oldImageLayout,
                             VkImageLayout           newImageLayout,
                             VkImageSubresourceRange subresourceRange,
                             VkPipelineStageFlags    srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                             VkPipelineStageFlags    dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

extern uint32_t get_memory_type(VkPhysicalDevice device, uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32* memTypeFound = nullptr);
} // namespace utilities

} // namespace vk
} // namespace lumen