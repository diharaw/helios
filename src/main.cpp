#include "material.h"
#include "geometry.h"
#include "scene.h"
#include "bvh.h"
#include <iostream>
#include <random>
#include <algorithm>

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

std::default_random_engine            generator;
std::uniform_real_distribution<float> distribution(0.0f, 1.0f);

glm::vec3 random_in_unit_sphere()
{
    float z   = distribution(generator) * 2.0f - 1.0f;
    float t   = distribution(generator) * 2.0f * 3.1415926f;
    float r   = sqrt(std::max(0.0f, 1.0f - z * z));
    float x   = r * cos(t);
    float y   = r * sin(t);
    glm::vec3  res = glm::vec3(x, y, z);
    res *= pow(distribution(generator), 1.0f / 3.0f);
    return res;
}


int main()
{
    int w = 400;
    int h = 400;

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
    int   misses = 0;

    for (int j = 0; j < h; j++)
    {
        for (int i = 0; i < w; i++)
        {
            glm::vec3 accumulate = glm::vec3(0.0f);

			for (int sample = 0; sample < 200; sample++)
			{
				glm::vec3 pixel = glm::vec3(1.0f);
				
				lumen::Ray ray = lumen::Ray::compute(i / float(w), 1.0f - (j / float(h)), 0.1f, FLT_MAX, camera);
				
				for (int bounce = 0; bounce < 5; bounce++)
				{
				    lumen::RayResult result;
				
				    scene.m_bvh->trace(ray, result, true);
				
				    if (result.hit())
				    {
				        std::shared_ptr<lumen::Material> mat = scene.m_materials[scene.m_triangles[result.id].w];
				
						//pixel = mat->albedo * glm::dot(result.normal, glm::normalize(glm::vec3(0.0, 1.0, 0.0)));
				        if (mat->is_light())
				        {
				            pixel *= mat->emissive;
				            break;
				        }
				        else
				        {
				            pixel *= mat->albedo; // * glm::dot(result.normal, -ray.dir);
				            ray.origin = result.position;
				            ray.dir    = random_in_unit_sphere();
				        }
				    }
				    else
				    {
						//misses++;
				        pixel *= 0.0f;
				        break;
				    }
				}

				accumulate += pixel;
			}

			accumulate /= 200.0f;
			accumulate = glm::pow(accumulate / (glm::vec3(1.0f) + accumulate), glm::vec3(1.0f / 2.2f));

            Pixel p;

            p.r = accumulate.x * 255;
            p.g = accumulate.y * 255;
            p.b = accumulate.z * 255;

            framebuffer[w * j + i] = p;
        }
    }

    stbi_write_tga("out.tga", w, h, 3, framebuffer.data());

    return 0;
}