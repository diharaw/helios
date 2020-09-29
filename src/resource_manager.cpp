#include <resource_manager.h>
#include <loader/loader.h>

namespace lumen
{
// -----------------------------------------------------------------------------------------------------------------------------------

ResourceManager::ResourceManager(vk::Backend::Ptr backend) :
    m_backend(backend)
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

ResourceManager::~ResourceManager()
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

Texture::Ptr ResourceManager::load_texture(const std::string& path)
{
    if (!m_backend.expired())
    {
        vk::BatchUploader uploader(m_backend.lock());

        auto resource = load_texture_internal(path, uploader);

        uploader.submit();

        return resource;
    }
    else
        return nullptr;
}

// -----------------------------------------------------------------------------------------------------------------------------------

Material::Ptr ResourceManager::load_material(const std::string& path)
{
    if (!m_backend.expired())
    {
        vk::BatchUploader uploader(m_backend.lock());

        auto resource = load_material_internal(path, uploader);

        uploader.submit();

        return resource;
    }
    else
        return nullptr;
}

// -----------------------------------------------------------------------------------------------------------------------------------

Mesh::Ptr ResourceManager::load_mesh(const std::string& path)
{
    if (!m_backend.expired())
    {
        vk::BatchUploader uploader(m_backend.lock());

        auto resource = load_mesh_internal(path, uploader);

        uploader.submit();

        return resource;
    }
    else
        return nullptr;
}

// -----------------------------------------------------------------------------------------------------------------------------------

Scene::Ptr ResourceManager::load_scene(const std::string& path)
{
    if (!m_backend.expired())
    {
        vk::BatchUploader uploader(m_backend.lock());
    }
    else
        return nullptr;
}

// -----------------------------------------------------------------------------------------------------------------------------------

Texture::Ptr ResourceManager::load_texture_internal(const std::string& path, vk::BatchUploader& uploader)
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

Material::Ptr ResourceManager::load_material_internal(const std::string& path, vk::BatchUploader& uploader)
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

Mesh::Ptr ResourceManager::load_mesh_internal(const std::string& path, vk::BatchUploader& uploader)
{
}

// -----------------------------------------------------------------------------------------------------------------------------------
} // namespace lumen