#pragma once

#include <glm.hpp>

namespace lumen
{
extern float     rand();
extern glm::vec3 random_in_unit_sphere();
extern glm::vec3 sample_cosine_lobe_direction(glm::vec3 n, glm::vec2 rand);
} // namespace lumen