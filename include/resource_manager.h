#pragma once

#include <unordered_map>
#include <texture.h>
#include <material.h>
#include <mesh.h>
#include <scene.h>
#include <vk.h>

namespace lumen
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
    Texture2D::Ptr   load_texture_2d_internal(const std::string& path, bool srgb, bool absolute, vk::BatchUploader& uploader);
    TextureCube::Ptr load_texture_cube_internal(const std::string& path, bool srgb, bool absolute, vk::BatchUploader& uploader);
    Material::Ptr    load_material_internal(const std::string& path, bool absolute, vk::BatchUploader& uploader);
    Mesh::Ptr        load_mesh_internal(const std::string& path, bool absolute, vk::BatchUploader& uploader);
};
} // namespace lumen