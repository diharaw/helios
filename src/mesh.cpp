#include "mesh.h"
#include "material.h"
#include <runtime/loader.h>

namespace lumen
{
	std::unordered_map<std::string, std::weak_ptr<Mesh>> Mesh::m_cache;

	std::shared_ptr<Mesh> Mesh::create(const std::string& path)
	{
		if (m_cache.find(path) != m_cache.end() && m_cache[path].lock())
			return m_cache[path].lock();
		else
		{
			ast::Mesh ast_mesh;

			if (ast::load_mesh(path, ast_mesh))
			{
				std::shared_ptr<Mesh> mesh = std::make_shared<Mesh>();

				mesh->m_vertices.reserve(ast_mesh.vertices.size());

				for (auto& v : ast_mesh.vertices)
					mesh->m_vertices.push_back({ v.position, v.normal, v.tex_coord });

				mesh->m_indices.reserve(ast_mesh.indices.size());

				for (auto& i : ast_mesh.indices)
					mesh->m_indices.push_back(i);

				mesh->m_sub_meshes.reserve(ast_mesh.submeshes.size());

				for (auto& m : ast_mesh.submeshes)
					mesh->m_sub_meshes.push_back({ m.material_index, m.index_count, m.base_vertex, m.base_index });

				mesh->m_materials.reserve(ast_mesh.material_paths.size());

				for (auto& p : ast_mesh.material_paths)
					mesh->m_materials.push_back(Material::create(p));

				m_cache[path] = mesh;

				return mesh;
			}
		}
	}
}