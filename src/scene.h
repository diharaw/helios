#pragma once

#include "mesh.h"

namespace lumen
{
	class Scene
	{
	public:
		static std::shared_ptr<Scene> create(const std::string& path);
		uint32_t add_mesh(const std::shared_ptr<Mesh> mesh, const glm::mat4& transform);
		void     set_transform(const uint32_t& id, const glm::mat4& transform);
		void     remove_mesh(const uint32_t& id);
		void	 build();

		struct MeshInstance
		{
			std::shared_ptr<Mesh> mesh;
			glm::mat4			  transform;
		};

		std::vector<MeshInstance>			   m_instances;
		std::vector<Vertex>					   m_vertices;
		std::vector<Triangle>				   m_triangles;
		std::vector<std::shared_ptr<Material>> m_materials;
	};
}