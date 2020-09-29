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
    std::weak_ptr<vk::Backend>                     m_backend;
    std::unordered_map<std::string, Texture::Ptr>  m_textures;
    std::unordered_map<std::string, Material::Ptr> m_materials;
    std::unordered_map<std::string, Mesh::Ptr>     m_meshes;

public:
    ResourceManager(vk::Backend::Ptr backend);
    ~ResourceManager();

    Texture::Ptr  load_texture(const std::string& path);
    Material::Ptr load_material(const std::string& path);
    Mesh::Ptr     load_mesh(const std::string& path);
    Scene::Ptr    load_scene(const std::string& path);

private:
    Texture::Ptr  load_texture_internal(const std::string& path, vk::BatchUploader& uploader);
    Material::Ptr load_material_internal(const std::string& path, vk::BatchUploader& uploader);
    Mesh::Ptr     load_mesh_internal(const std::string& path, vk::BatchUploader& uploader);
};
} // namespace lumen