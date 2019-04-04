#include "scene.h"

namespace lumen
{
	std::shared_ptr<Scene> Scene::create(const std::string& path)
	{
		return nullptr;
	}
		
	uint32_t Scene::add_mesh(const std::shared_ptr<Mesh> mesh, const glm::mat4& transform)
	{
		if (mesh)
		{
			uint32_t current_vertex_count = m_vertices.size();
			uint32_t current_material_count = m_materials.size();

			m_instances.push_back({ mesh, transform });

			for (auto v : mesh->m_vertices)
			{
				glm::vec4 t_p = transform * glm::vec4(v.position, 1.0f);
				glm::vec3 t_n = glm::mat3(transform) * v.normal;

				m_vertices.push_back({ t_p, t_n, v.uv });
			}

			for (auto& submesh : mesh->m_sub_meshes)
			{
				m_materials.push_back(mesh->m_materials[submesh.material_index]);
				uint32_t mat_id = submesh.material_index + current_material_count;

				for (uint32_t i = submesh.base_index; i < submesh.index_count; i += 3)
				{
					uint32_t i0 = submesh.base_vertex + mesh->m_indices[i] + current_vertex_count;
					uint32_t i1 = submesh.base_vertex + mesh->m_indices[i + 1] + current_vertex_count;
					uint32_t i2 = submesh.base_vertex + mesh->m_indices[i + 2] + current_vertex_count;
		
					m_triangles.push_back({ i0, i1, i2, mat_id });
				}
			}

			return 1;
		}
		else
			return 0;
	}
		
	void Scene::set_transform(const uint32_t& id, const glm::mat4& transform)
	{

	}
		
	void Scene::remove_mesh(const uint32_t& id)
	{

	}

	void Scene::build()
	{
		std::vector<MeshInstance> instances = m_instances;

		m_instances.clear();
		m_vertices.clear();
		m_triangles.clear();
		m_materials.clear();

		for (auto& instance : instances)
			add_mesh(instance.mesh, instance.transform);
	}
}