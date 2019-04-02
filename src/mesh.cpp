#include "mesh.h"
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



				m_cache[path] = mesh;

				return mesh;
			}
		}
	}
}