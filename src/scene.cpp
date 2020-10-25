#include <scene.h>

namespace lumen
{
// -----------------------------------------------------------------------------------------------------------------------------------

Node::Node(const NodeType& type, const std::string& name)
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

Node::~Node()
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Node::add_child(Node::Ptr child)
{
    m_is_heirarchy_out_of_date = true;
    child->m_parent            = this;
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
    m_is_heirarchy_out_of_date = true;
    int child_to_remove        = -1;

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

void Node::update_children(RenderState& render_state)
{
    if (m_is_heirarchy_out_of_date)
    {
        render_state.acceleration_structure_state = ACCELERATION_STRUCTURE_REQUIRES_REBUILD;
        m_is_heirarchy_out_of_date                = false;
    }

    for (auto& child : m_children)
        child->update(render_state);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Node::mark_as_dirty()
{
    m_is_dirty = true;

    for (auto& child : m_children)
        child->mark_as_dirty();
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
    if (m_is_dirty)
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

        if (render_state.acceleration_structure_state != ACCELERATION_STRUCTURE_REQUIRES_REBUILD)
            render_state.acceleration_structure_state = ACCELERATION_STRUCTURE_REQUIRES_UPDATE;

        m_is_dirty = false;
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

bool TransformNode::is_dirty()
{
    return m_is_dirty;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void TransformNode::set_orientation_from_euler_yxz(const glm::vec3& e)
{
    glm::quat pitch = glm::quat(glm::vec3(glm::radians(e.x), glm::radians(0.0f), glm::radians(0.0f)));
    glm::quat yaw   = glm::quat(glm::vec3(glm::radians(0.0f), glm::radians(e.y), glm::radians(0.0f)));
    glm::quat roll  = glm::quat(glm::vec3(glm::radians(0.0f), glm::radians(0.0f), glm::radians(e.z)));

    m_orientation = yaw * pitch * roll;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void TransformNode::set_orientation_from_euler_xyz(const glm::vec3& e)
{
    mark_as_dirty();

    glm::quat pitch = glm::quat(glm::vec3(glm::radians(e.x), glm::radians(0.0f), glm::radians(0.0f)));
    glm::quat yaw   = glm::quat(glm::vec3(glm::radians(0.0f), glm::radians(e.y), glm::radians(0.0f)));
    glm::quat roll  = glm::quat(glm::vec3(glm::radians(0.0f), glm::radians(0.0f), glm::radians(e.z)));

    m_orientation = pitch * yaw * roll;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void TransformNode::set_position(const glm::vec3& position)
{
    mark_as_dirty();

    m_position = position;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void TransformNode::rotate_euler_yxz(const glm::vec3& e)
{
    mark_as_dirty();

    glm::quat pitch = glm::quat(glm::vec3(glm::radians(e.x), glm::radians(0.0f), glm::radians(0.0f)));
    glm::quat yaw   = glm::quat(glm::vec3(glm::radians(0.0f), glm::radians(e.y), glm::radians(0.0f)));
    glm::quat roll  = glm::quat(glm::vec3(glm::radians(0.0f), glm::radians(0.0f), glm::radians(e.z)));

    glm::quat delta = yaw * pitch * roll;
    m_orientation   = m_orientation * delta;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void TransformNode::rotate_euler_xyz(const glm::vec3& e)
{
    mark_as_dirty();

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

        render_state.meshes.push_back(this);

        update_children(render_state);
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

        render_state.directional_lights.push_back(this);

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

        render_state.spot_lights.push_back(this);

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

        render_state.point_lights.push_back(this);

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

        if (!render_state.camera)
            render_state.camera = this;

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
        if (!render_state.ibl_environment_map)
            render_state.ibl_environment_map = this;

        update_children(render_state);
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

RenderState::RenderState()
{
    meshes.reserve(100000);
    directional_lights.reserve(100000);
    spot_lights.reserve(100000);
    point_lights.reserve(100000);

    clear();
}

// -----------------------------------------------------------------------------------------------------------------------------------

RenderState::~RenderState()
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

void RenderState::clear()
{
    meshes.clear();
    directional_lights.clear();
    spot_lights.clear();
    point_lights.clear();

    camera                       = nullptr;
    ibl_environment_map          = nullptr;
    acceleration_structure_state = ACCELERATION_STRUCTURE_READY;
}

// -----------------------------------------------------------------------------------------------------------------------------------

Scene::Scene(vk::Backend::Ptr backend, Node::Ptr root) :
    m_backend(backend), m_root(root)
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

Scene::~Scene()
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Scene::update(RenderState& render_state)
{
    m_root->update(render_state);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Scene::set_root_node(Node::Ptr node)
{
    m_root = node;
}

// -----------------------------------------------------------------------------------------------------------------------------------

Node::Ptr Scene::root_node()
{
    return m_root;
}

// -----------------------------------------------------------------------------------------------------------------------------------
} // namespace lumen