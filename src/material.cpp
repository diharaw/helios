#include "material.h"
#include <runtime/loader.h>

namespace lumen
{
std::unordered_map<std::string, std::weak_ptr<Material>> Material::m_cache;

std::shared_ptr<Material> Material::create(const std::string& path)
{
    if (m_cache.find(path) != m_cache.end() && m_cache[path].lock())
        return m_cache[path].lock();
    else
    {
        ast::Material ast_material;

        if (ast::load_material(path, ast_material))
        {
            std::shared_ptr<Material> material = std::make_shared<Material>();

            material->name = ast_material.name;

            for (auto& prop : ast_material.properties)
            {
                if (prop.type == ast::PROPERTY_ALBEDO)
                    material->albedo = { prop.vec3_value[0], prop.vec3_value[1], prop.vec3_value[2] };
                if (prop.type == ast::PROPERTY_EMISSIVE)
                    material->emissive = { prop.vec3_value[0], prop.vec3_value[1], prop.vec3_value[2] };
                if (prop.type == ast::PROPERTY_SHININESS)
                    material->shininess = prop.float_value;
                if (prop.type == ast::PROPERTY_REFLECTIVITY)
                    material->reflectivity = prop.float_value;
            }

            m_cache[path] = material;

            return material;
        }
        else
            return nullptr;
    }
}

bool Material::is_light()
{
    return emissive.x > 0.0f && emissive.y > 0.0f && emissive.z > 0.0f;
}
} // namespace lumen