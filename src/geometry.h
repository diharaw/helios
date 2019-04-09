#pragma once

#include "camera.h"
#include <glm.hpp>

namespace lumen
{
inline float vmin(const glm::vec3& v)
{
    const float* tp = &v[0];
    float        r  = tp[0];
    for (int i = 1; i < 3; i++) r = glm::min(r, tp[i]);
    return r;
}
inline float vmax(const glm::vec3& v)
{
    const float* tp = &v[0];
    float        r  = tp[0];
    for (int i = 1; i < 3; i++) r = glm::max(r, tp[i]);
    return r;
}
inline glm::vec3 vmin(const glm::vec3& v1, const glm::vec3& v2)
{
    const float* tp1 = &v1[0];
    const float* tp2 = &v2[0];
    glm::vec3    r;
    for (int i = 0; i < 3; i++) r[i] = glm::min(tp1[i], tp2[i]);
    return r;
}
inline glm::vec3 vmax(const glm::vec3& v1, const glm::vec3& v2)
{
    const float* tp1 = &v1[0];
    const float* tp2 = &v2[0];
    glm::vec3    r;
    for (int i = 0; i < 3; i++) r[i] = glm::max(tp1[i], tp2[i]);
    return r;
}

class AABB
{
public:
    AABB(void) :
        m_mn(FLT_MAX, FLT_MAX, FLT_MAX), m_mx(-FLT_MAX, -FLT_MAX, -FLT_MAX) {}
    AABB(const glm::vec3& mn, const glm::vec3& mx) :
        m_mn(mn), m_mx(mx) {}

    inline void grow(const glm::vec3& pt)
    {
        m_mn = vmin(m_mn, pt);
        m_mx = vmax(m_mx, pt);
    }
    inline void grow(const AABB& aabb)
    {
        grow(aabb.m_mn);
        grow(aabb.m_mx);
    }
    inline void intersect(const AABB& aabb)
    {
        m_mn = vmax(m_mn, aabb.m_mn);
        m_mx = vmin(m_mx, aabb.m_mx);
    }
    inline float volume(void) const
    {
        if (!valid()) return 0.0f;
        return (m_mx.x - m_mn.x) * (m_mx.y - m_mn.y) * (m_mx.z - m_mn.z);
    }
    inline float area(void) const
    {
        if (!valid()) return 0.0f;
        glm::vec3 d = m_mx - m_mn;
        return (d.x * d.y + d.y * d.z + d.z * d.x) * 2.0f;
    }
    inline bool             valid(void) const { return m_mn.x <= m_mx.x && m_mn.y <= m_mx.y && m_mn.z <= m_mx.z; }
    inline glm::vec3        midPoint(void) const { return (m_mn + m_mx) * 0.5f; }
    inline const glm::vec3& min(void) const { return m_mn; }
    inline const glm::vec3& max(void) const { return m_mx; }
    inline glm::vec3&       min(void) { return m_mn; }
    inline glm::vec3&       max(void) { return m_mx; }
    inline AABB      operator+(const AABB& aabb) const
    {
        AABB u(*this);
        u.grow(aabb);
        return u;
    }

private:
    glm::vec3 m_mn;
    glm::vec3 m_mx;
};

struct Ray
{
    glm::vec3 origin;
    glm::vec3 dir;
    float     tmin;
    float     tmax;

    static Ray compute(float x, float y, float tmin, float tmax, const Camera& camera);
};

#define RAY_NO_HIT (-1)

struct RayResult
{
    RayResult(int32_t ii = RAY_NO_HIT, float ti = 0.f) :
        id(ii), t(ti) {}
    inline bool hit(void) const { return (id != RAY_NO_HIT); }
    inline void clear(void) { id = RAY_NO_HIT; }

    int32_t id;
    float   t;
    int32_t padA;
    int32_t padB;
};

#define kEpsilon 1e-8
#define CULLING

namespace intersect
{
inline glm::vec2 ray_box(const AABB& box, const Ray& ray)
{
    const glm::vec3& orig = ray.origin;
    const glm::vec3& dir  = ray.dir;

    glm::vec3 t0 = (box.min() - orig) / dir;
    glm::vec3 t1 = (box.max() - orig) / dir;

    float tmin = vmax(vmin(t0, t1));
    float tmax = vmin(vmax(t0, t1));

    return glm::vec2(tmin, tmax);
}

inline bool ray_triangle(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const Ray& ray, float& u, float& v, float& t)
{
    glm::vec3 v0v1 = v1 - v0;
    glm::vec3 v0v2 = v2 - v0;
    glm::vec3 pvec = glm::cross(ray.dir, v0v2);
    float     det  = glm::dot(v0v1, pvec);
#ifdef CULLING
    // if the determinant is negative the triangle is backfacing
    // if the determinant is close to 0, the ray misses the triangle
    if (det < kEpsilon)
        return false;
#else
    // ray and triangle are parallel if det is close to 0
    if (fabs(det) < kEpsilon)
        return false;
#endif
    float invDet = 1 / det;

    glm::vec3 tvec = ray.origin - v0;
    u              = glm::dot(tvec, pvec) * invDet;
    if (u < 0 || u > 1)
        return false;

    glm::vec3 qvec = glm::cross(tvec, v0v1);
    v              = glm::dot(ray.dir, qvec) * invDet;
    if (v < 0 || u + v > 1)
        return false;

    t = glm::dot(v0v2, qvec) * invDet;

    return true;
}
} // namespace intersect
} // namespace lumen