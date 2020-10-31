#include <material.h>

namespace lumen
{
// -----------------------------------------------------------------------------------------------------------------------------------

static uint32_t g_last_material_id = 0;

// -----------------------------------------------------------------------------------------------------------------------------------

Material::Ptr Material::create(MaterialType             type,
                               std::shared_ptr<Texture> albedo_texture,
                               std::shared_ptr<Texture> normal_texture,
                               std::shared_ptr<Texture> metallic_texture,
                               std::shared_ptr<Texture> roughness_texture,
                               std::shared_ptr<Texture> emissive_texture,
                               glm::vec4                albedo_value,
                               glm::vec4                emissive_value,
                               float                    metallic_value,
                               float                    roughness_value,
                               bool                     orca)
{
    return std::shared_ptr<Material>(new Material(type, albedo_texture, normal_texture, metallic_texture, roughness_texture, emissive_texture, albedo_value, emissive_value, metallic_value, roughness_value, orca));
}

// -----------------------------------------------------------------------------------------------------------------------------------

Material::Material(MaterialType             type,
                   std::shared_ptr<Texture> albedo_texture,
                   std::shared_ptr<Texture> normal_texture,
                   std::shared_ptr<Texture> metallic_texture,
                   std::shared_ptr<Texture> roughness_texture,
                   std::shared_ptr<Texture> emissive_texture,
                   glm::vec4                albedo_value,
                   glm::vec4                emissive_value,
                   float                    metallic_value,
                   float                    roughness_value,
                   bool                     orca) :
    m_type(type),
    m_albedo_texture(albedo_texture),
    m_normal_texture(normal_texture),
    m_metallic_texture(metallic_texture),
    m_roughness_texture(roughness_texture),
    m_emissive_texture(emissive_texture),
    m_albedo_value(albedo_value),
    m_emissive_value(emissive_value),
    m_metallic_value(metallic_value),
    m_roughness_value(roughness_value),
    m_orca(orca),
    m_id(g_last_material_id++)
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

Material::~Material()
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

bool Material::is_emissive()
{
    if (m_emissive_texture)
        return true;
    else
        return m_emissive_value.x > 0.0f || m_emissive_value.y > 0.0f || m_emissive_value.z > 0.0f;
}

// -----------------------------------------------------------------------------------------------------------------------------------
} // namespace lumen