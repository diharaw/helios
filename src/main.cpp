#include <iostream>
#include "scene.h"

int main()
{
	lumen::Scene scene;

	auto m = lumen::Mesh::create("assets/cornell_box.ast");

	scene.add_mesh(m, glm::mat4(1.0f));

	int w = 800;
	int h = 600;

	for (int y = 0; y < h; y++)
	{
		for (int x = 0; x < w; x++)
		{
			for (auto& tri : scene.m_triangles)
			{
				lumen::Vertex v0 = scene.m_vertices[tri.v0];
				lumen::Vertex v1 = scene.m_vertices[tri.v1];
				lumen::Vertex v2 = scene.m_vertices[tri.v2];


			}
		}
	}


	return 0;
}