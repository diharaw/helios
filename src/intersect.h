#pragma once

#include "ray.h"
#include <climits>

namespace lumen
{
#define kEpsilon 1e-8
#define CULLING

	inline bool ray_triangle(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const Ray& ray, float& u, float& v, float& t)
	{
		glm::vec3 v0v1 = v1 - v0; 
		glm::vec3 v0v2 = v2 - v0; 
		glm::vec3 pvec = glm::cross(ray.dir, v0v2); 
		float det = glm::dot(v0v1, pvec); 
	#ifdef CULLING 
		// if the determinant is negative the triangle is backfacing
		// if the determinant is close to 0, the ray misses the triangle
		if (det < kEpsilon) return false; 
	#else 
		// ray and triangle are parallel if det is close to 0
		if (fabs(det) < kEpsilon) return false; 
	#endif 
		float invDet = 1 / det; 
 
		glm::vec3 tvec = ray.origin - v0; 
		u = glm::dot(tvec, pvec) * invDet; 
		if (u < 0 || u > 1) return false; 
 
		glm::vec3 qvec = glm::cross(tvec, v0v1); 
		v = glm::dot(ray.dir, qvec) * invDet; 
		if (v < 0 || u + v > 1) return false; 
 
		t = glm::dot(v0v2, qvec) * invDet; 
 
		return true; 
	}
}