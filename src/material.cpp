#include <material.h>

namespace lumen
{
// -----------------------------------------------------------------------------------------------------------------------------------

Material::Material(MaterialType             type,
                   std::shared_ptr<Texture> albedo_texture,
                   std::shared_ptr<Texture> normal_texture,
                   std::shared_ptr<Texture> metallic_texture,
                   std::shared_ptr<Texture> roughness_texture,
                   glm::vec4                albedo_value,
                   float                    metallic_value,
                   float                    roughness_value,
                   bool                     orca) :
    m_type(type),
    m_albedo_texture(albedo_texture),
    m_normal_texture(normal_texture),
    m_metallic_texture(metallic_texture),
    m_roughness_texture(roughness_texture),
    m_albedo_value(albedo_value),
    m_metallic_value(metallic_value),
    m_roughness_value(roughness_value),
    m_orca(orca)
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

Material::~Material()
{
}

// -----------------------------------------------------------------------------------------------------------------------------------
} // namespace lumen