#pragma once

#include <glm.hpp>

namespace lumen
{
class BRDF
{
public:
    virtual glm::vec3 sample(glm::vec3& l, const glm::vec3& v, glm::vec2& rand_sample) = 0;
    virtual glm::vec3 evaluate(const glm::vec3& l, const glm::vec3& v)               = 0;
};

class LambertBRDF : public BRDF
{
public:
    glm::vec3 sample(glm::vec3& l, const glm::vec3& v, glm::vec2& rand_sample) override;
    glm::vec3 evaluate(const glm::vec3& l, const glm::vec3& v) override;
};

class MicrofacetBRDF : public BRDF
{
public:
    glm::vec3 sample(glm::vec3& l, const glm::vec3& v, glm::vec2& rand_sample) override;
    glm::vec3 evaluate(const glm::vec3& l, const glm::vec3& v) override;
};

class UberBRDF : public BRDF
{
public:
    glm::vec3 sample(glm::vec3& l, const glm::vec3& v, glm::vec2& rand_sample) override;
    glm::vec3 evaluate(const glm::vec3& l, const glm::vec3& v) override;
};
} // namespace lumen