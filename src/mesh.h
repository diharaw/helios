#pragma once

#include <glm.hpp>
#include <string>
#include <unordered_map>
#include <memory>

namespace lumen
{
	class Material;

	struct Vertex
	{
		glm::vec3 position;
		glm::vec3 normal;
		glm::vec2 uv;
		uint32_t  mat_id;
	};

	class Mesh
	{
	public:
		static std::shared_ptr<Mesh> create(const std::string& path);

		std::vector<Vertex>					   m_vertices;
		std::vector<uint32_t>				   m_indices;
		std::vector<std::shared_ptr<Material>> m_materials;

		static std::unordered_map<std::string, std::weak_ptr<Mesh>> m_cache;
	};
}