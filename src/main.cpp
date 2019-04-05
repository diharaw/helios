#include <iostream>
#include "scene.h"
#include "ray.h"
#include "intersect.h"
#include "material.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

int main()
{
	int w = 400;
	int h = 400;

	lumen::Scene scene;
	lumen::Camera camera;

	auto m = lumen::Mesh::create("assets/cornell_box.ast");

	scene.add_mesh(m, glm::mat4(1.0f));

	camera.set_projection(60.0f, float(w) / float(h), 0.1f, 1000.0f);
	camera.set_orientation(glm::vec3(0.0f, 0.0f, 50.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	camera.update();

	{
		glm::vec3 v0 = glm::vec3(-100.0f, 0.0f, 0.0f);
		glm::vec3 v1 = glm::vec3(0.0f, 100.0f, 0.0f);
		glm::vec3 v2 = glm::vec3(100.0f, 0.0f, 0.0f);
		glm::vec3 middle = (v0 + v1 + v2) / 3.0f;

		lumen::Ray ray;

		ray.origin = glm::vec3(0.0f, 0.0f, 50.0f);
		ray.dir = glm::normalize(middle - ray.origin);

		float u, v, t;

		if (lumen::ray_triangle(v2, v1, v0, ray, u, v, t))
			std::cout << "HIT" << std::endl;
		else
			std::cout << "NOT HIT" << std::endl;
	}

	struct Pixel
	{
		uint8_t r;
		uint8_t g;
		uint8_t b;
	};

	std::vector<Pixel> framebuffer;

	framebuffer.resize(w * h);

	for (int y = 0; y < h; y++)
	{
		for (int x = 0; x < w; x++)
		{
			glm::vec3 pixel = glm::vec3(0.0f);
			
			lumen::Ray ray = lumen::Ray::compute(x, y, 0.1f, FLT_MAX, camera);

			for (auto& tri : scene.m_triangles)
			{
				lumen::Vertex v0 = scene.m_vertices[tri.v0];
				lumen::Vertex v1 = scene.m_vertices[tri.v1];
				lumen::Vertex v2 = scene.m_vertices[tri.v2];

				float u, v, t;

				if (lumen::ray_triangle(v0.position, v1.position, v2.position, ray, u, v, t))
				{
					pixel = scene.m_materials[tri.mat_id]->albedo;
					break;
				}
			}

			pixel = glm::pow(pixel / (glm::vec3(1.0f) + pixel), glm::vec3(1.0f / 2.2f));

			Pixel p;

			p.r = pixel.x * 255;
			p.g = pixel.y * 255;
			p.b = pixel.z * 255;

			framebuffer[w * y + x] = p;
		}
	}

	stbi_write_tga("out.tga", w, h, 3, framebuffer.data());

	return 0;
}