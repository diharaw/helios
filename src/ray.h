#pragma once

#include <glm.hpp>
#include "camera.h"

namespace lumen
{
	struct Ray
	{
		glm::vec3 origin;
		glm::vec3 dir;
		float tmin;
		float tmax;

		static Ray compute(float x, float y, float tmin, float tmax, const Camera& camera);
	};
}