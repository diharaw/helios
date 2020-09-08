#include "sampling.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include <random>

namespace lumen
{
static std::default_random_engine            generator;
static std::uniform_real_distribution<float> distribution(0.0f, 0.9999999f);

float rand()
{
    return distribution(generator);
}

glm::mat3 make_rotation_matrix(glm::vec3 z)
{
    const glm::vec3 ref = glm::abs(glm::dot(z, glm::vec3(0, 1, 0))) > 0.99f ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);

    const glm::vec3 x = glm::normalize(glm::cross(ref, z));
    const glm::vec3 y = glm::cross(z, x);

    //assert(!is_nan(x));
    //assert(!is_nan(y));
    //assert(!is_nan(z));

    return { x, y, z };
}

glm::vec3 random_in_unit_sphere()
{
    float     z   = distribution(generator) * 2.0f - 1.0f;
    float     t   = distribution(generator) * 2.0f * 3.1415926f;
    float     r   = sqrt(std::max(0.0f, 1.0f - z * z));
    float     x   = r * cos(t);
    float     y   = r * sin(t);
    glm::vec3 res = glm::vec3(x, y, z);
    res *= pow(distribution(generator), 1.0f / 3.0f);
    return res;
}

glm::vec3 sample_cosine_lobe_direction(glm::vec3 n, glm::vec2 rand)
{
    glm::vec2 sample = glm::max(glm::vec2(0.00001f), rand);

    const float phi = 2.0f * M_PI * sample.y;

    const float cos_theta = sqrt(sample.x);
    const float sin_theta = sqrt(1 - sample.x);

    glm::vec3 t = glm::vec3(sin_theta * cos(phi), sin_theta * sin(phi), cos_theta);

    //assert(!is_nan(t));

    return glm::normalize(make_rotation_matrix(n) * t);
}
} // namespace lumen