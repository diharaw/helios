#include <resource/material.h>

namespace helios
{
// -----------------------------------------------------------------------------------------------------------------------------------

static uint32_t g_last_material_id = 0;

// -----------------------------------------------------------------------------------------------------------------------------------

Material::Ptr Material::create(vk::Backend::Ptr                        backend,
                               MaterialType                            type,
                               std::vector<std::shared_ptr<Texture2D>> textures,
                               TextureInfo                             albedo_texture_info,
                               TextureInfo                             normal_texture_info,
                               TextureInfo                             metallic_texture_info,
                               TextureInfo                             roughness_texture_info,
                               TextureInfo                             emissive_texture_info,
                               glm::vec4                               albedo_value,
                               glm::vec4                               emissive_value,
                               float                                   metallic_value,
                               float                                   roughness_value,
                               bool                                    alpha_test,
                               const std::string& path)
{
    return std::shared_ptr<Material>(new Material(backend, type, textures, albedo_texture_info, normal_texture_info, metallic_texture_info, roughness_texture_info, emissive_texture_info, albedo_value, emissive_value, metallic_value, roughness_value, alpha_test, path));
}

// -----------------------------------------------------------------------------------------------------------------------------------

Material::Material(vk::Backend::Ptr                        backend,
                   MaterialType                            type,
                   std::vector<std::shared_ptr<Texture2D>> textures,
                   TextureInfo                             albedo_texture_info,
                   TextureInfo                             normal_texture_info,
                   TextureInfo                             metallic_texture_info,
                   TextureInfo                             roughness_texture_info,
                   TextureInfo                             emissive_texture_info,
                   glm::vec4                               albedo_value,
                   glm::vec4                               emissive_value,
                   float                                   metallic_value,
                   float                                   roughness_value,
                   bool                                    alpha_test,
                   const std::string&                      path) :
    vk::Object(backend),
    m_type(type),
    m_textures(textures),
    m_albedo_texture_info(albedo_texture_info),
    m_normal_texture_info(normal_texture_info),
    m_metallic_texture_info(metallic_texture_info),
    m_roughness_texture_info(roughness_texture_info),
    m_emissive_texture_info(emissive_texture_info),
    m_albedo_value(albedo_value),
    m_emissive_value(emissive_value),
    m_metallic_value(metallic_value),
    m_roughness_value(roughness_value),
    m_alpha_test(alpha_test),
    m_id(g_last_material_id++),
    m_path(path)
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

Material::~Material()
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

bool Material::is_emissive()
{
    if (m_emissive_texture_info.array_index != -1)
        return true;
    else
        return m_emissive_value.x > 0.0f || m_emissive_value.y > 0.0f || m_emissive_value.z > 0.0f;
}

// -----------------------------------------------------------------------------------------------------------------------------------
} // namespace helios