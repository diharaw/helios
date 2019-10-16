#include "material.h"
#include "geometry.h"
#include "scene.h"
#include "bvh.h"
#include <iostream>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

class BRDF
{

};

class LambertBRDF : public BRDF
{

};

class MicrofacetBRDF : public BRDF
{

};

int main()
{
    int w = 1024;
    int h = 1024;

    lumen::Scene  scene;
    lumen::Camera camera;

    auto m = lumen::Mesh::create("assets/mesh/cornell_box.ast");

    scene.add_mesh(m, glm::mat4(1.0f));

    scene.build();

    camera.set_projection(60.0f, float(w) / float(h), 0.1f, 1000.0f);
    camera.set_orientation(glm::vec3(0.0f, 1.0f, 2.5f),
                           glm::vec3(0.0f, 1.0f, 0.0f),
                           glm::vec3(0.0f, 1.0f, 0.0f));
    camera.update();

    struct Pixel
    {
        uint8_t r;
        uint8_t g;
        uint8_t b;
    };

    std::vector<Pixel> framebuffer;

    framebuffer.resize(w * h);

    float scale = 1.0f;

    for (int j = 0; j < h; j++)
    {
        for (int i = 0; i < w; i++)
        {
            glm::vec3 pixel = glm::vec3(0.0f);

            lumen::Ray ray = lumen::Ray::compute(i / float(w), 1.0f - (j / float(h)), 0.1f, FLT_MAX, camera);

			for (int bounce = 0; bounce < 15; i++)
			{
				lumen::RayResult result;
				
				scene.m_bvh->trace(ray, result, true);

				if (result.hit())
				{
					std::shared_ptr<lumen::Material> mat = scene.m_materials[scene.m_triangles[result.id].w];

					if (mat->is_light())
					{
						pixel *= mat->emissive;
						break;
					}
					else
					{
						pixel *= mat->albedo * glm::dot(result.normal, -ray.dir);
						ray.origin = result.position;
						//ray.dir = 
					}
				}
				else
					break;
			}

			pixel = glm::pow(pixel / (glm::vec3(1.0f) + pixel), glm::vec3(1.0f / 2.2f));

            Pixel p;

            p.r = pixel.x * 255;
            p.g = pixel.y * 255;
            p.b = pixel.z * 255;

            framebuffer[w * j + i] = p;
        }
    }

    stbi_write_tga("out.tga", w, h, 3, framebuffer.data());

    return 0;
}