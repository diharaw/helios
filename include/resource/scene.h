#pragma once

#include <core/vk.h>
#include <glm.hpp>
#include <gtc/quaternion.hpp>
#include <memory>
#include <vector>

namespace lumen
{
#define MAX_SCENE_MESH_COUNT 100000
#define MAX_SCENE_LIGHT_COUNT 100000
#define MAX_SCENE_MATERIAL_COUNT 4096
#define MAX_SCENE_MATERIAL_TEXTURE_COUNT (MAX_SCENE_MATERIAL_COUNT * 4)

class Mesh;
class Material;
class TextureCube;

enum NodeType
{
    NODE_MESH,
    NODE_CAMERA,
    NODE_DIRECTIONAL_LIGHT,
    NODE_SPOT_LIGHT,
    NODE_POINT_LIGHT,
    NODE_IBL
};

struct RenderState;

struct AccelerationStructureData
{
    vk::AccelerationStructure::Ptr tlas;
    vk::Buffer::Ptr                instance_buffer_host;
};

class Node
{
public:
    using Ptr = std::shared_ptr<Node>;

protected:
    NodeType                           m_type;
    bool                               m_is_enabled         = true;
    bool                               m_is_transform_dirty = true;
    bool                               m_is_heirarchy_dirty = true;
    std::string                        m_name;
    Node*                              m_parent = nullptr;
    std::vector<std::shared_ptr<Node>> m_children;

public:
    Node(const NodeType& type, const std::string& name);
    virtual ~Node();

    virtual void update(RenderState& render_state) = 0;

    void        add_child(Node::Ptr child);
    Node::Ptr   find_child(const std::string& name);
    void        remove_child(const std::string& name);
    inline bool is_enabled() { return m_is_enabled; }
    inline bool is_transform_dirty() { return m_is_transform_dirty; }
    inline void enable() { m_is_enabled = true; }
    inline void disable() { m_is_enabled = false; }

protected:
    void update_children(RenderState& render_state);
    void mark_transforms_as_dirty();
};

class TransformNode : public Node
{
public:
    using Ptr = std::shared_ptr<TransformNode>;

protected:
    glm::vec3 m_position                   = glm::vec3(0.0f);
    glm::quat m_orientation                = glm::quat(glm::radians(glm::vec3(0.0f)));
    glm::vec3 m_scale                      = glm::vec3(1.0f);
    glm::mat4 m_prev_model_matrix          = glm::mat4(1.0f);
    glm::mat4 m_model_matrix               = glm::mat4(1.0f);
    glm::mat4 m_model_matrix_without_scale = glm::mat4(1.0f);

public:
    TransformNode(const NodeType& type, const std::string& name);
    ~TransformNode();

    void update(RenderState& render_state) override;

    glm::vec3 forward();
    glm::vec3 up();
    glm::vec3 left();
    glm::vec3 position();
    glm::mat4 model_matrix();
    void      set_orientation_from_euler_yxz(const glm::vec3& e);
    void      set_orientation_from_euler_xyz(const glm::vec3& e);
    void      set_position(const glm::vec3& position);
    void      set_scale(const glm::vec3& scale);
    void      rotate_euler_yxz(const glm::vec3& e);
    void      rotate_euler_xyz(const glm::vec3& e);
};

class MeshNode : public TransformNode
{
public:
    using Ptr = std::shared_ptr<MeshNode>;

private:
    std::shared_ptr<Mesh>     m_mesh;
    std::shared_ptr<Material> m_material_override;
    vk::Buffer::Ptr           m_instance_data_buffer;

public:
    MeshNode(const std::string& name);
    ~MeshNode();

    void update(RenderState& render_state) override;

    inline void                      set_mesh(std::shared_ptr<Mesh> mesh) { m_mesh = mesh; }
    inline void                      set_material_override(std::shared_ptr<Material> material_override) { m_material_override = material_override; }
    inline std::shared_ptr<Mesh>     mesh() { return m_mesh; }
    inline std::shared_ptr<Material> material_override() { return m_material_override; }
    inline vk::Buffer::Ptr           instance_data_buffer() { return m_instance_data_buffer; }
};

class DirectionalLightNode : public TransformNode
{
public:
    using Ptr = std::shared_ptr<DirectionalLightNode>;

private:
    glm::vec3 m_color;
    float     m_intensity;

public:
    DirectionalLightNode(const std::string& name);
    ~DirectionalLightNode();

    void update(RenderState& render_state) override;

    inline void      set_color(const glm::vec3& color) { m_color = color; }
    inline void      set_intensity(const float& intensity) { m_intensity = intensity; }
    inline glm::vec3 color() { return m_color; }
    inline float     intensity() { return m_intensity; }
};

class SpotLightNode : public TransformNode
{
public:
    using Ptr = std::shared_ptr<SpotLightNode>;

    glm::vec3 m_color;
    float     m_cone_angle;
    float     m_range;
    float     m_intensity;

public:
    SpotLightNode(const std::string& name);
    ~SpotLightNode();

    void update(RenderState& render_state) override;

    inline void      set_color(const glm::vec3& color) { m_color = color; }
    inline void      set_intensity(const float& intensity) { m_intensity = intensity; }
    inline void      set_range(const float& range) { m_range = range; }
    inline void      set_cone_angle(const float& cone_angle) { m_cone_angle = cone_angle; }
    inline glm::vec3 color() { return m_color; }
    inline float     intensity() { return m_intensity; }
    inline float     range() { return m_range; }
    inline float     cone_angle() { return m_cone_angle; }
};

class PointLightNode : public TransformNode
{
public:
    using Ptr = std::shared_ptr<PointLightNode>;

private:
    glm::vec3 m_color;
    float     m_range;
    float     m_intensity;

public:
    PointLightNode(const std::string& name);
    ~PointLightNode();

    void update(RenderState& render_state) override;

    inline void      set_color(const glm::vec3& color) { m_color = color; }
    inline void      set_intensity(const float& intensity) { m_intensity = intensity; }
    inline void      set_range(const float& range) { m_range = range; }
    inline glm::vec3 color() { return m_color; }
    inline float     intensity() { return m_intensity; }
    inline float     range() { return m_range; }
};

class CameraNode : public TransformNode
{
public:
    using Ptr = std::shared_ptr<CameraNode>;

private:
    float     m_near_plane        = 1.0f;
    float     m_far_plane         = 1000.0f;
    float     m_fov               = 60.0f;
    glm::mat4 m_view_matrix       = glm::mat4(1.0f);
    glm::mat4 m_projection_matrix = glm::mat4(1.0f);

public:
    CameraNode(const std::string& name);
    ~CameraNode();

    void update(RenderState& render_state) override;

    inline void      set_near_plane(const float& near_plane) { m_near_plane = near_plane; }
    inline void      set_far_plane(const float& far_plane) { m_far_plane = far_plane; }
    inline void      set_fov(const float& fov) { m_fov = fov; }
    inline float     near_plane() { return m_near_plane; }
    inline float     far_plane() { return m_far_plane; }
    inline float     fov() { return m_fov; }
    inline glm::mat4 view_matrix() { return m_view_matrix; }
    inline glm::mat4 projection_matrix() { return m_projection_matrix; }
};

struct IBLNode : public Node
{
public:
    using Ptr = std::shared_ptr<IBLNode>;

private:
    std::shared_ptr<TextureCube> m_image;

public:
    IBLNode(const std::string& name);
    ~IBLNode();

    void update(RenderState& render_state) override;

    inline void                         set_image(std::shared_ptr<TextureCube> image) { m_image = image; }
    inline std::shared_ptr<TextureCube> image() { return m_image; }
};

enum SceneState
{
    SCENE_STATE_READY,
    SCENE_STATE_HIERARCHY_UPDATED,
    SCENE_STATE_TRANSFORMS_UPDATED
};

struct RenderState
{
    std::vector<MeshNode*>             meshes;
    std::vector<DirectionalLightNode*> directional_lights;
    std::vector<SpotLightNode*>        spot_lights;
    std::vector<PointLightNode*>       point_lights;
    CameraNode*                        camera;
    IBLNode*                           ibl_environment_map;
    SceneState                         scene_state = SCENE_STATE_READY;

    RenderState();
    ~RenderState();

    void clear();
};

struct RTGeometryInstance
{
    glm::mat3x4 transform;
    uint32_t    instanceCustomIndex : 24;
    uint32_t    mask : 8;
    uint32_t    instanceOffset : 24;
    uint32_t    flags : 8;
    uint64_t    accelerationStructureHandle;
};

class Scene
{
public:
    using Ptr = std::shared_ptr<Scene>;

    friend class ResourceManager;

public:
    static Scene::Ptr create(vk::Backend::Ptr backend, const std::string& name, Node::Ptr root = nullptr);
    ~Scene();

    void      update(RenderState& render_state);
    void      set_root_node(Node::Ptr node);
    Node::Ptr root_node();

    inline void                       set_name(const std::string& name) { m_name = name; }
    inline std::string                name() { return m_name; }
    inline AccelerationStructureData& acceleration_structure_data() { return m_tlas; }

private:
    Scene(vk::Backend::Ptr backend, const std::string& name, Node::Ptr root = nullptr);
    void create_gpu_resources(RenderState& render_state);

private:
    AccelerationStructureData    m_tlas;
    Node::Ptr                    m_root;
    vk::DescriptorSetLayout::Ptr m_descriptor_set_layout;
    vk::DescriptorSet::Ptr       m_descriptor_set;
    vk::Buffer::Ptr              m_light_data_buffer;
    vk::Buffer::Ptr              m_material_data_buffer;
    vk::Sampler::Ptr             m_material_sampler;
    std::weak_ptr<vk::Backend>   m_backend;
    std::string                  m_name;
};
} // namespace lumen