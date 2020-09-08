#pragma once

#include <glm.hpp>

namespace lumen
{
class BRDF
{
public:
    virtual glm::vec3 sample(glm::vec3& l, const glm::vec3& v) = 0;
    virtual glm::vec3 evaluate(const glm::vec3& l, const glm::vec3& v)               = 0;
};

class LambertBRDF : public BRDF
{
public:
    LambertBRDF(glm::vec3 normal, glm::vec3 albedo);
    glm::vec3 sample(glm::vec3& l, const glm::vec3& v) override;
    glm::vec3 evaluate(const glm::vec3& l, const glm::vec3& v) override;

private:
    glm::vec3 m_normal;
    glm::vec3 m_albedo;
};

class MicrofacetBRDF : public BRDF
{
public:
    glm::vec3 sample(glm::vec3& l, const glm::vec3& v) override;
    glm::vec3 evaluate(const glm::vec3& l, const glm::vec3& v) override;
};

class UberBRDF : public BRDF
{
public:
    glm::vec3 sample(glm::vec3& l, const glm::vec3& v) override;
    glm::vec3 evaluate(const glm::vec3& l, const glm::vec3& v) override;
};
} // namespace lumen