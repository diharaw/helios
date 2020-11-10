#pragma once

#include <glm.hpp>
#include <memory>
#include <vector>

namespace lumen
{
class Texture2D;

enum MaterialType
{
    MATERIAL_OPAQUE,
    MATERIAL_TRANSPARENT
};

struct TextureInfo
{
    int32_t array_index   = -1;
    int32_t channel_index = -1;
};

class Material
{
public:
    using Ptr = std::shared_ptr<Material>;

    friend class ResourceManager;

private:
    MaterialType                            m_type = MATERIAL_OPAQUE;
    std::vector<std::shared_ptr<Texture2D>> m_textures;
    TextureInfo                             m_albedo_texture_info;
    TextureInfo                             m_normal_texture_info;
    TextureInfo                             m_metallic_texture_info;
    TextureInfo                             m_roughness_texture_info;
    TextureInfo                             m_emissive_texture_info;
    glm::vec4                               m_albedo_value    = glm::vec4(0.0f);
    glm::vec4                               m_emissive_value  = glm::vec4(0.0f);
    float                                   m_metallic_value  = 0.0f;
    float                                   m_roughness_value = 0.0f;
    bool                                    m_alpha_test      = false;
    uint32_t                                m_id;

public:
    static Material::Ptr create(MaterialType                            type,
                                std::vector<std::shared_ptr<Texture2D>> textures,
                                TextureInfo                             albedo_texture_info,
                                TextureInfo                             normal_texture_info,
                                TextureInfo                             metallic_texture_info,
                                TextureInfo                             roughness_texture_info,
                                TextureInfo                             emissive_texture_info,
                                glm::vec4                               albedo_value    = glm::vec4(0.0f),
                                glm::vec4                               emissive_value  = glm::vec4(0.0f),
                                float                                   metallic_value  = 0.0f,
                                float                                   roughness_value = 0.0f,
                                bool                                    alpha_test      = false);
    ~Material();

    bool                              is_emissive();
    inline bool                       is_alpha_tested() { return m_alpha_test; }
    inline MaterialType               type() { return m_type; }
    inline std::shared_ptr<Texture2D> albedo_texture() { return m_albedo_texture_info.array_index == -1 ? nullptr : m_textures[m_albedo_texture_info.array_index]; }
    inline std::shared_ptr<Texture2D> normal_texture() { return m_normal_texture_info.array_index == -1 ? nullptr : m_textures[m_normal_texture_info.array_index]; }
    inline std::shared_ptr<Texture2D> metallic_texture() { return m_metallic_texture_info.array_index == -1 ? nullptr : m_textures[m_metallic_texture_info.array_index]; }
    inline std::shared_ptr<Texture2D> roughness_texture() { return m_roughness_texture_info.array_index == -1 ? nullptr : m_textures[m_roughness_texture_info.array_index]; }
    inline std::shared_ptr<Texture2D> emissive_texture() { return m_emissive_texture_info.array_index == -1 ? nullptr : m_textures[m_emissive_texture_info.array_index]; }
    inline TextureInfo                albedo_texture_info() { return m_albedo_texture_info; }
    inline TextureInfo                normal_texture_info() { return m_normal_texture_info; }
    inline TextureInfo                metallic_texture_info() { return m_metallic_texture_info; }
    inline TextureInfo                roughness_texture_info() { return m_roughness_texture_info; }
    inline TextureInfo                emissive_texture_info() { return m_emissive_texture_info; }
    inline glm::vec4                  albedo_value() { return m_albedo_value; }
    inline glm::vec4                  emissive_value() { return m_emissive_value; }
    inline float                      metallic_value() { return m_metallic_value; }
    inline float                      roughness_value() { return m_roughness_value; }
    inline uint32_t                   id() { return m_id; }

private:
    Material(MaterialType                            type,
             std::vector<std::shared_ptr<Texture2D>> textures,
             TextureInfo                             albedo_texture_info,
             TextureInfo                             normal_texture_info,
             TextureInfo                             metallic_texture_info,
             TextureInfo                             roughness_texture_info,
             TextureInfo                             emissive_texture_info,
             glm::vec4                               albedo_value    = glm::vec4(0.0f),
             glm::vec4                               emissive_value  = glm::vec4(0.0f),
             float                                   metallic_value  = 0.0f,
             float                                   roughness_value = 0.0f,
             bool                                    alpha_test      = false);
};
} // namespace lumen