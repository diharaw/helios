#pragma once

#include "camera.h"
#include <glm.hpp>

namespace lumen
{
struct Ray
{
    glm::vec3 origin;
    glm::vec3 dir;
    float     tmin;
    float     tmax;

    static Ray compute(float x, float y, float tmin, float tmax, const Camera& camera);
};

struct RayResult
{

};
} // namespace lumen