#include "brdf.h"

namespace lumen
{
glm::vec3 LambertBRDF::sample(glm::vec3& l, const glm::vec3& v, glm::vec2& rand_sample)
{

}

glm::vec3 LambertBRDF::evaluate(const glm::vec3& l, const glm::vec3& v)
{
}

glm::vec3 MicrofacetBRDF::sample(glm::vec3& l, const glm::vec3& v, glm::vec2& rand_sample)
{
}

glm::vec3 MicrofacetBRDF::evaluate(const glm::vec3& l, const glm::vec3& v)
{
}

glm::vec3 UberBRDF::sample(glm::vec3& l, const glm::vec3& v, glm::vec2& rand_sample)
{
}

glm::vec3 UberBRDF::evaluate(const glm::vec3& l, const glm::vec3& v)
{
}
} // namespace lumen