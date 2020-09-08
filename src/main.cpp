#include "material.h"
#include "geometry.h"
#include "scene.h"
#include "bvh.h"
#include "sampling.h"
#include <iostream>
#include <random>
#include <algorithm>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#define MAX_BOUNCES 10
#define MAX_SAMPLES 100

int main()
{
    int w = 256;
    int h = 256;

    lumen::Scene  scene;
    lumen::Camera camera;

    auto m = lumen::Mesh::create("assets/mesh/cornell_box.ast");

    scene.add_mesh(m, glm::mat4(1.0f));

    scene.build();

    camera.set_projection(40.0f, float(w) / float(h), 0.1f, 1000.0f);
    camera.set_orientation(glm::vec3(0.0f, 1.0f, 3.7f),
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

    bool debug_normals = false;
    bool debug_albedo  = false;

#pragma omp parallel for
    for (int j = 0; j < h; j++)
    {
        for (int i = 0; i < w; i++)
        {
            glm::vec3 accumulate  = glm::vec3(0.0f);
            
            for (int sample = 0; sample < MAX_SAMPLES; sample++)
            {
                glm::vec3 color = glm::vec3(0.0f);
                glm::vec3 attenuation = glm::vec3(1.0f);

                float u = float(i + lumen::rand()) / float(w);
                float v = float(j + lumen::rand()) / float(h);

                lumen::Ray ray = lumen::Ray::compute(u, 1.0f - v, 0.0001f, FLT_MAX, camera);

                for (int bounce = 0; bounce < MAX_BOUNCES; bounce++)
                {
                    lumen::RayResult result;

                    scene.m_bvh->trace(ray, result, true);

                    if (!result.hit())
                    {
                        color = glm::vec3(0.0f);
                        break;
                    }

                    std::shared_ptr<lumen::Material> mat = scene.m_materials[result.id];

                    if (mat->is_emissive())
                    {
                        if (bounce == 0)
                            color = mat->emissive;
                        else
                            color += mat->emissive * attenuation;

                        break;
                    }
                    else
                    {
                        lumen::LambertBRDF brdf = mat->create_brdf(result.normal);

                        glm::vec3 next_dir;

                        glm::vec3 weight = brdf.sample(next_dir, -ray.dir);

                        attenuation *= weight;

                        ray.origin = result.position;
                        ray.dir    = next_dir;
                    }
                }

                accumulate += color;
            }

            accumulate /= float(MAX_SAMPLES);
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