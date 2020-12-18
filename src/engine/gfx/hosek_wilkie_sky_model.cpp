#include <gfx/hosek_wilkie_sky_model.h>
#include <glm.hpp>
#include <algorithm>
#include <utility/macros.h>
#include <utility/logger.h>
#include <utility/profiler.h>
#include <gtc/matrix_transform.hpp>
#include <vk_mem_alloc.h>

#define _USE_MATH_DEFINES
#include <math.h>

#include <gfx/hosek_data_rgb.inl>

namespace helios
{
#define SKY_CUBEMAP_SIZE 512

struct HosekWilkieUBO
{
    glm::vec4 A;
    glm::vec4 B;
    glm::vec4 C;
    glm::vec4 D;
    glm::vec4 E;
    glm::vec4 F;
    glm::vec4 G;
    glm::vec4 H;
    glm::vec4 I;
    glm::vec4 Z;
};

struct HosekWilkiePushConstants
{
    glm::mat4 view_projection;
    glm::vec3 direction;
};

// -----------------------------------------------------------------------------------------------------------------------------------

double evaluate_spline(const double* spline, size_t stride, double value)
{
    return 1 * pow(1 - value, 5) * spline[0 * stride] + 5 * pow(1 - value, 4) * pow(value, 1) * spline[1 * stride] + 10 * pow(1 - value, 3) * pow(value, 2) * spline[2 * stride] + 10 * pow(1 - value, 2) * pow(value, 3) * spline[3 * stride] + 5 * pow(1 - value, 1) * pow(value, 4) * spline[4 * stride] + 1 * pow(value, 5) * spline[5 * stride];
}

// -----------------------------------------------------------------------------------------------------------------------------------

double evaluate(const double* dataset, size_t stride, float turbidity, float albedo, float sunTheta)
{
    // splines are functions of elevation^1/3
    double elevationK = pow(std::max<float>(0.f, 1.f - sunTheta / (M_PI / 2.f)), 1.f / 3.0f);

    // table has values for turbidity 1..10
    int   turbidity0 = glm::clamp(static_cast<int>(turbidity), 1, 10);
    int   turbidity1 = std::min(turbidity0 + 1, 10);
    float turbidityK = glm::clamp(turbidity - turbidity0, 0.f, 1.f);

    const double* datasetA0 = dataset;
    const double* datasetA1 = dataset + stride * 6 * 10;

    double a0t0 = evaluate_spline(datasetA0 + stride * 6 * (turbidity0 - 1), stride, elevationK);
    double a1t0 = evaluate_spline(datasetA1 + stride * 6 * (turbidity0 - 1), stride, elevationK);
    double a0t1 = evaluate_spline(datasetA0 + stride * 6 * (turbidity1 - 1), stride, elevationK);
    double a1t1 = evaluate_spline(datasetA1 + stride * 6 * (turbidity1 - 1), stride, elevationK);

    return a0t0 * (1 - albedo) * (1 - turbidityK) + a1t0 * albedo * (1 - turbidityK) + a0t1 * (1 - albedo) * turbidityK + a1t1 * albedo * turbidityK;
}

// -----------------------------------------------------------------------------------------------------------------------------------

glm::vec3 hosek_wilkie(float cos_theta, float gamma, float cos_gamma, glm::vec3 A, glm::vec3 B, glm::vec3 C, glm::vec3 D, glm::vec3 E, glm::vec3 F, glm::vec3 G, glm::vec3 H, glm::vec3 I)
{
    glm::vec3 chi = (1.f + cos_gamma * cos_gamma) / pow(1.f + H * H - 2.f * cos_gamma * H, glm::vec3(1.5f));
    return (1.f + A * exp(B / (cos_theta + 0.01f))) * (C + D * exp(E * gamma) + F * (cos_gamma * cos_gamma) + G * chi + I * (float)sqrt(std::max(0.f, cos_theta)));
}

// -----------------------------------------------------------------------------------------------------------------------------------

HosekWilkieSkyModel::HosekWilkieSkyModel(vk::Backend::Ptr backend)
{
    glm::mat4 capture_projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    glm::mat4 capture_views[]    = {
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))
    };

    m_view_projection_mats.resize(6);

    for (int i = 0; i < 6; i++)
        m_view_projection_mats[i] = capture_projection * capture_views[i];

    m_cubemap_image = vk::Image::create(backend, VK_IMAGE_TYPE_2D, SKY_CUBEMAP_SIZE, SKY_CUBEMAP_SIZE, 1, 1, 6, VK_FORMAT_R32G32B32A32_SFLOAT, VMA_MEMORY_USAGE_GPU_ONLY, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_UNDEFINED, 0, nullptr, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);
    m_cubemap_image->set_name("Procedural Sky");

    m_cubemap_image_view = vk::ImageView::create(backend, m_cubemap_image, VK_IMAGE_VIEW_TYPE_CUBE, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6);
    m_cubemap_image_view->set_name("Procedural Sky Image View");

    m_face_image_views.resize(6);
    m_face_framebuffers.resize(6);

    VkAttachmentDescription attachment;
    HELIOS_ZERO_MEMORY(attachment);

    // Color attachment
    attachment.format         = VK_FORMAT_R32G32B32A32_SFLOAT;
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

    m_cubemap_renderpass = vk::RenderPass::create(backend, { attachment }, subpass_description, dependencies);

    for (int i = 0; i < 6; i++)
    {
        m_face_image_views[i] = vk::ImageView::create(backend, m_cubemap_image, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, i, 1);
        m_face_image_views[i]->set_name("Procedural Sky Face " + std::to_string(i) + " Image View");

        m_face_framebuffers[i] = vk::Framebuffer::create(backend, m_cubemap_renderpass, { m_face_image_views[i] }, SKY_CUBEMAP_SIZE, SKY_CUBEMAP_SIZE, 1);
        m_face_framebuffers[i]->set_name("Procedural Sky Face " + std::to_string(i) + " Framebuffer");
    }

    float cube_vertices[] = {
        // back face
        -1.0f,
        -1.0f,
        -1.0f,
        0.0f,
        0.0f,
        -1.0f,
        0.0f,
        0.0f, // bottom-left
        1.0f,
        1.0f,
        -1.0f,
        0.0f,
        0.0f,
        -1.0f,
        1.0f,
        1.0f, // top-right
        1.0f,
        -1.0f,
        -1.0f,
        0.0f,
        0.0f,
        -1.0f,
        1.0f,
        0.0f, // bottom-right
        1.0f,
        1.0f,
        -1.0f,
        0.0f,
        0.0f,
        -1.0f,
        1.0f,
        1.0f, // top-right
        -1.0f,
        -1.0f,
        -1.0f,
        0.0f,
        0.0f,
        -1.0f,
        0.0f,
        0.0f, // bottom-left
        -1.0f,
        1.0f,
        -1.0f,
        0.0f,
        0.0f,
        -1.0f,
        0.0f,
        1.0f, // top-left
        // front face
        -1.0f,
        -1.0f,
        1.0f,
        0.0f,
        0.0f,
        1.0f,
        0.0f,
        0.0f, // bottom-left
        1.0f,
        -1.0f,
        1.0f,
        0.0f,
        0.0f,
        1.0f,
        1.0f,
        0.0f, // bottom-right
        1.0f,
        1.0f,
        1.0f,
        0.0f,
        0.0f,
        1.0f,
        1.0f,
        1.0f, // top-right
        1.0f,
        1.0f,
        1.0f,
        0.0f,
        0.0f,
        1.0f,
        1.0f,
        1.0f, // top-right
        -1.0f,
        1.0f,
        1.0f,
        0.0f,
        0.0f,
        1.0f,
        0.0f,
        1.0f, // top-left
        -1.0f,
        -1.0f,
        1.0f,
        0.0f,
        0.0f,
        1.0f,
        0.0f,
        0.0f, // bottom-left
        // left face
        -1.0f,
        1.0f,
        1.0f,
        -1.0f,
        0.0f,
        0.0f,
        1.0f,
        0.0f, // top-right
        -1.0f,
        1.0f,
        -1.0f,
        -1.0f,
        0.0f,
        0.0f,
        1.0f,
        1.0f, // top-left
        -1.0f,
        -1.0f,
        -1.0f,
        -1.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f, // bottom-left
        -1.0f,
        -1.0f,
        -1.0f,
        -1.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f, // bottom-left
        -1.0f,
        -1.0f,
        1.0f,
        -1.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0f, // bottom-right
        -1.0f,
        1.0f,
        1.0f,
        -1.0f,
        0.0f,
        0.0f,
        1.0f,
        0.0f, // top-right
        // right face
        1.0f,
        1.0f,
        1.0f,
        1.0f,
        0.0f,
        0.0f,
        1.0f,
        0.0f, // top-left
        1.0f,
        -1.0f,
        -1.0f,
        1.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f, // bottom-right
        1.0f,
        1.0f,
        -1.0f,
        1.0f,
        0.0f,
        0.0f,
        1.0f,
        1.0f, // top-right
        1.0f,
        -1.0f,
        -1.0f,
        1.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f, // bottom-right
        1.0f,
        1.0f,
        1.0f,
        1.0f,
        0.0f,
        0.0f,
        1.0f,
        0.0f, // top-left
        1.0f,
        -1.0f,
        1.0f,
        1.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0f, // bottom-left
        // bottom face
        -1.0f,
        -1.0f,
        -1.0f,
        0.0f,
        -1.0f,
        0.0f,
        0.0f,
        1.0f, // top-right
        1.0f,
        -1.0f,
        -1.0f,
        0.0f,
        -1.0f,
        0.0f,
        1.0f,
        1.0f, // top-left
        1.0f,
        -1.0f,
        1.0f,
        0.0f,
        -1.0f,
        0.0f,
        1.0f,
        0.0f, // bottom-left
        1.0f,
        -1.0f,
        1.0f,
        0.0f,
        -1.0f,
        0.0f,
        1.0f,
        0.0f, // bottom-left
        -1.0f,
        -1.0f,
        1.0f,
        0.0f,
        -1.0f,
        0.0f,
        0.0f,
        0.0f, // bottom-right
        -1.0f,
        -1.0f,
        -1.0f,
        0.0f,
        -1.0f,
        0.0f,
        0.0f,
        1.0f, // top-right
        // top face
        -1.0f,
        1.0f,
        -1.0f,
        0.0f,
        1.0f,
        0.0f,
        0.0f,
        1.0f, // top-left
        1.0f,
        1.0f,
        1.0f,
        0.0f,
        1.0f,
        0.0f,
        1.0f,
        0.0f, // bottom-right
        1.0f,
        1.0f,
        -1.0f,
        0.0f,
        1.0f,
        0.0f,
        1.0f,
        1.0f, // top-right
        1.0f,
        1.0f,
        1.0f,
        0.0f,
        1.0f,
        0.0f,
        1.0f,
        0.0f, // bottom-right
        -1.0f,
        1.0f,
        -1.0f,
        0.0f,
        1.0f,
        0.0f,
        0.0f,
        1.0f, // top-left
        -1.0f,
        1.0f,
        1.0f,
        0.0f,
        1.0f,
        0.0f,
        0.0f,
        0.0f // bottom-left
    };

    m_cube_vbo = vk::Buffer::create(backend, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, sizeof(cube_vertices), VMA_MEMORY_USAGE_GPU_ONLY, 0, cube_vertices);

    vk::DescriptorSetLayout::Desc buffer_array_ds_layout_desc;

    buffer_array_ds_layout_desc.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    m_ds_layout = vk::DescriptorSetLayout::create(backend, buffer_array_ds_layout_desc);
    m_ubo       = vk::Buffer::create(backend, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(HosekWilkieUBO), VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT);
    m_ds        = backend->allocate_descriptor_set(m_ds_layout);

    VkDescriptorBufferInfo ubo_info;

    ubo_info.buffer = m_ubo->handle();
    ubo_info.offset = 0;
    ubo_info.range  = VK_WHOLE_SIZE;

    VkWriteDescriptorSet write_data;
    HELIOS_ZERO_MEMORY(write_data);

    write_data.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_data.descriptorCount = 1;
    write_data.pBufferInfo     = &ubo_info;
    write_data.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write_data.dstBinding      = 0;
    write_data.dstSet          = m_ds->handle();

    vkUpdateDescriptorSets(backend->device(), 1, &write_data, 0, nullptr);

    // ---------------------------------------------------------------------------
    // Create shader modules
    // ---------------------------------------------------------------------------

    std::vector<char> spirv;

    vk::ShaderModule::Ptr vs = vk::ShaderModule::create_from_file(backend, "assets/shader/procedural_sky.vert.spv");
    vk::ShaderModule::Ptr fs = vk::ShaderModule::create_from_file(backend, "assets/shader/procedural_sky.frag.spv");

    vk::GraphicsPipeline::Desc pso_desc;

    pso_desc.add_shader_stage(VK_SHADER_STAGE_VERTEX_BIT, vs, "main")
        .add_shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT, fs, "main");

    // ---------------------------------------------------------------------------
    // Create vertex input state
    // ---------------------------------------------------------------------------

    vk::VertexInputStateDesc vertex_input_state_desc;

    struct SkyboxVertex
    {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 texcoord;
    };

    vertex_input_state_desc.add_binding_desc(0, sizeof(SkyboxVertex), VK_VERTEX_INPUT_RATE_VERTEX);

    vertex_input_state_desc.add_attribute_desc(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0);
    vertex_input_state_desc.add_attribute_desc(1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(SkyboxVertex, normal));
    vertex_input_state_desc.add_attribute_desc(2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(SkyboxVertex, texcoord));

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

    pl_desc.add_descriptor_set_layout(m_ds_layout);
    pl_desc.add_push_constant_range(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(HosekWilkiePushConstants));

    m_cubemap_pipeline_layout = vk::PipelineLayout::create(backend, pl_desc);

    pso_desc.set_pipeline_layout(m_cubemap_pipeline_layout);

    // ---------------------------------------------------------------------------
    // Create dynamic state
    // ---------------------------------------------------------------------------

    pso_desc.add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
        .add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR);

    pso_desc.set_render_pass(m_cubemap_renderpass);

    // ---------------------------------------------------------------------------
    // Create line list pipeline
    // ---------------------------------------------------------------------------

    input_assembly_state_desc.set_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    pso_desc.set_input_assembly_state(input_assembly_state_desc);

    m_cubemap_pipeline = vk::GraphicsPipeline::create(backend, pso_desc);
}

// -----------------------------------------------------------------------------------------------------------------------------------

HosekWilkieSkyModel::~HosekWilkieSkyModel()
{
    m_ds.reset();
    m_ds_layout.reset();
    m_ubo.reset();
    m_cubemap_pipeline.reset();
    m_cubemap_pipeline_layout.reset();
    m_face_framebuffers.clear();
    m_face_image_views.clear();
    m_cubemap_renderpass.reset();
    m_cubemap_image_view.reset();
    m_cubemap_image.reset();
}

// -----------------------------------------------------------------------------------------------------------------------------------

void HosekWilkieSkyModel::update(vk::CommandBuffer::Ptr cmd_buf, glm::vec3 direction)
{
    HELIOS_SCOPED_SAMPLE("Procedural Sky");

    const float sunTheta = std::acos(glm::clamp(direction.y, 0.f, 1.f));

    for (int i = 0; i < 3; ++i)
    {
        A[i] = evaluate(datasetsRGB[i] + 0, 9, m_turbidity, m_albedo, sunTheta);
        B[i] = evaluate(datasetsRGB[i] + 1, 9, m_turbidity, m_albedo, sunTheta);
        C[i] = evaluate(datasetsRGB[i] + 2, 9, m_turbidity, m_albedo, sunTheta);
        D[i] = evaluate(datasetsRGB[i] + 3, 9, m_turbidity, m_albedo, sunTheta);
        E[i] = evaluate(datasetsRGB[i] + 4, 9, m_turbidity, m_albedo, sunTheta);
        F[i] = evaluate(datasetsRGB[i] + 5, 9, m_turbidity, m_albedo, sunTheta);
        G[i] = evaluate(datasetsRGB[i] + 6, 9, m_turbidity, m_albedo, sunTheta);

        // Swapped in the dataset
        H[i] = evaluate(datasetsRGB[i] + 8, 9, m_turbidity, m_albedo, sunTheta);
        I[i] = evaluate(datasetsRGB[i] + 7, 9, m_turbidity, m_albedo, sunTheta);

        Z[i] = evaluate(datasetsRGBRad[i], 1, m_turbidity, m_albedo, sunTheta);
    }

    if (m_normalized_sun_y)
    {
        glm::vec3 S = hosek_wilkie(std::cos(sunTheta), 0, 1.f, A, B, C, D, E, F, G, H, I) * Z;
        Z /= glm::dot(S, glm::vec3(0.2126, 0.7152, 0.0722));
        Z *= m_normalized_sun_y;
    }

    HosekWilkieUBO ubo;

    ubo.A = glm::vec4(A, 0.0f);
    ubo.B = glm::vec4(B, 0.0f);
    ubo.C = glm::vec4(C, 0.0f);
    ubo.D = glm::vec4(D, 0.0f);
    ubo.E = glm::vec4(E, 0.0f);
    ubo.F = glm::vec4(F, 0.0f);
    ubo.G = glm::vec4(G, 0.0f);
    ubo.H = glm::vec4(H, 0.0f);
    ubo.I = glm::vec4(I, 0.0f);
    ubo.Z = glm::vec4(Z, 0.0f);

    memcpy(m_ubo->mapped_ptr(), &ubo, sizeof(HosekWilkieUBO));

    vkCmdBindPipeline(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_cubemap_pipeline->handle());

    const VkDescriptorSet sets[] = { m_ds->handle() };

    vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_cubemap_pipeline_layout->handle(), 0, 1, sets, 0, nullptr);

    for (int i = 0; i < 6; i++)
    {
        VkClearValue clear_value;

        clear_value.color.float32[0] = 0.0f;
        clear_value.color.float32[1] = 0.0f;
        clear_value.color.float32[2] = 0.0f;
        clear_value.color.float32[3] = 1.0f;

        VkRenderPassBeginInfo info    = {};
        info.sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass               = m_cubemap_renderpass->handle();
        info.framebuffer              = m_face_framebuffers[i]->handle();
        info.renderArea.extent.width  = SKY_CUBEMAP_SIZE;
        info.renderArea.extent.height = SKY_CUBEMAP_SIZE;
        info.clearValueCount          = 1;
        info.pClearValues             = &clear_value;

        vkCmdBeginRenderPass(cmd_buf->handle(), &info, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport vp;

        vp.x        = 0.0f;
        vp.y        = 0.0f;
        vp.width    = (float)SKY_CUBEMAP_SIZE;
        vp.height   = (float)SKY_CUBEMAP_SIZE;
        vp.minDepth = 0.0f;
        vp.maxDepth = 1.0f;

        vkCmdSetViewport(cmd_buf->handle(), 0, 1, &vp);

        VkRect2D scissor_rect;

        scissor_rect.extent.width  = SKY_CUBEMAP_SIZE;
        scissor_rect.extent.height = SKY_CUBEMAP_SIZE;
        scissor_rect.offset.x      = 0;
        scissor_rect.offset.y      = 0;

        vkCmdSetScissor(cmd_buf->handle(), 0, 1, &scissor_rect);

        HosekWilkiePushConstants push_constants;

        push_constants.view_projection = m_view_projection_mats[i];
        push_constants.direction       = direction;

        vkCmdPushConstants(cmd_buf->handle(), m_cubemap_pipeline_layout->handle(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(HosekWilkiePushConstants), &push_constants);

        const VkBuffer     buffer = m_cube_vbo->handle();
        const VkDeviceSize size   = 0;
        vkCmdBindVertexBuffers(cmd_buf->handle(), 0, 1, &buffer, &size);

        vkCmdDraw(cmd_buf->handle(), 36, 1, 0, 0);

        vkCmdEndRenderPass(cmd_buf->handle());
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------
} // namespace helios