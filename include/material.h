#pragma once

#include <glm.hpp>
#include <memory>

namespace lumen
{
class Texture;

enum MaterialType
{
    MATERIAL_OPAQUE,
    MATERIAL_TRANSPARENT
};

class Material
{
public:
    using Ptr = std::shared_ptr<Material>;

    friend class ResourceManager;

private:
    MaterialType             m_type = MATERIAL_OPAQUE;
    std::shared_ptr<Texture> m_albedo_texture;
    std::shared_ptr<Texture> m_normal_texture;
    std::shared_ptr<Texture> m_metallic_texture;
    std::shared_ptr<Texture> m_roughness_texture;
    glm::vec4                m_albedo_value    = glm::vec4(0.0f);
    float                    m_metallic_value  = 0.0f;
    float                    m_roughness_value = 0.0f;
    bool                     m_orca            = false;

public:
    Material(MaterialType             type,
             std::shared_ptr<Texture> albedo_texture,
             std::shared_ptr<Texture> normal_texture,
             std::shared_ptr<Texture> metallic_texture,
             std::shared_ptr<Texture> roughness_texture,
             glm::vec4                albedo_value    = glm::vec4(0.0f),
             float                    metallic_value  = 0.0f,
             float                    roughness_value = 0.0f,
             bool                     orca = false);
    ~Material();
};
} // namespace lumen