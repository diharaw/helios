#include <scene.h>

namespace lumen
{
// -----------------------------------------------------------------------------------------------------------------------------------

Scene::Node::Node(const Scene::NodeType& type, const std::string& name)
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

Scene::Node::~Node()
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Scene::Node::add_child(Node::Ptr child)
{
    child->m_parent = this;
    m_children.push_back(child);
}

// -----------------------------------------------------------------------------------------------------------------------------------

Scene::Node::Ptr Scene::Node::find_child(const std::string& name)
{
    for (auto child : m_children)
    {
        if (child->m_name == name)
            return child;
    }

    return nullptr;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Scene::Node::remove_child(const std::string& name)
{
    int child_to_remove = -1;

    for (int i = 0; i < m_children.size(); i++)
    {
        if (m_children[i]->m_name == name)
        {
            child_to_remove = i;
            break;
        }
    }

    if (child_to_remove != -1)
        m_children.erase(m_children.begin() + child_to_remove);
}

// -----------------------------------------------------------------------------------------------------------------------------------

Scene::TransformNode::TransformNode(const Scene::NodeType& type, const std::string& name) :
    Node(type, name)
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

Scene::TransformNode::~TransformNode()
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Scene::TransformNode::update()
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

    for (auto& child : m_children)
        child->update();
}

// -----------------------------------------------------------------------------------------------------------------------------------

glm::vec3 Scene::TransformNode::forward()
{
    return m_orientation * glm::vec3(0.0f, 0.0f, 1.0f);
}

// -----------------------------------------------------------------------------------------------------------------------------------

glm::vec3 Scene::TransformNode::up()
{
    return m_orientation * glm::vec3(0.0f, 1.0f, 0.0f);
}

// -----------------------------------------------------------------------------------------------------------------------------------

glm::vec3 Scene::TransformNode::left()
{
    return m_orientation * glm::vec3(1.0f, 0.0f, 0.0f);
}

// -----------------------------------------------------------------------------------------------------------------------------------

glm::vec3 Scene::TransformNode::position()
{
    return m_position;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Scene::TransformNode::set_orientation_from_euler_yxz(const glm::vec3& e)
{
    glm::quat pitch = glm::quat(glm::vec3(glm::radians(e.x), glm::radians(0.0f), glm::radians(0.0f)));
    glm::quat yaw   = glm::quat(glm::vec3(glm::radians(0.0f), glm::radians(e.y), glm::radians(0.0f)));
    glm::quat roll  = glm::quat(glm::vec3(glm::radians(0.0f), glm::radians(0.0f), glm::radians(e.z)));

    m_orientation = yaw * pitch * roll;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Scene::TransformNode::set_orientation_from_euler_xyz(const glm::vec3& e)
{
    glm::quat pitch = glm::quat(glm::vec3(glm::radians(e.x), glm::radians(0.0f), glm::radians(0.0f)));
    glm::quat yaw   = glm::quat(glm::vec3(glm::radians(0.0f), glm::radians(e.y), glm::radians(0.0f)));
    glm::quat roll  = glm::quat(glm::vec3(glm::radians(0.0f), glm::radians(0.0f), glm::radians(e.z)));

    m_orientation = pitch * yaw * roll;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Scene::TransformNode::set_position(const glm::vec3& position)
{
    m_position = position;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Scene::TransformNode::rotate_euler_yxz(const glm::vec3& e)
{
    glm::quat pitch = glm::quat(glm::vec3(glm::radians(e.x), glm::radians(0.0f), glm::radians(0.0f)));
    glm::quat yaw   = glm::quat(glm::vec3(glm::radians(0.0f), glm::radians(e.y), glm::radians(0.0f)));
    glm::quat roll  = glm::quat(glm::vec3(glm::radians(0.0f), glm::radians(0.0f), glm::radians(e.z)));

    glm::quat delta = yaw * pitch * roll;
    m_orientation   = m_orientation * delta;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Scene::TransformNode::rotate_euler_xyz(const glm::vec3& e)
{
    glm::quat pitch = glm::quat(glm::vec3(glm::radians(e.x), glm::radians(0.0f), glm::radians(0.0f)));
    glm::quat yaw   = glm::quat(glm::vec3(glm::radians(0.0f), glm::radians(e.y), glm::radians(0.0f)));
    glm::quat roll  = glm::quat(glm::vec3(glm::radians(0.0f), glm::radians(0.0f), glm::radians(e.z)));

    glm::quat delta = pitch * yaw * roll;
    m_orientation   = m_orientation * delta;
}

// -----------------------------------------------------------------------------------------------------------------------------------

Scene::MeshNode::MeshNode(const std::string& name) :
    TransformNode(Scene::NODE_MESH, name)
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

Scene::TransformNode::~TransformNode()
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Scene::MeshNode::update()
{
    TransformNode::update();
}

// -----------------------------------------------------------------------------------------------------------------------------------

Scene::DirectionalLightNode::DirectionalLightNode(const std::string& name) :
    TransformNode(Scene::NODE_DIRECTIONAL_LIGHT, name)
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

Scene::DirectionalLightNode::~DirectionalLightNode()
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Scene::DirectionalLightNode::update()
{
    TransformNode::update();
}

// -----------------------------------------------------------------------------------------------------------------------------------

Scene::SpotLightNode::SpotLightNode(const std::string& name) :
    TransformNode(Scene::NODE_SPOT_LIGHT, name)
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

Scene::SpotLightNode::~SpotLightNode()
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Scene::SpotLightNode::update()
{
    TransformNode::update();
}

// -----------------------------------------------------------------------------------------------------------------------------------

Scene::PointLightNode::PointLightNode(const std::string& name) :
    TransformNode(Scene::NODE_POINT_LIGHT, name)
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

Scene::PointLightNode::~PointLightNode()
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Scene::PointLightNode::update()
{
    TransformNode::update();
}

// -----------------------------------------------------------------------------------------------------------------------------------

Scene::CameraNode::CameraNode(const std::string& name) :
    TransformNode(Scene::NODE_CAMERA, name)
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

Scene::CameraNode::~CameraNode()
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Scene::CameraNode::update()
{
    TransformNode::update();

    m_projection_matrix = glm::perspective(glm::radians(m_fov), 1.0f, m_near_plane, m_far_plane);
    m_view_matrix       = glm::inverse(m_model_matrix_without_scale);
}

// -----------------------------------------------------------------------------------------------------------------------------------

Scene::IBLNode::IBLNode(const std::string& name) :
    Node(Scene::NODE_IBL, name)
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

Scene::IBLNode::~IBLNode()
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Scene::IBLNode::update()
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

Scene::Scene(vk::Backend::Ptr backend, Node::Ptr root)
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

Scene::~Scene()
{
}

// -----------------------------------------------------------------------------------------------------------------------------------
} // namespace lumen