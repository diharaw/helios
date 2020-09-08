#include "brdf.h"
#include "sampling.h"
#define _USE_MATH_DEFINES
#include <math.h>

namespace lumen
{
LambertBRDF::LambertBRDF(glm::vec3 normal, glm::vec3 albedo) :
    m_normal(normal), m_albedo(albedo)
{
}

glm::vec3 LambertBRDF::sample(glm::vec3& l, const glm::vec3& v)
{
    l = sample_cosine_lobe_direction(m_normal, glm::vec2(rand(), rand()));

    return m_albedo;
}

glm::vec3 LambertBRDF::evaluate(const glm::vec3& l, const glm::vec3& v)
{
    float ndotl = glm::clamp(glm::dot(m_normal, l), 0.0f, 1.0f);

    return m_albedo * ndotl / float(M_PI);
}

glm::vec3 MicrofacetBRDF::sample(glm::vec3& l, const glm::vec3& v) 
{
    return glm::vec3(0.0f);
}

glm::vec3 MicrofacetBRDF::evaluate(const glm::vec3& l, const glm::vec3& v)
{
    return glm::vec3(0.0f);
}

glm::vec3 UberBRDF::sample(glm::vec3& l, const glm::vec3& v)
{
    return glm::vec3(0.0f);
}

glm::vec3 UberBRDF::evaluate(const glm::vec3& l, const glm::vec3& v)
{
    return glm::vec3(0.0f);
}
} // namespace lumen