#include <resource/scene.h>
#include <resource/mesh.h>
#include <resource/material.h>
#include <resource/texture.h>
#include <utility/macros.h>
#include <vk_mem_alloc.h>
#include <unordered_map>
#include <unordered_set>

namespace lumen
{
// -----------------------------------------------------------------------------------------------------------------------------------

struct CameraData
{
    LUMEN_ALIGNED(16)
    glm::mat4 view_inverse;
    LUMEN_ALIGNED(16)
    glm::mat4 proj_inverse;
    LUMEN_ALIGNED(16)
    glm::mat4 view;
    LUMEN_ALIGNED(16)
    glm::mat4 proj;
    LUMEN_ALIGNED(16)
    glm::vec4 cam_pos;
};
// -----------------------------------------------------------------------------------------------------------------------------------

struct GPUMaterial
{
    glm::ivec4 texture_indices0 = glm::ivec4(-1); // x: albedo, y: normals, z: roughness, w: metallic
    glm::ivec4 texture_indices1 = glm::ivec4(-1); // x: emissive, z: roughness_channel, w: metallic_channel
    glm::vec4  albedo;
    glm::vec4  emissive;
    glm::vec4  roughness_metallic;
};

// -----------------------------------------------------------------------------------------------------------------------------------

struct GPULight
{
    glm::vec4 light_data0; // x: light type, yzw: color
    glm::vec4 light_data1; // xyz: direction, w: intensity
    glm::vec4 light_data2; // x: range, y: cone angle
};

// -----------------------------------------------------------------------------------------------------------------------------------

Node::Node(const NodeType& type, const std::string& name) :
    m_type(type), m_name(name)
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

Node::~Node()
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Node::mid_frame_cleanup()
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Node::add_child(Node::Ptr child)
{
    m_is_heirarchy_dirty = true;
    child->m_parent      = this;
    m_children.push_back(child);
}

// -----------------------------------------------------------------------------------------------------------------------------------

Node::Ptr Node::find_child(const std::string& name)
{
    for (auto child : m_children)
    {
        if (child->m_name == name)
            return child;
    }

    return nullptr;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Node::remove_child(const std::string& name)
{
    m_is_heirarchy_dirty = true;
    int child_to_remove  = -1;

    for (int i = 0; i < m_children.size(); i++)
    {
        if (m_children[i]->m_name == name)
        {
            child_to_remove = i;
            break;
        }
    }

    if (child_to_remove != -1)
    {
        m_children[child_to_remove]->mid_frame_cleanup();
        m_children.erase(m_children.begin() + child_to_remove);
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Node::update_children(RenderState& render_state)
{
    if (m_is_heirarchy_dirty)
    {
        render_state.m_scene_state = SCENE_STATE_HIERARCHY_UPDATED;
        m_is_heirarchy_dirty       = false;
    }

    for (auto& child : m_children)
        child->update(render_state);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Node::mark_transforms_as_dirty()
{
    m_is_transform_dirty = true;

    for (auto& child : m_children)
        child->mark_transforms_as_dirty();
}

// -----------------------------------------------------------------------------------------------------------------------------------

TransformNode::TransformNode(const NodeType& type, const std::string& name) :
    Node(type, name)
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

TransformNode::~TransformNode()
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

void TransformNode::update(RenderState& render_state)
{
    if (m_is_transform_dirty)
    {
        glm::mat4 R = glm::mat4_cast(m_orientation);
        glm::mat4 S = glm::scale(glm::mat4(1.0f), m_scale);
        glm::mat4 T = glm::translate(glm::mat4(1.0f), m_position);

        m_prev_model_matrix          = m_model_matrix;
        m_model_matrix_without_scale = T * R;
        m_model_matrix               = m_model_matrix_without_scale * S;

        TransformNode* parent_transform = dynamic_cast<TransformNode*>(m_parent);

        if (parent_transform)
            m_model_matrix = m_model_matrix * parent_transform->m_model_matrix;

        if (render_state.m_scene_state != SCENE_STATE_HIERARCHY_UPDATED)
            render_state.m_scene_state = SCENE_STATE_TRANSFORMS_UPDATED;

        m_is_transform_dirty = false;
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

glm::vec3 TransformNode::forward()
{
    return m_orientation * glm::vec3(0.0f, 0.0f, 1.0f);
}

// -----------------------------------------------------------------------------------------------------------------------------------

glm::vec3 TransformNode::up()
{
    return m_orientation * glm::vec3(0.0f, 1.0f, 0.0f);
}

// -----------------------------------------------------------------------------------------------------------------------------------

glm::vec3 TransformNode::left()
{
    return m_orientation * glm::vec3(1.0f, 0.0f, 0.0f);
}

// -----------------------------------------------------------------------------------------------------------------------------------

glm::vec3 TransformNode::position()
{
    return m_position;
}

// -----------------------------------------------------------------------------------------------------------------------------------

glm::mat4 TransformNode::model_matrix()
{
    return m_model_matrix;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void TransformNode::set_orientation(const glm::quat& q)
{
    mark_transforms_as_dirty();

    m_orientation = q;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void TransformNode::set_orientation_from_euler_yxz(const glm::vec3& e)
{
    mark_transforms_as_dirty();

    glm::quat pitch = glm::quat(glm::vec3(glm::radians(e.x), glm::radians(0.0f), glm::radians(0.0f)));
    glm::quat yaw   = glm::quat(glm::vec3(glm::radians(0.0f), glm::radians(e.y), glm::radians(0.0f)));
    glm::quat roll  = glm::quat(glm::vec3(glm::radians(0.0f), glm::radians(0.0f), glm::radians(e.z)));

    m_orientation = yaw * pitch * roll;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void TransformNode::set_orientation_from_euler_xyz(const glm::vec3& e)
{
    mark_transforms_as_dirty();

    glm::quat pitch = glm::quat(glm::vec3(glm::radians(e.x), glm::radians(0.0f), glm::radians(0.0f)));
    glm::quat yaw   = glm::quat(glm::vec3(glm::radians(0.0f), glm::radians(e.y), glm::radians(0.0f)));
    glm::quat roll  = glm::quat(glm::vec3(glm::radians(0.0f), glm::radians(0.0f), glm::radians(e.z)));

    m_orientation = pitch * yaw * roll;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void TransformNode::set_position(const glm::vec3& position)
{
    mark_transforms_as_dirty();

    m_position = position;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void TransformNode::set_scale(const glm::vec3& scale)
{
    mark_transforms_as_dirty();

    m_scale = scale;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void TransformNode::move(const glm::vec3& displacement)
{
    mark_transforms_as_dirty();

    m_position += displacement;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void TransformNode::rotate_euler_yxz(const glm::vec3& e)
{
    mark_transforms_as_dirty();

    glm::quat pitch = glm::quat(glm::vec3(glm::radians(e.x), glm::radians(0.0f), glm::radians(0.0f)));
    glm::quat yaw   = glm::quat(glm::vec3(glm::radians(0.0f), glm::radians(e.y), glm::radians(0.0f)));
    glm::quat roll  = glm::quat(glm::vec3(glm::radians(0.0f), glm::radians(0.0f), glm::radians(e.z)));

    glm::quat delta = yaw * pitch * roll;
    m_orientation   = m_orientation * delta;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void TransformNode::rotate_euler_xyz(const glm::vec3& e)
{
    mark_transforms_as_dirty();

    glm::quat pitch = glm::quat(glm::vec3(glm::radians(e.x), glm::radians(0.0f), glm::radians(0.0f)));
    glm::quat yaw   = glm::quat(glm::vec3(glm::radians(0.0f), glm::radians(e.y), glm::radians(0.0f)));
    glm::quat roll  = glm::quat(glm::vec3(glm::radians(0.0f), glm::radians(0.0f), glm::radians(e.z)));

    glm::quat delta = pitch * yaw * roll;
    m_orientation   = m_orientation * delta;
}

// -----------------------------------------------------------------------------------------------------------------------------------

MeshNode::MeshNode(const std::string& name) :
    TransformNode(NODE_MESH, name)
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

MeshNode::~MeshNode()
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

void MeshNode::update(RenderState& render_state)
{
    if (m_is_enabled)
    {
        TransformNode::update(render_state);

        render_state.m_meshes.push_back(this);

        update_children(render_state);
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

void MeshNode::mid_frame_cleanup()
{
    if (m_mesh)
    {
        auto backend = m_mesh->backend().lock();

        if (backend)
            backend->queue_object_deletion(m_mesh);
    }

    mid_frame_material_cleanup();
}

// -----------------------------------------------------------------------------------------------------------------------------------

void MeshNode::mid_frame_material_cleanup()
{
    if (m_material_override)
    {
        auto backend = m_material_override->backend().lock();
        backend->queue_object_deletion(m_material_override);
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

void MeshNode::set_mesh(std::shared_ptr<Mesh> mesh)
{
    mid_frame_cleanup();

    m_mesh = mesh;

    create_instance_data_buffer();
}

// -----------------------------------------------------------------------------------------------------------------------------------

void MeshNode::set_material_override(std::shared_ptr<Material> material_override)
{
    mid_frame_material_cleanup();

    m_material_override = material_override;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void MeshNode::create_instance_data_buffer()
{
    if (m_mesh)
    {
        auto backend = m_mesh->backend().lock();

        if (backend)
        {
            backend->queue_object_deletion(m_instance_data_buffer);
            m_instance_data_buffer = vk::Buffer::create(backend, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(glm::mat4) + sizeof(uint32_t) * (m_mesh->sub_meshes().size() + 1), VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT);
        }
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

DirectionalLightNode::DirectionalLightNode(const std::string& name) :
    TransformNode(NODE_DIRECTIONAL_LIGHT, name)
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

DirectionalLightNode::~DirectionalLightNode()
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

void DirectionalLightNode::update(RenderState& render_state)
{
    if (m_is_enabled)
    {
        TransformNode::update(render_state);

        render_state.m_directional_lights.push_back(this);

        update_children(render_state);
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

SpotLightNode::SpotLightNode(const std::string& name) :
    TransformNode(NODE_SPOT_LIGHT, name)
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

SpotLightNode::~SpotLightNode()
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

void SpotLightNode::update(RenderState& render_state)
{
    if (m_is_enabled)
    {
        TransformNode::update(render_state);

        render_state.m_spot_lights.push_back(this);

        update_children(render_state);
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

PointLightNode::PointLightNode(const std::string& name) :
    TransformNode(NODE_POINT_LIGHT, name)
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

PointLightNode::~PointLightNode()
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

void PointLightNode::update(RenderState& render_state)
{
    if (m_is_enabled)
    {
        TransformNode::update(render_state);

        render_state.m_point_lights.push_back(this);

        update_children(render_state);
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

CameraNode::CameraNode(const std::string& name) :
    TransformNode(NODE_CAMERA, name)
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

CameraNode::~CameraNode()
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

void CameraNode::update(RenderState& render_state)
{
    if (m_is_enabled)
    {
        TransformNode::update(render_state);

        m_projection_matrix = glm::perspective(glm::radians(m_fov), 1.0f, m_near_plane, m_far_plane);
        m_view_matrix       = glm::inverse(m_model_matrix_without_scale);

        if (!render_state.m_camera)
            render_state.m_camera = this;

        update_children(render_state);
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

IBLNode::IBLNode(const std::string& name) :
    Node(NODE_IBL, name)
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

IBLNode::~IBLNode()
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

void IBLNode::update(RenderState& render_state)
{
    if (m_is_enabled)
    {
        if (!render_state.m_ibl_environment_map)
            render_state.m_ibl_environment_map = this;

        update_children(render_state);
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

void IBLNode::set_image(std::shared_ptr<TextureCube> image)
{
    mid_frame_cleanup();

    m_image = image;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void IBLNode::mid_frame_cleanup()
{
    if (m_image)
    {
        auto backend = m_image->backend().lock();

        if (backend)
            backend->queue_object_deletion(m_image);
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

RenderState::RenderState()
{
    m_meshes.reserve(100000);
    m_directional_lights.reserve(100000);
    m_spot_lights.reserve(100000);
    m_point_lights.reserve(100000);
}

// -----------------------------------------------------------------------------------------------------------------------------------

RenderState::~RenderState()
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

void RenderState::clear()
{
    m_meshes.clear();
    m_directional_lights.clear();
    m_spot_lights.clear();
    m_point_lights.clear();
    m_camera               = nullptr;
    m_ibl_environment_map  = nullptr;
    m_read_image_ds        = nullptr;
    m_write_image_ds       = nullptr;
    m_scene_ds             = nullptr;
    m_cmd_buffer           = nullptr;
    m_scene                = nullptr;
    m_vbo_ds               = nullptr;
    m_ibo_ds               = nullptr;
    m_instance_ds          = nullptr;
    m_texture_ds           = nullptr;
    m_scene_state          = SCENE_STATE_READY;
    m_camera_buffer_offset = 0;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void RenderState::setup(vk::CommandBuffer::Ptr cmd_buffer)
{
    clear();
    m_cmd_buffer = cmd_buffer;
}

// -----------------------------------------------------------------------------------------------------------------------------------

Scene::Ptr Scene::create(vk::Backend::Ptr backend, const std::string& name, Node::Ptr root)
{
    return std::shared_ptr<Scene>(new Scene(backend, name, root));
}

// -----------------------------------------------------------------------------------------------------------------------------------

Scene::Scene(vk::Backend::Ptr backend, const std::string& name, Node::Ptr root) :
    m_name(name), m_backend(backend), m_root(root)
{
    // Create TLAS
    vk::AccelerationStructure::Desc desc;

    desc.set_instance_count(1);
    desc.set_type(VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV);
    desc.set_flags(VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_NV | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_NV);

    m_tlas.tlas = vk::AccelerationStructure::create(backend, desc);

    // Allocate instance buffer
    m_tlas.instance_buffer_host = vk::Buffer::create(backend, VK_BUFFER_USAGE_RAY_TRACING_BIT_NV | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, sizeof(RTGeometryInstance) * MAX_SCENE_MESH_INSTANCE_COUNT, VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT);

    // Allocate TLAS scratch buffer
    VkAccelerationStructureMemoryRequirementsInfoNV memory_requirements_info;
    memory_requirements_info.sType                 = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
    memory_requirements_info.pNext                 = nullptr;
    memory_requirements_info.type                  = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_NV;
    memory_requirements_info.accelerationStructure = m_tlas.tlas->handle();

    VkMemoryRequirements2 mem_req_blas;
    vkGetAccelerationStructureMemoryRequirementsNV(backend->device(), &memory_requirements_info, &mem_req_blas);

    m_tlas.scratch_buffer = vk::Buffer::create(backend, VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, mem_req_blas.memoryRequirements.size, VMA_MEMORY_USAGE_GPU_ONLY, 0);

    vk::DescriptorPool::Desc dp_desc;

    dp_desc.set_max_sets(5)
        .add_pool_size(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1)
        .add_pool_size(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_SCENE_MATERIAL_TEXTURE_COUNT)
        .add_pool_size(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 5 * MAX_SCENE_MESH_INSTANCE_COUNT)
        .add_pool_size(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV, 1);

    m_descriptor_pool = vk::DescriptorPool::create(backend, dp_desc);

    // Allocate descriptor set
    m_scene_descriptor_set    = vk::DescriptorSet::create(backend, backend->scene_descriptor_set_layout(), m_descriptor_pool);
    m_vbo_descriptor_set      = vk::DescriptorSet::create(backend, backend->buffer_array_descriptor_set_layout(), m_descriptor_pool);
    m_ibo_descriptor_set      = vk::DescriptorSet::create(backend, backend->buffer_array_descriptor_set_layout(), m_descriptor_pool);
    m_instance_descriptor_set = vk::DescriptorSet::create(backend, backend->buffer_array_descriptor_set_layout(), m_descriptor_pool);
    m_textures_descriptor_set = vk::DescriptorSet::create(backend, backend->combined_sampler_array_descriptor_set_layout(), m_descriptor_pool);

    // Create camera buffer
    m_camera_buffer_aligned_size = backend->aligned_dynamic_ubo_size(sizeof(CameraData));
    m_camera_buffer              = vk::Buffer::create(backend, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, m_camera_buffer_aligned_size * vk::Backend::kMaxFramesInFlight, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT);

    // Create light data buffer
    m_light_data_buffer = vk::Buffer::create(backend, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(GPULight) * MAX_SCENE_LIGHT_COUNT, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT);

    // Create material data buffer
    m_material_data_buffer = vk::Buffer::create(backend, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(GPUMaterial) * MAX_SCENE_MATERIAL_COUNT, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT);
}

// -----------------------------------------------------------------------------------------------------------------------------------

Scene::~Scene()
{
    m_textures_descriptor_set.reset();
    m_instance_descriptor_set.reset();
    m_ibo_descriptor_set.reset();
    m_vbo_descriptor_set.reset();
    m_scene_descriptor_set.reset();
    m_descriptor_pool.reset();
    m_tlas.scratch_buffer.reset();
    m_tlas.instance_buffer_host.reset();
    m_tlas.tlas.reset();
    m_light_data_buffer.reset();
    m_material_data_buffer.reset();
    m_root.reset();
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Scene::update(RenderState& render_state)
{
    m_root->update(render_state);

    // Copy camera data
    if (render_state.m_camera)
    {
        auto backend = m_backend.lock();

        CameraData camera_data;

        camera_data.view         = render_state.m_camera->view_matrix();
        camera_data.view_inverse = glm::inverse(render_state.m_camera->view_matrix());
        camera_data.proj         = render_state.m_camera->projection_matrix();
        camera_data.view_inverse = glm::inverse(render_state.m_camera->view_matrix());
        camera_data.proj_inverse = glm::inverse(render_state.m_camera->projection_matrix());
        camera_data.cam_pos      = glm::vec4(render_state.m_camera->position(), 0.0f);

        uint32_t camera_buffer_offset = m_camera_buffer_aligned_size * backend->current_frame_idx();

        uint8_t* ptr = (uint8_t*)m_camera_buffer->mapped_ptr();
        memcpy(ptr + camera_buffer_offset, &camera_data, sizeof(CameraData));

        render_state.m_camera_buffer_offset = camera_buffer_offset;
    }

    render_state.m_scene_ds    = m_scene_descriptor_set;
    render_state.m_vbo_ds      = m_vbo_descriptor_set;
    render_state.m_ibo_ds      = m_ibo_descriptor_set;
    render_state.m_instance_ds = m_instance_descriptor_set;
    render_state.m_texture_ds  = m_textures_descriptor_set;
    render_state.m_scene       = this;

    create_gpu_resources(render_state);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Scene::create_gpu_resources(RenderState& render_state)
{
    if (render_state.m_scene_state != SCENE_STATE_READY)
    {
        if (render_state.m_scene_state == SCENE_STATE_HIERARCHY_UPDATED)
        {
            auto backend = m_backend.lock();

            std::unordered_set<uint32_t>           processed_meshes;
            std::unordered_set<uint32_t>           processed_materials;
            std::unordered_set<uint32_t>           processed_textures;
            std::unordered_map<uint32_t, uint32_t> global_material_indices;
            std::unordered_map<uint32_t, uint32_t> global_mesh_indices;

            std::vector<VkDescriptorBufferInfo> vbo_descriptors;
            std::vector<VkDescriptorBufferInfo> ibo_descriptors;
            std::vector<VkDescriptorImageInfo>  image_descriptors;
            std::vector<VkDescriptorBufferInfo> instance_data_descriptors;
            uint32_t                            gpu_material_counter     = 0;
            GPUMaterial*                        material_buffer          = (GPUMaterial*)m_material_data_buffer->mapped_ptr();
            RTGeometryInstance*                 geometry_instance_buffer = (RTGeometryInstance*)m_tlas.instance_buffer_host->mapped_ptr();

            for (int mesh_node_idx = 0; mesh_node_idx < render_state.m_meshes.size(); mesh_node_idx++)
            {
                auto&       mesh_node = render_state.m_meshes[mesh_node_idx];
                auto&       mesh      = mesh_node->mesh();
                const auto& materials = mesh->materials();
                const auto& submeshes = mesh->sub_meshes();

                if (processed_meshes.find(mesh->id()) == processed_meshes.end())
                {
                    processed_meshes.insert(mesh->id());

                    global_mesh_indices[mesh->id()] = global_mesh_indices.size();

                    VkDescriptorBufferInfo ibo_info;

                    ibo_info.buffer = mesh->index_buffer()->handle();
                    ibo_info.offset = 0;
                    ibo_info.range  = VK_WHOLE_SIZE;

                    ibo_descriptors.push_back(ibo_info);

                    VkDescriptorBufferInfo vbo_info;

                    vbo_info.buffer = mesh->vertex_buffer()->handle();
                    vbo_info.offset = 0;
                    vbo_info.range  = VK_WHOLE_SIZE;

                    vbo_descriptors.push_back(vbo_info);

                    for (uint32_t i = 0; i < submeshes.size(); i++)
                    {
                        auto material = materials[submeshes[i].mat_idx];

                        if (mesh_node->material_override())
                            material = mesh_node->material_override();

                        if (processed_materials.find(material->id()) == processed_materials.end())
                        {
                            processed_materials.insert(material->id());

                            GPUMaterial& gpu_material = material_buffer[gpu_material_counter++];

                            // Fill GPUMaterial
                            if (material->albedo_texture())
                            {
                                auto texture = material->albedo_texture();

                                if (processed_textures.find(texture->id()) == processed_textures.end())
                                {
                                    processed_textures.insert(texture->id());

                                    VkDescriptorImageInfo image_info;

                                    image_info.sampler     = backend->trilinear_sampler()->handle();
                                    image_info.imageView   = texture->image_view()->handle();
                                    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                                    gpu_material.texture_indices0.x = image_descriptors.size();

                                    image_descriptors.push_back(image_info);
                                }
                            }
                            else
                                gpu_material.albedo = material->albedo_value();

                            if (material->normal_texture())
                            {
                                auto texture = material->normal_texture();

                                if (processed_textures.find(texture->id()) == processed_textures.end())
                                {
                                    processed_textures.insert(texture->id());

                                    VkDescriptorImageInfo image_info;

                                    image_info.sampler     = backend->trilinear_sampler()->handle();
                                    image_info.imageView   = texture->image_view()->handle();
                                    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                                    gpu_material.texture_indices0.y = image_descriptors.size();

                                    image_descriptors.push_back(image_info);
                                }
                            }

                            if (material->roughness_texture())
                            {
                                auto texture = material->roughness_texture();

                                if (processed_textures.find(texture->id()) == processed_textures.end())
                                {
                                    processed_textures.insert(texture->id());

                                    VkDescriptorImageInfo image_info;

                                    image_info.sampler     = backend->trilinear_sampler()->handle();
                                    image_info.imageView   = texture->image_view()->handle();
                                    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                                    gpu_material.texture_indices0.z = image_descriptors.size();
                                    gpu_material.texture_indices1.z = material->roughness_texture_info().array_index;

                                    image_descriptors.push_back(image_info);
                                }
                            }
                            else
                                gpu_material.roughness_metallic.x = material->roughness_value();

                            if (material->metallic_texture())
                            {
                                auto texture = material->metallic_texture();

                                if (processed_textures.find(texture->id()) == processed_textures.end())
                                {
                                    processed_textures.insert(texture->id());

                                    VkDescriptorImageInfo image_info;

                                    image_info.sampler     = backend->trilinear_sampler()->handle();
                                    image_info.imageView   = texture->image_view()->handle();
                                    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                                    gpu_material.texture_indices0.w = image_descriptors.size();
                                    gpu_material.texture_indices1.w = material->metallic_texture_info().array_index;

                                    image_descriptors.push_back(image_info);
                                }
                            }
                            else
                                gpu_material.roughness_metallic.y = material->metallic_value();

                            if (material->emissive_texture())
                            {
                                auto texture = material->emissive_texture();

                                if (processed_textures.find(texture->id()) == processed_textures.end())
                                {
                                    processed_textures.insert(texture->id());

                                    VkDescriptorImageInfo image_info;

                                    image_info.sampler     = backend->trilinear_sampler()->handle();
                                    image_info.imageView   = texture->image_view()->handle();
                                    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                                    gpu_material.texture_indices1.x = image_descriptors.size();

                                    image_descriptors.push_back(image_info);
                                }
                            }
                            else
                                gpu_material.emissive = material->emissive_value();

                            global_material_indices[material->id()] = gpu_material_counter - 1;
                        }
                    }
                }

                VkDescriptorBufferInfo instance_data_info;

                instance_data_info.buffer = mesh_node->instance_data_buffer()->handle();
                instance_data_info.offset = 0;
                instance_data_info.range  = VK_WHOLE_SIZE;

                instance_data_descriptors.push_back(instance_data_info);

                // Copy geometry instance data
                RTGeometryInstance& rt_instance = geometry_instance_buffer[mesh_node_idx];

                rt_instance.transform                   = glm::mat3x4(mesh_node->model_matrix());
                rt_instance.instanceCustomIndex         = mesh_node_idx;
                rt_instance.mask                        = 0xff;
                rt_instance.instanceOffset              = 0;
                rt_instance.flags                       = VK_GEOMETRY_INSTANCE_TRIANGLE_CULL_DISABLE_BIT_NV;
                rt_instance.accelerationStructureHandle = mesh->acceleration_structure()->opaque_handle();

                // Update instance data
                uint8_t*   base_instance_data_ptr = (uint8_t*)mesh_node->instance_data_buffer()->mapped_ptr();
                glm::mat4* instance_transform     = (glm::mat4*)base_instance_data_ptr;

                instance_transform[0] = mesh_node->model_matrix();

                uint32_t* instance_indices = (uint32_t*)(base_instance_data_ptr + sizeof(glm::mat4));

                // Set mesh data index
                instance_indices[0] = global_mesh_indices[mesh->id()];

                // Set submesh materials
                for (uint32_t i = 0; i < submeshes.size(); i++)
                {
                    auto material = materials[submeshes[i].mat_idx];

                    if (mesh_node->material_override())
                        material = mesh_node->material_override();

                    instance_indices[i + 1] = global_material_indices[material->id()];
                }
            }

            VkDescriptorBufferInfo camera_buffer_info;

            camera_buffer_info.buffer = m_camera_buffer->handle();
            camera_buffer_info.offset = 0;
            camera_buffer_info.range  = VK_WHOLE_SIZE;

            VkDescriptorBufferInfo material_buffer_info;

            material_buffer_info.buffer = m_material_data_buffer->handle();
            material_buffer_info.offset = 0;
            material_buffer_info.range  = VK_WHOLE_SIZE;

            VkDescriptorBufferInfo light_buffer_info;

            light_buffer_info.buffer = m_light_data_buffer->handle();
            light_buffer_info.offset = 0;
            light_buffer_info.range  = VK_WHOLE_SIZE;

            VkWriteDescriptorSet write_data[8];

            LUMEN_ZERO_MEMORY(write_data[0]);
            LUMEN_ZERO_MEMORY(write_data[1]);
            LUMEN_ZERO_MEMORY(write_data[2]);
            LUMEN_ZERO_MEMORY(write_data[3]);
            LUMEN_ZERO_MEMORY(write_data[4]);
            LUMEN_ZERO_MEMORY(write_data[5]);
            LUMEN_ZERO_MEMORY(write_data[6]);
            LUMEN_ZERO_MEMORY(write_data[7]);

            write_data[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_data[0].descriptorCount = 1;
            write_data[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            write_data[0].pBufferInfo     = &camera_buffer_info;
            write_data[0].dstBinding      = 0;
            write_data[0].dstSet          = m_scene_descriptor_set->handle();

            write_data[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_data[1].descriptorCount = 1;
            write_data[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            write_data[1].pBufferInfo     = &material_buffer_info;
            write_data[1].dstBinding      = 1;
            write_data[1].dstSet          = m_scene_descriptor_set->handle();

            write_data[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_data[2].descriptorCount = 1;
            write_data[2].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            write_data[2].pBufferInfo     = &light_buffer_info;
            write_data[2].dstBinding      = 2;
            write_data[2].dstSet          = m_scene_descriptor_set->handle();

            VkWriteDescriptorSetAccelerationStructureNV descriptor_as;

            descriptor_as.sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_NV;
            descriptor_as.pNext                      = nullptr;
            descriptor_as.accelerationStructureCount = 1;
            descriptor_as.pAccelerationStructures    = &m_tlas.tlas->handle();

            write_data[3].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_data[3].pNext           = &descriptor_as;
            write_data[3].descriptorCount = 1;
            write_data[3].descriptorType  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;
            write_data[3].dstBinding      = 3;
            write_data[3].dstSet          = m_scene_descriptor_set->handle();

            write_data[4].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_data[4].descriptorCount = vbo_descriptors.size();
            write_data[4].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            write_data[4].pBufferInfo     = vbo_descriptors.data();
            write_data[4].dstBinding      = 0;
            write_data[4].dstSet          = m_vbo_descriptor_set->handle();

            write_data[5].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_data[5].descriptorCount = ibo_descriptors.size();
            write_data[5].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            write_data[5].pBufferInfo     = ibo_descriptors.data();
            write_data[5].dstBinding      = 0;
            write_data[5].dstSet          = m_ibo_descriptor_set->handle();

            write_data[6].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_data[6].descriptorCount = instance_data_descriptors.size();
            write_data[6].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            write_data[6].pBufferInfo     = instance_data_descriptors.data();
            write_data[6].dstBinding      = 0;
            write_data[6].dstSet          = m_instance_descriptor_set->handle();

            write_data[7].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_data[7].descriptorCount = image_descriptors.size();
            write_data[7].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write_data[7].pImageInfo      = image_descriptors.data();
            write_data[7].dstBinding      = 0;
            write_data[7].dstSet          = m_textures_descriptor_set->handle();

            vkUpdateDescriptorSets(backend->device(), image_descriptors.size() > 0 ? 8 : 7, write_data, 0, nullptr);
        }

        // Copy lights
        uint32_t  gpu_light_counter = 0;
        GPULight* light_buffer      = (GPULight*)m_light_data_buffer->mapped_ptr();

        for (int i = 0; i < render_state.m_directional_lights.size(); i++)
        {
            auto light = render_state.m_directional_lights[i];

            GPULight& gpu_light = light_buffer[gpu_light_counter++];

            gpu_light.light_data0 = glm::vec4(0.0f, light->color());
            gpu_light.light_data1 = glm::vec4(light->forward(), light->intensity());
        }

        for (int i = 0; i < render_state.m_point_lights.size(); i++)
        {
            auto light = render_state.m_point_lights[i];

            GPULight& gpu_light = light_buffer[gpu_light_counter++];

            gpu_light.light_data0 = glm::vec4(1.0f, light->color());
            gpu_light.light_data1 = glm::vec4(0.0f, 0.0f, 0.0f, light->intensity());
            gpu_light.light_data2 = glm::vec4(light->range(), 0.0f, 0.0f, 0.0f);
        }

        for (int i = 0; i < render_state.m_spot_lights.size(); i++)
        {
            auto light = render_state.m_spot_lights[i];

            GPULight& gpu_light = light_buffer[gpu_light_counter++];

            gpu_light.light_data0 = glm::vec4(2.0f, light->color());
            gpu_light.light_data1 = glm::vec4(light->forward(), light->intensity());
            gpu_light.light_data2 = glm::vec4(light->range(), light->cone_angle(), 0.0f, 0.0f);
        }
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Scene::set_root_node(Node::Ptr node)
{
    if (m_root)
        m_root->mid_frame_cleanup();

    m_root = node;
}

// -----------------------------------------------------------------------------------------------------------------------------------

Node::Ptr Scene::root_node()
{
    return m_root;
}

// -----------------------------------------------------------------------------------------------------------------------------------

Node::Ptr Scene::find_node(const std::string& name)
{
    if (m_root->name() == name)
        return m_root;
    else
        return m_root->find_child(name);
}

// -----------------------------------------------------------------------------------------------------------------------------------
} // namespace lumen