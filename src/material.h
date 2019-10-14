#pragma once

#include <glm.hpp>
#include <memory>
#include <string>
#include <unordered_map>

namespace lumen
{
class Material
{
public:
    static std::shared_ptr<Material> create(const std::string& path);

	std::string name;
    glm::vec3 albedo       = glm::vec3(1.0f);
    glm::vec3 emissive     = glm::vec3(0.0f);
    float     shininess    = 0.0f;
    float     reflectivity = 0.0f;

private:
    static std::unordered_map<std::string, std::weak_ptr<Material>> m_cache;
};
} // namespace lumen