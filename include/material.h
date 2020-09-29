#pragma once

#include <glm.hpp>
#include <memory>

namespace lumen
{
class Texture;

class Material
{
public:
    using Ptr = std::shared_ptr<Material>;

    friend class ResourceManager;

private:
    std::shared_ptr<Texture> m_albedo_texture;
    std::shared_ptr<Texture> m_normal_texture;
    std::shared_ptr<Texture> m_metallic_specular_texture;
    std::shared_ptr<Texture> m_roughness_glossiness_texture;
    std::shared_ptr<Texture> m_displacement_texture;
    bool                     m_transparent                = false;
    glm::vec4                m_albedo_value               = glm::vec4(0.0f);
    float                    m_metallic_specular_value    = 0.0f;
    float                    m_roughness_glossiness_value = 0.0f;

public:
    Material(std::shared_ptr<Texture> albedo_texture,
             std::shared_ptr<Texture> normal_texture,
             std::shared_ptr<Texture> metallic_specular_texture,
             std::shared_ptr<Texture> roughness_glossiness_texture,
             std::shared_ptr<Texture> displacement_texture,
             bool                     transparent                = false,
             glm::vec4                albedo_value               = glm::vec4(0.0f),
             float                    metallic_specular_value    = 0.0f,
             float                    roughness_glossiness_value = 0.0f);
    ~Material();
};
} // namespace lumen