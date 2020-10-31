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
    std::shared_ptr<Texture> m_emissive_texture;
    glm::vec4                m_albedo_value    = glm::vec4(0.0f);
    glm::vec4                m_emissive_value  = glm::vec4(0.0f);
    float                    m_metallic_value  = 0.0f;
    float                    m_roughness_value = 0.0f;
    bool                     m_orca            = false;
    uint32_t                 m_id;

public:
    static Material::Ptr create(MaterialType             type,
                                std::shared_ptr<Texture> albedo_texture,
                                std::shared_ptr<Texture> normal_texture,
                                std::shared_ptr<Texture> metallic_texture,
                                std::shared_ptr<Texture> roughness_texture,
                                std::shared_ptr<Texture> emissive_texture,
                                glm::vec4                albedo_value    = glm::vec4(0.0f),
                                glm::vec4                emissive_value  = glm::vec4(0.0f),
                                float                    metallic_value  = 0.0f,
                                float                    roughness_value = 0.0f,
                                bool                     orca            = false);
    ~Material();

    bool                            is_emissive();
    inline MaterialType             type() { return m_type; }
    inline std::shared_ptr<Texture> albedo_texture() { return m_albedo_texture; }
    inline std::shared_ptr<Texture> normal_texture() { return m_normal_texture; }
    inline std::shared_ptr<Texture> metallic_texture() { return m_metallic_texture; }
    inline std::shared_ptr<Texture> roughness_texture() { return m_roughness_texture; }
    inline std::shared_ptr<Texture> emissive_texture() { return m_emissive_texture; }
    inline glm::vec4                albedo_value() { return m_albedo_value; }
    inline glm::vec4                emissive_value() { return m_emissive_value; }
    inline float                    metallic_value() { return m_metallic_value; }
    inline float                    roughness_value() { return m_roughness_value; }
    inline bool                     is_orca() { return m_orca; }
    inline uint32_t                 id() { return m_id; }

private:
    Material(MaterialType             type,
             std::shared_ptr<Texture> albedo_texture,
             std::shared_ptr<Texture> normal_texture,
             std::shared_ptr<Texture> metallic_texture,
             std::shared_ptr<Texture> roughness_texture,
             std::shared_ptr<Texture> emissive_texture,
             glm::vec4                albedo_value    = glm::vec4(0.0f),
             glm::vec4                emissive_value  = glm::vec4(0.0f),
             float                    metallic_value  = 0.0f,
             float                    roughness_value = 0.0f,
             bool                     orca            = false);
};
} // namespace lumen