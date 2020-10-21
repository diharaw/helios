#include <scene.h>

namespace lumen
{
// -----------------------------------------------------------------------------------------------------------------------------------

void Scene::Node::add_child(Node::Ptr child)
{
    child->parent = this;
    children.push_back(child);
}

// -----------------------------------------------------------------------------------------------------------------------------------

Scene::Node::Ptr Scene::Node::find_child(const std::string& name)
{
    for (auto child : children)
    {
        if (child->name == name)
            return child;
    }

    return nullptr;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Scene::Node::remove_child(const std::string& name)
{
    int child_to_remove = -1;

    for (int i = 0; i < children.size(); i++)
    {
        if (children[i]->name == name)
        {
            child_to_remove = i;
            break;
        }
    }

    children.erase(children.begin() + child_to_remove);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Scene::TransformNode::update()
{
    glm::mat4 R = glm::mat4_cast(orientation);
    glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);
    glm::mat4 T = glm::translate(glm::mat4(1.0f), position);

    prev_model_matrix          = model_matrix;
    model_matrix_without_scale = T * R;
    model_matrix               = model_matrix_without_scale * S;

    TransformNode* parent_transform = dynamic_cast<TransformNode*>(parent);

    if (parent_transform)
        model_matrix = model_matrix * parent_transform->model_matrix;

    for (auto& child : children)
        child->update();
}

// -----------------------------------------------------------------------------------------------------------------------------------

glm::vec3 Scene::TransformNode::forward()
{
    return orientation * glm::vec3(0.0f, 0.0f, 1.0f);
}

// -----------------------------------------------------------------------------------------------------------------------------------

inline glm::vec3 Scene::TransformNode::up()
{
    return orientation * glm::vec3(0.0f, 1.0f, 0.0f);
}

// -----------------------------------------------------------------------------------------------------------------------------------

inline glm::vec3 Scene::TransformNode::left()
{
    return orientation * glm::vec3(1.0f, 0.0f, 0.0f);
}

// -----------------------------------------------------------------------------------------------------------------------------------

inline void Scene::TransformNode::set_orientation_from_euler_yxz(const glm::vec3& e)
{
    glm::quat pitch = glm::quat(glm::vec3(glm::radians(e.x), glm::radians(0.0f), glm::radians(0.0f)));
    glm::quat yaw   = glm::quat(glm::vec3(glm::radians(0.0f), glm::radians(e.y), glm::radians(0.0f)));
    glm::quat roll  = glm::quat(glm::vec3(glm::radians(0.0f), glm::radians(0.0f), glm::radians(e.z)));

    orientation = yaw * pitch * roll;
}

// -----------------------------------------------------------------------------------------------------------------------------------

inline void Scene::TransformNode::set_orientation_from_euler_xyz(const glm::vec3& e)
{
    glm::quat pitch = glm::quat(glm::vec3(glm::radians(e.x), glm::radians(0.0f), glm::radians(0.0f)));
    glm::quat yaw   = glm::quat(glm::vec3(glm::radians(0.0f), glm::radians(e.y), glm::radians(0.0f)));
    glm::quat roll  = glm::quat(glm::vec3(glm::radians(0.0f), glm::radians(0.0f), glm::radians(e.z)));

    orientation = pitch * yaw * roll;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Scene::TransformNode::rotate_euler_yxz(const glm::vec3& e)
{
    glm::quat pitch = glm::quat(glm::vec3(glm::radians(e.x), glm::radians(0.0f), glm::radians(0.0f)));
    glm::quat yaw   = glm::quat(glm::vec3(glm::radians(0.0f), glm::radians(e.y), glm::radians(0.0f)));
    glm::quat roll  = glm::quat(glm::vec3(glm::radians(0.0f), glm::radians(0.0f), glm::radians(e.z)));

    glm::quat delta = yaw * pitch * roll;
    orientation     = orientation * delta;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Scene::TransformNode::rotate_euler_xyz(const glm::vec3& e)
{
    glm::quat pitch = glm::quat(glm::vec3(glm::radians(e.x), glm::radians(0.0f), glm::radians(0.0f)));
    glm::quat yaw   = glm::quat(glm::vec3(glm::radians(0.0f), glm::radians(e.y), glm::radians(0.0f)));
    glm::quat roll  = glm::quat(glm::vec3(glm::radians(0.0f), glm::radians(0.0f), glm::radians(e.z)));

    glm::quat delta = pitch * yaw * roll;
    orientation     = orientation * delta;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Scene::MeshNode::update()
{
    TransformNode::update();
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Scene::DirectionalLightNode::update()
{
    TransformNode::update();
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Scene::SpotLightNode::update()
{
    TransformNode::update();
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Scene::PointLightNode::update()
{
    TransformNode::update();
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Scene::CameraNode::update()
{
    TransformNode::update();

    projection_matrix = glm::perspective(glm::radians(fov), 1.0f, near_plane, far_plane);
    view_matrix       = glm::inverse(model_matrix_without_scale);
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