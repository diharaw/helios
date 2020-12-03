#pragma once

#include <unordered_map>
#include <resource/texture.h>
#include <resource/material.h>
#include <resource/mesh.h>
#include <resource/scene.h>
#include <core/vk.h>
#include <common/scene.h>

namespace helios
{
class ResourceManager
{
private:
    std::weak_ptr<vk::Backend>                        m_backend;
    std::unordered_map<std::string, Texture2D::Ptr>   m_textures_2d;
    std::unordered_map<std::string, TextureCube::Ptr> m_textures_cube;
    std::unordered_map<std::string, Material::Ptr>    m_materials;
    std::unordered_map<std::string, Mesh::Ptr>        m_meshes;

public:
    ResourceManager(vk::Backend::Ptr backend);
    ~ResourceManager();

    Texture2D::Ptr   load_texture_2d(const std::string& path, bool srgb = false, bool absolute = false);
    TextureCube::Ptr load_texture_cube(const std::string& path, bool srgb = false, bool absolute = false);
    Material::Ptr    load_material(const std::string& path, bool absolute = false);
    Mesh::Ptr        load_mesh(const std::string& path, bool absolute = false);
    Scene::Ptr       load_scene(const std::string& path, bool absolute = false);

private:
    Texture2D::Ptr            load_texture_2d_internal(const std::string& path, bool srgb, bool absolute, vk::BatchUploader& uploader);
    TextureCube::Ptr          load_texture_cube_internal(const std::string& path, bool srgb, bool absolute, vk::BatchUploader& uploader);
    Material::Ptr             load_material_internal(const std::string& path, bool absolute, vk::BatchUploader& uploader);
    Mesh::Ptr                 load_mesh_internal(const std::string& path, bool absolute, vk::BatchUploader& uploader);
    Node::Ptr                 create_node(std::shared_ptr<ast::SceneNode> ast_node, vk::BatchUploader& uploader);
    MeshNode::Ptr             create_mesh_node(std::shared_ptr<ast::MeshNode> ast_node, vk::BatchUploader& uploader);
    CameraNode::Ptr           create_camera_node(std::shared_ptr<ast::CameraNode> ast_node, vk::BatchUploader& uploader);
    DirectionalLightNode::Ptr create_directional_light_node(std::shared_ptr<ast::DirectionalLightNode> ast_node, vk::BatchUploader& uploader);
    SpotLightNode::Ptr        create_spot_light_node(std::shared_ptr<ast::SpotLightNode> ast_node, vk::BatchUploader& uploader);
    PointLightNode::Ptr       create_point_light_node(std::shared_ptr<ast::PointLightNode> ast_node, vk::BatchUploader& uploader);
    IBLNode::Ptr              create_ibl_node(std::shared_ptr<ast::IBLNode> ast_node, vk::BatchUploader& uploader);
    void                      populate_scene_node(Node::Ptr node, std::shared_ptr<ast::SceneNode> ast_node, vk::BatchUploader& uploader);
    void                      populate_transform_node(TransformNode::Ptr node, std::shared_ptr<ast::TransformNode> ast_node);
};
} // namespace helios