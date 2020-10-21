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

    struct Node
    {
        using Ptr = std::shared_ptr<Node>;

        bool                               enabled = true;
        std::string                        name;
        Node*                              parent = nullptr;
        std::vector<std::shared_ptr<Node>> children;

        virtual void update() = 0;
        void         add_child(Node::Ptr child);
        Node::Ptr    find_child(const std::string& name);
        void         remove_child(const std::string& name);
    };

    struct TransformNode : public Node
    {
        using Ptr = std::shared_ptr<TransformNode>;

        glm::vec3 position = glm::vec3(0.0f);
        glm::quat orientation = glm::quat(glm::radians(glm::vec3(0.0f)));
        glm::vec3 scale = glm::vec3(1.0f);
        glm::mat4 prev_model_matrix = glm::mat4(1.0f);
        glm::mat4 model_matrix = glm::mat4(1.0f);
        glm::mat4 model_matrix_without_scale = glm::mat4(1.0f);

        void update() override;

        glm::vec3 forward();
        glm::vec3 up();
        glm::vec3 left();
        void      set_orientation_from_euler_yxz(const glm::vec3& e);
        void      set_orientation_from_euler_xyz(const glm::vec3& e);
        void      rotate_euler_yxz(const glm::vec3& e);
        void      rotate_euler_xyz(const glm::vec3& e);
    };

    struct MeshNode : public TransformNode
    {
        using Ptr = std::shared_ptr<MeshNode>;

        std::string mesh_path;
        std::string material_override_path;
        std::shared_ptr<Mesh> mesh;

        void update() override;
    };

    struct DirectionalLightNode : public TransformNode
    {
        using Ptr = std::shared_ptr<TransformNode>;

        glm::vec3 color;
        float     intensity;

        void update() override;
    };

    struct SpotLightNode : public TransformNode
    {
        using Ptr = std::shared_ptr<SpotLightNode>;

        glm::vec3 color;
        float     cone_angle;
        float     range;
        float     intensity;

        void update() override;
    };

    struct PointLightNode : public TransformNode
    {
        using Ptr = std::shared_ptr<PointLightNode>;

        glm::vec3 color;
        float     range;
        float     intensity;

        void update() override;
    };

    struct CameraNode : public TransformNode
    {
        using Ptr = std::shared_ptr<CameraNode>;

        float near_plane = 1.0f;
        float far_plane = 1000.0f;
        float fov = 60.0f;
        glm::mat4 view_matrix = glm::mat4(1.0f);
        glm::mat4 projection_matrix = glm::mat4(1.0f);

        void update() override;
    };

    struct IBLNode : public Node
    {
        using Ptr = std::shared_ptr<IBLNode>;

        std::string image_path;
        std::shared_ptr<TextureCube> image;
    
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