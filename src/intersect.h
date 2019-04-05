#pragma once

#include "ray.h"
#include <climits>

namespace lumen
{
	inline bool ray_triangle(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const Ray& ray, float& u, float& v, float& t)
	{
		const float EPSILON = 0.f;

		glm::vec3 edge1 = v1-v0;
		glm::vec3 edge2 = v2-v0;
		glm::vec3 pvec  = glm::cross(ray.dir, edge2);
		float det   = glm::dot(edge1, pvec);

		glm::vec3 tvec = ray.origin - v0;
		u = glm::dot(tvec,pvec);

		glm::vec3 qvec = glm::cross(tvec, edge1);
		v = glm::dot(ray.dir, qvec);

		// TODO: clear this
		if (det > EPSILON)
		{
			if (u < 0.0 || u > det)
				return false;
			if (v < 0.0 || u + v > det)
				return false;
		}
		else if (det < -EPSILON)
		{
			if (u > 0.0 || u < det)
				return false;
			if (v > 0.0 || u + v < det)
				return false;
		}
		else
			return false;

		float inv_det = 1.f / det;
		t = glm::dot(edge2, qvec) * inv_det;
		u *= inv_det;
		v *= inv_det;

		if (t > ray.tmin && t < ray.tmax)
			return true;

		return false;
	}
}