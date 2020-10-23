#pragma once

#include <vk.h>
#include <glm.hpp>
#include <gtc/quaternion.hpp>
#include <memory>

namespace lumen
{
class Mesh;
class TextureCube;

class Scene
{
public:
    using Ptr = std::shared_ptr<Scene>;

    friend class ResourceManager;

    enum NodeType
    {
        NODE_MESH,
        NODE_CAMERA,
        NODE_DIRECTIONAL_LIGHT,
        NODE_SPOT_LIGHT,
        NODE_POINT_LIGHT,
        NODE_IBL
    };

    class Node
    {
    public:
        using Ptr = std::shared_ptr<Node>;

    private:
        NodeType                           m_type;
        bool                               m_enabled = true;
        std::string                        m_name;
        Node*                              m_parent = nullptr;
        std::vector<std::shared_ptr<Node>> m_children;

    public:
        Node(const NodeType& type, const std::string& name);
        ~Node();
        
        virtual void update() = 0;
        void         add_child(Node::Ptr child);
        Node::Ptr    find_child(const std::string& name);
        void         remove_child(const std::string& name);
    };

    class TransformNode : public Node
    {
    public:
        using Ptr = std::shared_ptr<TransformNode>;

    private:
        bool      m_is_dirty                   = true;
        glm::vec3 m_position = glm::vec3(0.0f);
        glm::quat m_orientation = glm::quat(glm::radians(glm::vec3(0.0f)));
        glm::vec3 m_scale = glm::vec3(1.0f);
        glm::mat4 m_prev_model_matrix = glm::mat4(1.0f);
        glm::mat4 m_model_matrix = glm::mat4(1.0f);
        glm::mat4 m_model_matrix_without_scale = glm::mat4(1.0f);

    public:
        TransformNode(const NodeType& type, const std::string& name);
        ~TransformNode();

        void update() override;

        glm::vec3 forward();
        glm::vec3 up();
        glm::vec3 left();
        void      set_orientation_from_euler_yxz(const glm::vec3& e);
        void      set_orientation_from_euler_xyz(const glm::vec3& e);
        void      rotate_euler_yxz(const glm::vec3& e);
        void      rotate_euler_xyz(const glm::vec3& e);
    };

    class MeshNode : public TransformNode
    {
    public:
        using Ptr = std::shared_ptr<MeshNode>;

    private:
        std::string m_mesh_path;
        std::string m_material_override_path;
        std::shared_ptr<Mesh> m_mesh;

    public:
        MeshNode(const std::string& name);
        ~MeshNode();

        void update() override;
    };

    class DirectionalLightNode : public TransformNode
    {
    public:
        using Ptr = std::shared_ptr<TransformNode>;

    private:
        glm::vec3 color;
        float     intensity;

    public:
        DirectionalLightNode(const std::string& name);
        ~DirectionalLightNode();

        void update() override;
    };

    class SpotLightNode : public TransformNode
    {
    public:
        using Ptr = std::shared_ptr<SpotLightNode>;

        glm::vec3 color;
        float     cone_angle;
        float     range;
        float     intensity;

    public:
        SpotLightNode(const std::string& name);
        ~SpotLightNode();

        void update() override;
    };

    class PointLightNode : public TransformNode
    {
    public:
        using Ptr = std::shared_ptr<PointLightNode>;

    private:
        glm::vec3 color;
        float     range;
        float     intensity;

    public:
        PointLightNode(const std::string& name);
        ~PointLightNode();

        void update() override;
    };

    class CameraNode : public TransformNode
    {
    public:
        using Ptr = std::shared_ptr<CameraNode>;

    private:
        float near_plane = 1.0f;
        float far_plane = 1000.0f;
        float fov = 60.0f;
        glm::mat4 view_matrix = glm::mat4(1.0f);
        glm::mat4 projection_matrix = glm::mat4(1.0f);

    public:
        CameraNode(const std::string& name);
        ~CameraNode();

        void update() override;
    };

    struct IBLNode : public Node
    {
    public:
        using Ptr = std::shared_ptr<IBLNode>;

    private:
        std::string image_path;
        std::shared_ptr<TextureCube> image;
    
    public:
        IBLNode(const std::string& name);
        ~IBLNode();

        void update() override;
    };

public:
    Scene(vk::Backend::Ptr backend, Node::Ptr root);
    ~Scene();

    void update();

private:
    vk::AccelerationStructure::Ptr m_tlas;
    vk::DescriptorSet::Ptr         m_ds;
    Node::Ptr                      m_root;
};
} // namespace lumen